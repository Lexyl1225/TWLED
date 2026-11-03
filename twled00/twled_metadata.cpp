#include "ota_update.h"
#include "twled.h"
#include "twled_metadata.h"

#ifndef TWLED_VERSION
  #warning TWLED_VERSION was not set - using default value of 'dev'
  #define TWLED_VERSION dev
#endif
#ifndef TWLED_RELEASE_NAME
  #warning TWLED_RELEASE_NAME was not set - using default value of 'Custom'
  #define TWLED_RELEASE_NAME "Custom"
#endif
#ifndef TWLED_REPO
  // No warning for this one: integrators are not always on GitHub
  #define TWLED_REPO "unknown"
#endif

constexpr uint32_t TWLED_CUSTOM_DESC_MAGIC = 0x57535453;  // "WSTS" (TWLED System Tag Structure)
constexpr uint32_t TWLED_CUSTOM_DESC_VERSION = 1;
// Compile-time validation that release name doesn't exceed maximum length
static_assert(sizeof(TWLED_RELEASE_NAME) <= TWLED_RELEASE_NAME_MAX_LEN, 
              "TWLED_RELEASE_NAME exceeds maximum length of TWLED_RELEASE_NAME_MAX_LEN characters");

/**
 * DJB2 hash function (C++11 compatible constexpr)
 * Used for compile-time hash computation to validate structure contents
 * Recursive for compile time: not usable at runtime due to stack depth
 * 
 * Note that this only works on strings; there is no way to produce a compile-time
 * hash of a struct in C++11 without explicitly listing all the struct members.
 * So for now, we hash only the release name.  This suffices for a "did you find 
 * valid structure" check.
 * 
 */
constexpr uint32_t djb2_hash_constexpr(const char* str, uint32_t hash = 5381) {
    return (*str == '\0') ? hash : djb2_hash_constexpr(str + 1, ((hash << 5) + hash) + *str);
}

/**
 * Runtime DJB2 hash function for validation
 */
inline uint32_t djb2_hash_runtime(const char* str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + *str++;
    }
    return hash;
}

// ------------------------------------
// GLOBAL VARIABLES
// ------------------------------------
// Structure instantiation for this build 
const twled_metadata_t __attribute__((section(BUILD_METADATA_SECTION))) TWLED_BUILD_DESCRIPTION = {
    TWLED_CUSTOM_DESC_MAGIC,                   // magic
    TWLED_CUSTOM_DESC_VERSION,                 // version 
    TOSTRING(TWLED_VERSION),
    TWLED_RELEASE_NAME,                        // release_name
    std::integral_constant<uint32_t, djb2_hash_constexpr(TWLED_RELEASE_NAME)>::value, // hash - computed at compile time; integral_constant enforces this
};

static const char repoString_s[] PROGMEM = TWLED_REPO;
const __FlashStringHelper* repoString = FPSTR(repoString_s);

static const char productString_s[] PROGMEM = TWLED_PRODUCT_NAME;
const __FlashStringHelper* productString = FPSTR(productString_s);

static const char brandString_s [] PROGMEM = TWLED_BRAND;
const __FlashStringHelper* brandString = FPSTR(brandString_s);



/**
 * Extract TWLED custom description structure from binary
 * @param binaryData Pointer to binary file data
 * @param dataSize Size of binary data in bytes
 * @param extractedDesc Buffer to store extracted custom description structure
 * @return true if structure was found and extracted, false otherwise
 */
bool findTWledMetadata(const uint8_t* binaryData, size_t dataSize, twled_metadata_t* extractedDesc) {
  if (!binaryData || !extractedDesc || dataSize < sizeof(twled_metadata_t)) {
    return false;
  }

  for (size_t offset = 0; offset <= dataSize - sizeof(twled_metadata_t); offset++) {
    if ((binaryData[offset]) == static_cast<char>(TWLED_CUSTOM_DESC_MAGIC)) {
      // First byte matched; check next in an alignment-safe way
      uint32_t data_magic;
      memcpy(&data_magic, binaryData + offset, sizeof(data_magic));
      
      // Check for magic number
      if (data_magic == TWLED_CUSTOM_DESC_MAGIC) {            
        twled_metadata_t candidate;
        memcpy(&candidate, binaryData + offset, sizeof(candidate));

        // Found potential match, validate version
        if (candidate.desc_version != TWLED_CUSTOM_DESC_VERSION) {
          DEBUG_PRINTF_P(PSTR("Found TWLED structure at offset %u but version mismatch: %u\n"), 
                        offset, candidate.desc_version);
          continue;
        }
        
        // Validate hash using runtime function
        uint32_t expected_hash = djb2_hash_runtime(candidate.release_name);
        if (candidate.hash != expected_hash) {
          DEBUG_PRINTF_P(PSTR("Found TWLED structure at offset %u but hash mismatch\n"), offset);
          continue;
        }
        
        // Valid structure found - copy entire structure
        *extractedDesc = candidate;
        
        DEBUG_PRINTF_P(PSTR("Extracted TWLED structure at offset %u: '%s'\n"), 
                      offset, extractedDesc->release_name);
        return true;
      }
    }
  }
  
  DEBUG_PRINTLN(F("No TWLED custom description found in binary"));
  return false;
}


/**
 * Check if OTA should be allowed based on release compatibility using custom description
 * @param binaryData Pointer to binary file data (not modified)
 * @param dataSize Size of binary data in bytes
 * @param errorMessage Buffer to store error message if validation fails 
 * @param errorMessageLen Maximum length of error message buffer
 * @return true if OTA should proceed, false if it should be blocked
 */

bool shouldAllowOTA(const twled_metadata_t& firmwareDescription, char* errorMessage, size_t errorMessageLen) {
  // Clear error message
  if (errorMessage && errorMessageLen > 0) {
    errorMessage[0] = '\0';
  }

  // Validate compatibility using extracted release name
  // We make a stack copy so we can print it safely
  char safeFirmwareRelease[TWLED_RELEASE_NAME_MAX_LEN];
  strncpy(safeFirmwareRelease, firmwareDescription.release_name, TWLED_RELEASE_NAME_MAX_LEN - 1);
  safeFirmwareRelease[TWLED_RELEASE_NAME_MAX_LEN - 1] = '\0';
  
  if (strlen(safeFirmwareRelease) == 0) {
    return false;
  }  

  if (strncmp_P(safeFirmwareRelease, releaseString, TWLED_RELEASE_NAME_MAX_LEN) != 0) {
    if (errorMessage && errorMessageLen > 0) {
      snprintf_P(errorMessage, errorMessageLen, PSTR("Firmware compatibility mismatch: current='%s', uploaded='%s'."), 
               releaseString, safeFirmwareRelease);
      errorMessage[errorMessageLen - 1] = '\0'; // Ensure null termination
    }
    return false;
  }

  // TODO: additional checks go here

  return true;
}
