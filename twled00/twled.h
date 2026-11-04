#ifndef TWLED_H
#define TWLED_H
/*
   Main sketch, global variable declarations
   @title WLED project sketch
   @author Christian Schwinne
 */

// version code in format yymmddb (b = daily build)
#define VERSION 26110401

//uncomment this if you have a "my_config.h" file you'd like to use
#define TWLED_USE_MY_CONFIG

// ESP8266-01 (blue) got too little storage space to work with WLED. 0.10.2 is the last release supporting this unit.

// ESP8266-01 (black) has 1MB flash and can thus fit the whole program, although OTA update is not possible. Use 1M(128K SPIFFS).
// 2-step OTA may still be possible: https://github.com/wled-dev/WLED/issues/2040#issuecomment-981111096
// Uncomment some of the following lines to disable features:
// Alternatively, with platformio pass your chosen flags to your custom build target in platformio_override.ini

// You are required to disable over-the-air updates:
//#define TWLED_DISABLE_OTA         // saves 14kb
#ifdef TWLED_ENABLE_AOTA
  #if defined(TWLED_DISABLE_OTA)
    #warning TWLED_DISABLE_OTA was defined but it will be ignored due to TWLED_ENABLE_AOTA.
  #endif
  #undef TWLED_DISABLE_OTA
#endif

// You can choose some of these features to disable:
#define TWLED_DISABLE_ALEXA       // saves 11kb
//#define TWLED_DISABLE_HUESYNC     // saves 4kb
#define TWLED_DISABLE_INFRARED    // saves 12kb, there is no pin left for this on ESP8266-01
#ifndef TWLED_DISABLE_MQTT
  #define TWLED_ENABLE_MQTT         // saves 12kb
#endif
#ifndef TWLED_DISABLE_ADALIGHT      // can be used to disable reading commands from serial RX pin (see issue #3128).
  #define TWLED_ENABLE_ADALIGHT     // disable saves 5Kb (uses GPIO3 (RX) for serial). Related serial protocols: Adalight/TPM2, Improv, Serial JSON, Continuous Serial Streaming
#else
  #undef TWLED_ENABLE_ADALIGHT      // disable has priority over enable
#endif
//#define TWLED_ENABLE_DMX          // uses 3.5kb
#ifndef TWLED_DISABLE_LOXONE
  #define TWLED_ENABLE_LOXONE       // uses 1.2kb
#endif
#ifndef TWLED_DISABLE_WEBSOCKETS
  #define TWLED_ENABLE_WEBSOCKETS
#else
  #define TWLED_ENABLE_JSONLIVE     // peek LED output via /json/live (WS binary peek is always enabled)
#endif

//#define TWLED_DISABLE_ESPNOW      // Removes dependence on esp now

#define TWLED_ENABLE_FS_EDITOR      // enable /edit page for editing FS content. Will also be disabled with OTA lock

// to toggle usb serial debug (un)comment the following line
//#define TWLED_DEBUG

// filesystem specific debugging
//#define TWLED_DEBUG_FS

#ifndef TWLED_WATCHDOG_TIMEOUT
  // 3 seconds should be enough to detect a lockup
  // define TWLED_WATCHDOG_TIMEOUT=0 to disable watchdog, default
  #define TWLED_WATCHDOG_TIMEOUT 0
#endif

//optionally disable brownout detector on ESP32.
//This is generally a terrible idea, but improves boot success on boards with a 3.3v regulator + cap setup that can't provide 400mA peaks
//#define TWLED_DISABLE_BROWNOUT_DET

#include <cstddef>
#include <vector>

// Library inclusions.
#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
  #include <ESPAsyncTCP.h>
  #include <LittleFS.h>
  extern "C"
  {
  #include <user_interface.h>
  }
  #ifndef TWLED_DISABLE_ESPNOW
    #include <espnow.h>
    #define WIFI_MODE_STA WIFI_STA
    #define WIFI_MODE_AP WIFI_AP
    #include <QuickEspNow.h>
  #endif
#else // ESP32
  #include <HardwareSerial.h>  // ensure we have the correct "Serial" on new MCUs (depends on ARDUINO_USB_MODE and ARDUINO_USB_CDC_ON_BOOT)
  #include <WiFi.h>
  #include <ETH.h>
  #include "esp_wifi.h"
  #include <ESPmDNS.h>
  #include <AsyncTCP.h>
  #if LOROL_LITTLEFS
    #ifndef CONFIG_LITTLEFS_FOR_IDF_3_2
      #define CONFIG_LITTLEFS_FOR_IDF_3_2
    #endif
    #include <LITTLEFS.h>
  #else
    #include <LittleFS.h>
  #endif
  #include "esp_task_wdt.h"

  #ifndef TWLED_DISABLE_ESPNOW
    #include <esp_now.h>
    #include <QuickEspNow.h>
  #endif
#endif
#include <Wire.h>
#include <SPI.h>

#include "src/dependencies/network/Network.h"

#ifdef TWLED_USE_MY_CONFIG
  #include "my_config.h"
#endif

#include <ESPAsyncWebServer.h>
#ifdef TWLED_ADD_EEPROM_SUPPORT
  #include <EEPROM.h>
#endif
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <SPIFFSEditor.h>
#include "src/dependencies/time/TimeLib.h"
#include "src/dependencies/timezone/Timezone.h"
#include "src/dependencies/toki/Toki.h"

#ifndef TWLED_DISABLE_ALEXA
  #define ESPALEXA_ASYNC
  #define ESPALEXA_NO_SUBPAGE
  #define ESPALEXA_MAXDEVICES 10
  // #define ESPALEXA_DEBUG
  #include "src/dependencies/espalexa/Espalexa.h"
  #include "src/dependencies/espalexa/EspalexaDevice.h"
#endif

#ifdef TWLED_ENABLE_DMX
 #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  #include "src/dependencies/dmx/ESPDMX.h"
 #else //ESP32
  #include "src/dependencies/dmx/SparkFunDMX.h"
 #endif
#endif

#ifdef TWLED_ENABLE_DMX_INPUT
  #include "dmx_input.h"
#endif

#include "src/dependencies/e131/ESPAsyncE131.h"
#ifndef TWLED_DISABLE_MQTT
#include <AsyncMqttClient.h>
#endif

#define ARDUINOJSON_DECODE_UNICODE 0
#include "src/dependencies/json/AsyncJson-v6.h"
#include "src/dependencies/json/ArduinoJson-v6.h"

// ESP32-WROVER features SPI RAM (aka PSRAM) which can be allocated using ps_malloc()
// we can create custom PSRAMDynamicJsonDocument to use such feature (replacing DynamicJsonDocument)
// The following is a construct to enable code to compile without it.
// There is a code that will still not use PSRAM though:
//    AsyncJsonResponse is a derived class that implements DynamicJsonDocument (AsyncJson-v6.h)
#if defined(BOARD_HAS_PSRAM)
struct PSRAM_Allocator {
  void* allocate(size_t size) {
    return ps_malloc(size); // use PSRAM
  }
  void* reallocate(void* ptr, size_t new_size) {
    return ps_realloc(ptr, new_size); // use PSRAM
  }
  void deallocate(void* pointer) {
    free(pointer);
  }
};
using PSRAMDynamicJsonDocument = BasicJsonDocument<PSRAM_Allocator>;
#else
#define PSRAMDynamicJsonDocument DynamicJsonDocument
#endif

#define FASTLED_INTERNAL //remove annoying pragma messages
#define USE_GET_MILLISECOND_TIMER
#include "FastLED.h"
#include "const.h"
#include "fcn_declare.h"
#ifndef TWLED_DISABLE_OTA
  #include "ota_update.h"
#endif
#include "NodeStruct.h"
#include "pin_manager.h"
#include "colors.h"
#include "bus_manager.h"
#include "FX.h"
#include "twled_metadata.h"

#ifndef CLIENT_SSID
  #define CLIENT_SSID DEFAULT_CLIENT_SSID
#endif

#ifndef CLIENT_PASS
  #define CLIENT_PASS ""
#endif

#ifndef MDNS_NAME
  #define MDNS_NAME DEFAULT_MDNS_NAME
#endif

#if defined(TWLED_AP_PASS) && !defined(TWLED_AP_SSID)
  #error TWLED_AP_PASS is defined but TWLED_AP_SSID is still the default. \
         Please change TWLED_AP_SSID to something unique.
#endif

#ifndef TWLED_AP_SSID
  #define TWLED_AP_SSID DEFAULT_AP_SSID
#endif

#ifndef TWLED_AP_PASS
  #define TWLED_AP_PASS DEFAULT_AP_PASS
#endif

#ifndef TWLED_PIN
  #define TWLED_PIN ""
#endif

#ifndef SPIFFS_EDITOR_AIRCOOOKIE
  #error You are not using the Aircoookie fork of the ESPAsyncWebserver library.\
  Using upstream puts your WiFi password at risk of being served by the filesystem.\
  Comment out this error message to build regardless.
#endif

#ifndef TWLED_DISABLE_INFRARED
  #include <IRremoteESP8266.h>
  #include <IRrecv.h>
  #include <IRutils.h>
#endif

//Filesystem to use for preset and config files. SPIFFS or LittleFS on ESP8266, SPIFFS only on ESP32 (now using LITTLEFS port by lorol)
#ifdef ESP8266
  #define TWLED_FS LittleFS
#else
  #if LOROL_LITTLEFS
    #define TWLED_FS LITTLEFS
  #else
    #define TWLED_FS LittleFS
  #endif
#endif

// GLOBAL VARIABLES
// both declared and defined in header (solution from http://www.keil.com/support/docs/1868.htm)
//
//e.g. byte test = 2 becomes WLED_GLOBAL byte test _INIT(2);
//     int arr[]{0,1,2} becomes WLED_GLOBAL int arr[] _INIT_N(({0,1,2}));

#ifndef TWLED_DEFINE_GLOBAL_VARS
  #define TWLED_GLOBAL extern
  #define _INIT(x)
  #define _INIT_N(x)
  #define _INIT_PROGMEM(x)
#else
  #define TWLED_GLOBAL
  #define _INIT(x) = x
  //needed to ignore commas in array definitions
  #define UNPACK( ... ) __VA_ARGS__
  #define _INIT_N(x) UNPACK x
  #define _INIT_PROGMEM(x) PROGMEM = x
#endif

#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)

#define TWLED_CODENAME "BDD"

// AP and OTA default passwords (for maximum security change them!)
TWLED_GLOBAL char apPass[65]  _INIT(TWLED_AP_PASS);
#ifdef TWLED_OTA_PASS
TWLED_GLOBAL char otaPass[33] _INIT(TWLED_OTA_PASS);
#else
TWLED_GLOBAL char otaPass[33] _INIT(DEFAULT_OTA_PASS);
#endif

// Hardware and pin config
#ifndef BTNPIN
  #define BTNPIN 0,-1
#endif
#ifndef BTNTYPE
  #define BTNTYPE BTN_TYPE_PUSH,BTN_TYPE_NONE
#endif
#ifndef RLYPIN
TWLED_GLOBAL int8_t rlyPin _INIT(-1);
#else
TWLED_GLOBAL int8_t rlyPin _INIT(RLYPIN);
#endif
//Relay mode (1 = active high, 0 = active low, flipped in cfg.json)
#ifndef RLYMDE
TWLED_GLOBAL bool rlyMde _INIT(true);
#else
TWLED_GLOBAL bool rlyMde _INIT(RLYMDE);
#endif
//Use open drain (floating pin) when relay should be off
#ifndef RLYODRAIN
TWLED_GLOBAL bool rlyOpenDrain _INIT(false);
#else
TWLED_GLOBAL bool rlyOpenDrain _INIT(RLYODRAIN);
#endif
#ifndef IRPIN
  #define IRPIN -1
#endif
#ifndef IRTYPE
  #define IRTYPE 0
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || (defined(RX) && defined(TX))
  // use RX/TX as set by the framework - these boards do _not_ have RX=3 and TX=1
  constexpr uint8_t hardwareRX = RX;
  constexpr uint8_t hardwareTX = TX;
#else
  // use defaults for RX/TX
  constexpr uint8_t hardwareRX = 3;
  constexpr uint8_t hardwareTX = 1;
#endif

TWLED_GLOBAL char ntpServerName[33] _INIT("0.wled.pool.ntp.org");   // NTP server to use

// WiFi CONFIG (all these can be changed via web UI, no need to set them here)
TWLED_GLOBAL std::vector<WiFiConfig> multiWiFi;
TWLED_GLOBAL IPAddress dnsAddress _INIT_N(((  8,   8,  8,  8)));   // Google's DNS
TWLED_GLOBAL char cmDNS[33]       _INIT(MDNS_NAME);                // mDNS address (*.local, replaced by wledXXXXXX if default is used)
TWLED_GLOBAL char apSSID[33]      _INIT("");                       // AP off by default (unless setup)
#ifdef TWLED_SAVE_RAM
typedef class WiFiOptions {
  public:
    struct {
      uint8_t selectedWiFi : 4; // max 16 SSIDs
      uint8_t apChannel    : 4;
      uint8_t apHide       : 3;
      uint8_t apBehavior   : 3;
      bool    noWifiSleep  : 1;
      bool    force802_3g  : 1;
    };
    WiFiOptions(uint8_t s, uint8_t c, bool h, uint8_t b, bool sl, bool g) {
      selectedWiFi = s;
      apChannel = c;
      apHide = h;
      apBehavior = b;
      noWifiSleep = sl;
      force802_3g = g;
    }
} __attribute__ ((aligned(1), packed)) wifi_options_t;
  #ifdef ARDUINO_ARCH_ESP32
TWLED_GLOBAL wifi_options_t wifiOpt _INIT_N(({0, 1, false, AP_BEHAVIOR_BOOT_NO_CONN, true, false}));
  #else
TWLED_GLOBAL wifi_options_t wifiOpt _INIT_N(({0, 1, false, AP_BEHAVIOR_BOOT_NO_CONN, false, false}));
  #endif
#define selectedWiFi wifiOpt.selectedWiFi
#define apChannel    wifiOpt.apChannel
#define apHide       wifiOpt.apHide
#define apBehavior   wifiOpt.apBehavior
#define noWifiSleep  wifiOpt.noWifiSleep
#define force802_3g  wifiOpt.force802_3g
#else
TWLED_GLOBAL int8_t selectedWiFi  _INIT(0);
TWLED_GLOBAL byte apChannel       _INIT(1);                        // 2.4GHz WiFi AP channel (1-13)
TWLED_GLOBAL byte apHide          _INIT(0);                        // hidden AP SSID
TWLED_GLOBAL byte apBehavior      _INIT(AP_BEHAVIOR_BOOT_NO_CONN); // access point opens when no connection after boot by default
  #ifdef ARDUINO_ARCH_ESP32
TWLED_GLOBAL bool noWifiSleep _INIT(true);                         // disabling modem sleep modes will increase heat output and power usage, but may help with connection issues
  #else
TWLED_GLOBAL bool noWifiSleep _INIT(false);
  #endif
TWLED_GLOBAL bool force802_3g _INIT(false);
#endif // TWLED_SAVE_RAM
#ifdef ARDUINO_ARCH_ESP32
  #if defined(LOLIN_WIFI_FIX) && (defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
TWLED_GLOBAL uint8_t txPower _INIT(WIFI_POWER_8_5dBm);
  #else
TWLED_GLOBAL uint8_t txPower _INIT(WIFI_POWER_19_5dBm);
  #endif
#endif
#define TWLED_WIFI_CONFIGURED isWiFiConfigured()

#if defined(ARDUINO_ARCH_ESP32) && defined(TWLED_USE_ETHERNET)
  #ifdef TWLED_ETH_DEFAULT                                          // default ethernet board type if specified
    TWLED_GLOBAL int ethernetType _INIT(TWLED_ETH_DEFAULT);          // ethernet board type
  #else
    TWLED_GLOBAL int ethernetType _INIT(TWLED_ETH_NONE);             // use none for ethernet board type if default not defined
  #endif
#endif

// LED CONFIG
TWLED_GLOBAL bool turnOnAtBoot _INIT(true);                // turn on LEDs at power-up
TWLED_GLOBAL byte bootPreset   _INIT(0);                   // save preset to load after power-up

//if true, a segment per bus will be created on boot and LED settings save
//if false, only one segment spanning the total LEDs is created,
//but not on LED settings save if there is more than one segment currently
#ifdef ESP8266
TWLED_GLOBAL bool useGlobalLedBuffer _INIT(false); // double buffering disabled on ESP8266
#else
TWLED_GLOBAL bool useGlobalLedBuffer _INIT(true);  // double buffering enabled on ESP32
  #ifndef CONFIG_IDF_TARGET_ESP32C3
TWLED_GLOBAL bool useParallelI2S     _INIT(false); // parallel I2S for ESP32
  #endif
#endif
#ifdef TWLED_USE_IC_CCT
TWLED_GLOBAL bool cctICused          _INIT(true);  // CCT IC used (Athom 15W bulbs)
#else
TWLED_GLOBAL bool cctICused          _INIT(false); // CCT IC used (Athom 15W bulbs)
#endif
TWLED_GLOBAL bool gammaCorrectCol    _INIT(true);  // use gamma correction on colors
TWLED_GLOBAL bool gammaCorrectBri    _INIT(false); // use gamma correction on brightness
TWLED_GLOBAL float gammaCorrectVal   _INIT(2.2f);  // gamma correction value

TWLED_GLOBAL byte colPri[] _INIT_N(({ 255, 160, 0, 0 }));  // current RGB(W) primary color. colPri[] should be updated if you want to change the color.
TWLED_GLOBAL byte colSec[] _INIT_N(({ 0, 0, 0, 0 }));      // current RGB(W) secondary color
TWLED_GLOBAL byte nightlightTargetBri _INIT(0);      // brightness after nightlight is over
TWLED_GLOBAL byte nightlightDelayMins _INIT(60);
TWLED_GLOBAL byte nightlightMode      _INIT(NL_MODE_FADE); // See const.h for available modes. Was nightlightFade

TWLED_GLOBAL byte briMultiplier _INIT(100);          // % of brightness to set (to limit power, if you set it to 50 and set bri to 255, actual brightness will be 127)

// User Interface CONFIG
#ifndef SERVERNAME
TWLED_GLOBAL char serverDescription[33] _INIT("TWLED");  // Name of module - use default
#else
TWLED_GLOBAL char serverDescription[33] _INIT(SERVERNAME);  // use predefined name
#endif
TWLED_GLOBAL bool simplifiedUI          _INIT(false);   // enable simplified UI
TWLED_GLOBAL byte cacheInvalidate       _INIT(0);       // used to invalidate browser cache

// Sync CONFIG
TWLED_GLOBAL NodesMap Nodes;
TWLED_GLOBAL bool nodeListEnabled _INIT(true);
TWLED_GLOBAL bool nodeBroadcastEnabled _INIT(true);

#ifndef TWLED_DISABLE_INFRARED
TWLED_GLOBAL int8_t irPin        _INIT(IRPIN);
TWLED_GLOBAL byte irEnabled      _INIT(IRTYPE); // Infrared receiver
#endif
TWLED_GLOBAL bool irApplyToAllSelected _INIT(true); //apply IR or ESP-NOW to all selected segments

#ifndef TWLED_DISABLE_ALEXA
TWLED_GLOBAL bool alexaEnabled _INIT(false);                       // enable device discovery by Amazon Echo
TWLED_GLOBAL char alexaInvocationName[33] _INIT("Light");          // speech control name of device. Choose something voice-to-text can understand
TWLED_GLOBAL byte alexaNumPresets _INIT(0);                        // number of presets to expose to Alexa, starting from preset 1, up to 9
#endif

TWLED_GLOBAL uint16_t realtimeTimeoutMs _INIT(2500);               // ms timeout of realtime mode before returning to normal mode
TWLED_GLOBAL int arlsOffset _INIT(0);                              // realtime LED offset
TWLED_GLOBAL bool arlsDisableGammaCorrection _INIT(true);          // activate if gamma correction is handled by the source
TWLED_GLOBAL bool arlsForceMaxBri _INIT(false);                    // enable to force max brightness if source has very dark colors that would be black

#ifdef TWLED_ENABLE_DMX
 #if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  TWLED_GLOBAL DMXESPSerial dmx;
 #else //ESP32
  TWLED_GLOBAL SparkFunDMX dmx;
 #endif
  TWLED_GLOBAL uint16_t e131ProxyUniverse _INIT(0);                  // output this E1.31 (sACN) / ArtNet universe via MAX485 (0 = disabled)
  // dmx CONFIG
  TWLED_GLOBAL byte DMXChannels _INIT(7);        // number of channels per fixture
  TWLED_GLOBAL byte DMXFixtureMap[15] _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
  // assigns the different channels to different functions. See wled21_dmx.ino for more information.
  TWLED_GLOBAL uint16_t DMXGap _INIT(10);          // gap between the fixtures. makes addressing easier because you don't have to memorize odd numbers when climbing up onto a rig.
  TWLED_GLOBAL uint16_t DMXStart _INIT(10);        // start address of the first fixture
  TWLED_GLOBAL uint16_t DMXStartLED _INIT(0);      // LED from which DMX fixtures start
#endif
#ifdef TWLED_ENABLE_DMX_INPUT
  TWLED_GLOBAL int dmxInputTransmitPin _INIT(0);
  TWLED_GLOBAL int dmxInputReceivePin _INIT(0);
  TWLED_GLOBAL int dmxInputEnablePin _INIT(0);
  TWLED_GLOBAL int dmxInputPort _INIT(2);
  TWLED_GLOBAL DMXInput dmxInput;
#endif

TWLED_GLOBAL uint16_t e131Universe _INIT(1);                       // settings for E1.31 (sACN) protocol (only DMX_MODE_MULTIPLE_* can span over consequtive universes)
TWLED_GLOBAL uint16_t e131Port _INIT(5568);                        // DMX in port. E1.31 default is 5568, Art-Net is 6454
TWLED_GLOBAL byte e131Priority _INIT(0);                           // E1.31 port priority (if != 0 priority handling is active)
TWLED_GLOBAL E131Priority highPriority _INIT(3);                   // E1.31 highest priority tracking, init = timeout in seconds
TWLED_GLOBAL byte DMXMode _INIT(DMX_MODE_MULTIPLE_RGB);            // DMX mode (s.a.)
TWLED_GLOBAL uint16_t DMXAddress _INIT(1);                         // DMX start address of fixture, a.k.a. first Channel [for E1.31 (sACN) protocol]
TWLED_GLOBAL uint16_t DMXSegmentSpacing _INIT(0);                  // Number of void/unused channels between each segments DMX channels
TWLED_GLOBAL byte e131LastSequenceNumber[E131_MAX_UNIVERSE_COUNT]; // to detect packet loss
TWLED_GLOBAL bool e131Multicast _INIT(false);                      // multicast or unicast
TWLED_GLOBAL bool e131SkipOutOfSequence _INIT(false);              // freeze instead of flickering
TWLED_GLOBAL uint16_t pollReplyCount _INIT(0);                     // count number of replies for ArtPoll node report

// mqtt
TWLED_GLOBAL unsigned long lastMqttReconnectAttempt _INIT(0);  // used for other periodic tasks too
#ifndef TWLED_DISABLE_MQTT
  #ifndef MQTT_MAX_TOPIC_LEN
    #define MQTT_MAX_TOPIC_LEN 32
  #endif
  #ifndef MQTT_MAX_SERVER_LEN
    #define MQTT_MAX_SERVER_LEN 32
  #endif
TWLED_GLOBAL AsyncMqttClient *mqtt _INIT(NULL);
TWLED_GLOBAL bool mqttEnabled _INIT(false);
TWLED_GLOBAL char mqttStatusTopic[MQTT_MAX_TOPIC_LEN + 8] _INIT("");         // this must be global because of async handlers
TWLED_GLOBAL char mqttDeviceTopic[MQTT_MAX_TOPIC_LEN + 1] _INIT("");         // main MQTT topic (individual per device, default is wled/mac)
TWLED_GLOBAL char mqttGroupTopic[MQTT_MAX_TOPIC_LEN + 1]  _INIT("wled/all"); // second MQTT topic (for example to group devices)
TWLED_GLOBAL char mqttServer[MQTT_MAX_SERVER_LEN + 1]     _INIT("");         // both domains and IPs should work (no SSL)
TWLED_GLOBAL char mqttUser[41] _INIT("");                   // optional: username for MQTT auth
TWLED_GLOBAL char mqttPass[65] _INIT("");                   // optional: password for MQTT auth
TWLED_GLOBAL char mqttClientID[41] _INIT("");               // override the client ID
TWLED_GLOBAL uint16_t mqttPort _INIT(1883);
TWLED_GLOBAL bool retainMqttMsg _INIT(false);               // retain brightness and color
#define TWLED_MQTT_CONNECTED (mqtt != nullptr && mqtt->connected())
#else
#define TWLED_MQTT_CONNECTED false
#endif

#ifndef TWLED_DISABLE_HUESYNC
TWLED_GLOBAL bool huePollingEnabled _INIT(false);           // poll hue bridge for light state
TWLED_GLOBAL uint16_t huePollIntervalMs _INIT(2500);        // low values (< 1sec) may cause lag but offer quicker response
TWLED_GLOBAL char hueApiKey[47] _INIT("api");               // key token will be obtained from bridge
TWLED_GLOBAL byte huePollLightId _INIT(1);                  // ID of hue lamp to sync to. Find the ID in the hue app ("about" section)
TWLED_GLOBAL IPAddress hueIP _INIT_N(((0, 0, 0, 0))); // IP address of the bridge
TWLED_GLOBAL bool hueApplyOnOff _INIT(true);
TWLED_GLOBAL bool hueApplyBri _INIT(true);
TWLED_GLOBAL bool hueApplyColor _INIT(true);
#endif

TWLED_GLOBAL uint16_t serialBaud _INIT(1152); // serial baud rate, multiply by 100
TWLED_GLOBAL bool     serialCanRX _INIT(false);
TWLED_GLOBAL bool     serialCanTX _INIT(false);

#ifndef TWLED_DISABLE_ESPNOW
TWLED_GLOBAL bool enableESPNow        _INIT(false);  // global on/off for ESP-NOW
TWLED_GLOBAL byte statusESPNow        _INIT(ESP_NOW_STATE_UNINIT); // state of ESP-NOW stack (0 uninitialised, 1 initialised, 2 error)
TWLED_GLOBAL bool useESPNowSync       _INIT(false);  // use ESP-NOW wireless technology for sync
//TWLED_GLOBAL char linked_remote[13]   _INIT("");     // MAC of ESP-NOW remote (Wiz Mote)
TWLED_GLOBAL std::vector<std::array<char, 13>> linked_remotes; // MAC of ESP-NOW remotes (Wiz Mote)
TWLED_GLOBAL char last_signal_src[13] _INIT("");     // last seen ESP-NOW sender
#endif

// Time CONFIG
#ifndef TWLED_NTP_ENABLED
  #define TWLED_NTP_ENABLED false
#endif
#ifndef TWLED_TIMEZONE
  #define TWLED_TIMEZONE 0
#endif
#ifndef TWLED_UTC_OFFSET
  #define TWLED_UTC_OFFSET 0
#endif
TWLED_GLOBAL bool ntpEnabled      _INIT(TWLED_NTP_ENABLED); // get internet time. Only required if you use clock overlays or time-activated macros
TWLED_GLOBAL bool useAMPM         _INIT(false);            // 12h/24h clock format
TWLED_GLOBAL byte currentTimezone _INIT(TWLED_TIMEZONE);    // Timezone ID. Refer to timezones array in wled10_ntp.ino
TWLED_GLOBAL int utcOffsetSecs    _INIT(TWLED_UTC_OFFSET);  // Seconds to offset from UTC before timzone calculation

TWLED_GLOBAL byte overlayCurrent _INIT(0);    // 0: no overlay 1: analog clock 2: was single-digit clock 3: was cronixie
TWLED_GLOBAL uint16_t overlayMin _INIT(0), overlayMax _INIT(DEFAULT_LED_COUNT - 1);   // boundaries of overlay mode

TWLED_GLOBAL byte analogClock12pixel _INIT(0);               // The pixel in your strip where "midnight" would be
TWLED_GLOBAL bool analogClockSecondsTrail _INIT(false);      // Display seconds as trail of LEDs instead of a single pixel
TWLED_GLOBAL bool analogClock5MinuteMarks _INIT(false);      // Light pixels at every 5-minute position
TWLED_GLOBAL bool analogClockSolidBlack _INIT(false);        // Show clock overlay only if all LEDs are solid black (effect is 0 and color is black)

TWLED_GLOBAL bool countdownMode _INIT(false);                         // Clock will count down towards date
TWLED_GLOBAL byte countdownYear _INIT(20), countdownMonth _INIT(1);   // Countdown target date, year is last two digits
TWLED_GLOBAL byte countdownDay  _INIT(1) , countdownHour  _INIT(0);
TWLED_GLOBAL byte countdownMin  _INIT(0) , countdownSec   _INIT(0);

TWLED_GLOBAL byte macroNl   _INIT(0);        // after nightlight delay over
TWLED_GLOBAL byte macroCountdown _INIT(0);
TWLED_GLOBAL byte macroAlexaOn _INIT(0), macroAlexaOff _INIT(0);
TWLED_GLOBAL byte macroButton[TWLED_MAX_BUTTONS]        _INIT({0});
TWLED_GLOBAL byte macroLongPress[TWLED_MAX_BUTTONS]     _INIT({0});
TWLED_GLOBAL byte macroDoublePress[TWLED_MAX_BUTTONS]   _INIT({0});

// Security CONFIG
#ifdef TWLED_OTA_PASS
TWLED_GLOBAL bool otaLock        _INIT(true);     // prevents OTA firmware updates without password. ALWAYS enable if system exposed to any public networks
#else
TWLED_GLOBAL bool otaLock        _INIT(false);     // prevents OTA firmware updates without password. ALWAYS enable if system exposed to any public networks
#endif
TWLED_GLOBAL bool wifiLock       _INIT(false);     // prevents access to WiFi settings when OTA lock is enabled
#ifdef TWLED_ENABLE_AOTA
TWLED_GLOBAL bool aOtaEnabled    _INIT(true);      // ArduinoOTA allows easy updates directly from the IDE. Careful, it does not auto-disable when OTA lock is on
#else
TWLED_GLOBAL bool aOtaEnabled    _INIT(false);     // ArduinoOTA allows easy updates directly from the IDE. Careful, it does not auto-disable when OTA lock is on
#endif
TWLED_GLOBAL bool otaSameSubnet  _INIT(true);      // prevent OTA updates from other subnets (e.g. internet) if no PIN is set
TWLED_GLOBAL char settingsPIN[5] _INIT(TWLED_PIN);  // PIN for settings pages
TWLED_GLOBAL bool correctPIN     _INIT(!strlen(settingsPIN));
TWLED_GLOBAL unsigned long lastEditTime _INIT(0);

TWLED_GLOBAL uint16_t userVar0 _INIT(0), userVar1 _INIT(0); //available for use in usermod

// internal global variable declarations
// wifi
TWLED_GLOBAL bool apActive _INIT(false);
TWLED_GLOBAL byte apClients _INIT(0);
TWLED_GLOBAL bool forceReconnect _INIT(false);
TWLED_GLOBAL unsigned long lastReconnectAttempt _INIT(0);
TWLED_GLOBAL bool interfacesInited _INIT(false);
TWLED_GLOBAL bool wasConnected _INIT(false);
// color
TWLED_GLOBAL byte lastRandomIndex _INIT(0);        // used to save last random color so the new one is not the same
TWLED_GLOBAL std::vector<CRGBPalette16> customPalettes;  // custom palettes
TWLED_GLOBAL uint8_t paletteBlend _INIT(0);        // determines blending and wrapping of palette: 0: blend, wrap if moving (SEGMENT.speed>0); 1: blend, always wrap; 2: blend, never wrap; 3: don't blend or wrap
// transitions
TWLED_GLOBAL uint8_t       blendingStyle            _INIT(0);      // effect blending/transitionig style
TWLED_GLOBAL bool          transitionActive         _INIT(false);
TWLED_GLOBAL uint16_t      transitionDelay          _INIT(750);    // global transition duration
TWLED_GLOBAL uint16_t      transitionDelayDefault   _INIT(750);    // default transition time (stored in cfg.json)
TWLED_GLOBAL unsigned long transitionStartTime;
TWLED_GLOBAL bool          jsonTransitionOnce       _INIT(false);  // flag to override transitionDelay (playlist, JSON API: "live" & "seg":{"i"} & "tt")
TWLED_GLOBAL uint8_t       randomPaletteChangeTime  _INIT(5);      // amount of time [s] between random palette changes (min: 1s, max: 255s)
TWLED_GLOBAL bool          useHarmonicRandomPalette _INIT(true);   // use *harmonic* random palette generation (nicer looking) or truly random
// nightlight
TWLED_GLOBAL bool nightlightActive _INIT(false);
TWLED_GLOBAL bool nightlightActiveOld _INIT(false);
TWLED_GLOBAL uint32_t nightlightDelayMs _INIT(10);
TWLED_GLOBAL byte nightlightDelayMinsDefault _INIT(nightlightDelayMins);
TWLED_GLOBAL unsigned long nightlightStartTime;
TWLED_GLOBAL unsigned long lastNlUpdate;
TWLED_GLOBAL byte briNlT _INIT(0);                     // current nightlight brightness
TWLED_GLOBAL byte colNlT[] _INIT_N(({ 0, 0, 0, 0 }));        // current nightlight color
// brightness
TWLED_GLOBAL unsigned long lastOnTime _INIT(0);
TWLED_GLOBAL bool offMode             _INIT(!turnOnAtBoot);
TWLED_GLOBAL byte briS                _INIT(128);           // default brightness
TWLED_GLOBAL byte bri                 _INIT(briS);          // global brightness (set)
TWLED_GLOBAL byte briOld              _INIT(0);             // global brightness while in transition loop (previous iteration)
TWLED_GLOBAL byte briT                _INIT(0);             // global brightness during transition
TWLED_GLOBAL byte briLast             _INIT(128);           // brightness before turned off. Used for toggle function
TWLED_GLOBAL byte whiteLast           _INIT(128);           // white channel before turned off. Used for toggle function in ir.cpp
// button
TWLED_GLOBAL int8_t btnPin[TWLED_MAX_BUTTONS]                   _INIT({BTNPIN});
TWLED_GLOBAL byte buttonType[TWLED_MAX_BUTTONS]                 _INIT({BTNTYPE});
TWLED_GLOBAL bool buttonPublishMqtt                            _INIT(false);
TWLED_GLOBAL bool buttonPressedBefore[TWLED_MAX_BUTTONS]        _INIT({false});
TWLED_GLOBAL bool buttonLongPressed[TWLED_MAX_BUTTONS]          _INIT({false});
TWLED_GLOBAL unsigned long buttonPressedTime[TWLED_MAX_BUTTONS] _INIT({0});
TWLED_GLOBAL unsigned long buttonWaitTime[TWLED_MAX_BUTTONS]    _INIT({0});
TWLED_GLOBAL bool disablePullUp                                _INIT(false);
TWLED_GLOBAL byte touchThreshold                               _INIT(TOUCH_THRESHOLD);

// notifications
TWLED_GLOBAL bool sendNotifications    _INIT(false);           // master notification switch
TWLED_GLOBAL bool sendNotificationsRT  _INIT(false);           // master notification switch (runtime)
TWLED_GLOBAL unsigned long notificationSentTime _INIT(0);
TWLED_GLOBAL byte notificationSentCallMode _INIT(CALL_MODE_INIT);
TWLED_GLOBAL uint8_t notificationCount _INIT(0);
TWLED_GLOBAL uint8_t syncGroups    _INIT(0x01);                // sync send groups this instance syncs to (bit mapped)
TWLED_GLOBAL uint8_t receiveGroups _INIT(0x01);                // sync receive groups this instance belongs to (bit mapped)
#ifdef TWLED_SAVE_RAM
// this will save us 8 bytes of RAM while increasing code by ~400 bytes
typedef class Receive {
  public:
    union {
      uint8_t   Options;
      struct {
        bool    Brightness     : 1;
        bool    Color          : 1;
        bool    Effects        : 1;
        bool    SegmentOptions : 1;
        bool    SegmentBounds  : 1;
        bool    Direct         : 1;
        bool    Palette        : 1;
        uint8_t reserved       : 1;
      };
    };
    Receive(int i) { Options = i; }
    Receive(bool b, bool c, bool e, bool sO, bool sB, bool p)
    : Brightness(b)
    , Color(c)
    , Effects(e)
    , SegmentOptions(sO)
    , SegmentBounds(sB)
    , Palette(p)
    {};
} __attribute__ ((aligned(1), packed)) receive_notification_t;
typedef class Send {
  public:
    union {
      uint8_t Options;
      struct {
        bool Direct : 1;
        bool Button : 1;
        bool Alexa  : 1;
        bool Hue    : 1;
        uint8_t reserved : 4;
      };
    };
  Send(int o) { Options = o; }
  Send(bool d, bool b, bool a, bool h) {
    Direct = d;
    Button = b;
    Alexa = a;
    Hue = h;
  }
} __attribute__ ((aligned(1), packed)) send_notification_t;
TWLED_GLOBAL receive_notification_t receiveN _INIT(0b01100111);
TWLED_GLOBAL send_notification_t    notifyG  _INIT(0b00001111);
#define receiveNotificationBrightness receiveN.Brightness
#define receiveNotificationColor      receiveN.Color
#define receiveNotificationEffects    receiveN.Effects
#define receiveNotificationPalette    receiveN.Palette
#define receiveSegmentOptions         receiveN.SegmentOptions
#define receiveSegmentBounds          receiveN.SegmentBounds
#define receiveDirect                 receiveN.Direct
#define notifyDirect notifyG.Direct
#define notifyButton notifyG.Button
#define notifyAlexa  notifyG.Alexa
#define notifyHue    notifyG.Hue
#else
TWLED_GLOBAL bool receiveNotificationBrightness _INIT(true);       // apply brightness from incoming notifications
TWLED_GLOBAL bool receiveNotificationColor      _INIT(true);       // apply color
TWLED_GLOBAL bool receiveNotificationEffects    _INIT(true);       // apply effects setup
TWLED_GLOBAL bool receiveNotificationPalette    _INIT(true);       // apply palette
TWLED_GLOBAL bool receiveSegmentOptions         _INIT(false);      // apply segment options
TWLED_GLOBAL bool receiveSegmentBounds          _INIT(false);      // apply segment bounds (start, stop, offset)
TWLED_GLOBAL bool receiveDirect _INIT(true);                       // receive UDP/Hyperion realtime
TWLED_GLOBAL bool notifyDirect _INIT(true);                        // send notification if change via UI or HTTP API
TWLED_GLOBAL bool notifyButton _INIT(true);                        // send if updated by button or infrared remote
TWLED_GLOBAL bool notifyAlexa  _INIT(false);                       // send notification if updated via Alexa
TWLED_GLOBAL bool notifyHue    _INIT(false);                       // send notification if Hue light changes
#endif

// effects
TWLED_GLOBAL byte effectCurrent _INIT(0);
TWLED_GLOBAL byte effectSpeed _INIT(128);
TWLED_GLOBAL byte effectIntensity _INIT(128);
TWLED_GLOBAL byte effectPalette _INIT(0);
TWLED_GLOBAL bool stateChanged _INIT(false);

// network
#ifdef TWLED_SAVE_RAM
// this will save us 2 bytes of RAM while increasing code by ~400 bytes
typedef class Udp {
  public:
    uint16_t  Port;
    uint16_t  Port2;
    uint16_t  RgbPort;
    struct {
      uint8_t NumRetries : 5;
      bool    Connected : 1;
      bool    Connected2 : 1;
      bool    RgbConnected : 1;
    };
    Udp(int p1, int p2, int p3, int r, bool c1, bool c2, bool c3) {
      Port = p1;
      Port2 = p2;
      RgbPort = p3;
      NumRetries = r;
      Connected = c1;
      Connected2 = c2;
      RgbConnected = c3;
    }
} __attribute__ ((aligned(1), packed)) udp_port_t;
TWLED_GLOBAL udp_port_t udp _INIT_N(({21234, 65506, 19446, 0, false, false, false}));
#define udpPort         udp.Port
#define udpPort2        udp.Port2
#define udpRgbPort      udp.RgbPort
#define udpNumRetries   udp.NumRetries
#define udpConnected    udp.Connected
#define udp2Connected   udp.Connected2
#define udpRgbConnected udp.RgbConnected
#else
TWLED_GLOBAL uint16_t udpPort    _INIT(21324); // TWLED notifier default port
TWLED_GLOBAL uint16_t udpPort2   _INIT(65506); // TWLED notifier supplemental port
TWLED_GLOBAL uint16_t udpRgbPort _INIT(19446); // Hyperion port
TWLED_GLOBAL uint8_t  udpNumRetries _INIT(0);  // Number of times a UDP sync message is retransmitted. Increase to increase reliability
TWLED_GLOBAL bool     udpConnected _INIT(false);
TWLED_GLOBAL bool     udp2Connected _INIT(false);
TWLED_GLOBAL bool     udpRgbConnected _INIT(false);
#endif

// ui style
TWLED_GLOBAL bool showWelcomePage _INIT(false);

// hue
#ifndef WLED_DISABLE_HUESYNC
TWLED_GLOBAL byte hueError _INIT(HUE_ERROR_INACTIVE);
// TWLED_GLOBAL uint16_t hueFailCount _INIT(0);
TWLED_GLOBAL float hueXLast _INIT(0), hueYLast _INIT(0);
TWLED_GLOBAL uint16_t hueHueLast _INIT(0), hueCtLast _INIT(0);
TWLED_GLOBAL byte hueSatLast _INIT(0), hueBriLast _INIT(0);
TWLED_GLOBAL unsigned long hueLastRequestSent _INIT(0);
TWLED_GLOBAL bool hueAuthRequired _INIT(false);
TWLED_GLOBAL bool hueReceived _INIT(false);
TWLED_GLOBAL bool hueStoreAllowed _INIT(false), hueNewKey _INIT(false);
#endif

// countdown
TWLED_GLOBAL unsigned long countdownTime _INIT(1514764800L);
TWLED_GLOBAL bool countdownOverTriggered _INIT(true);

//timer
TWLED_GLOBAL byte lastTimerMinute  _INIT(0);
TWLED_GLOBAL byte timerHours[]     _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
TWLED_GLOBAL int8_t timerMinutes[] _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
TWLED_GLOBAL byte timerMacro[]     _INIT_N(({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }));
//weekdays to activate on, bit pattern of arr elem: 0b11111111: sun,sat,fri,thu,wed,tue,mon,validity
TWLED_GLOBAL byte timerWeekday[]   _INIT_N(({ 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 }));
//upper 4 bits start, lower 4 bits end month (default 28: start month 1 and end month 12)
TWLED_GLOBAL byte timerMonth[]     _INIT_N(({28,28,28,28,28,28,28,28}));
TWLED_GLOBAL byte timerDay[]       _INIT_N(({1,1,1,1,1,1,1,1}));
TWLED_GLOBAL byte timerDayEnd[]		_INIT_N(({31,31,31,31,31,31,31,31}));
TWLED_GLOBAL bool doAdvancePlaylist _INIT(false);

//improv
TWLED_GLOBAL byte improvActive _INIT(0); //0: no improv packet received, 1: improv active, 2: provisioning
TWLED_GLOBAL byte improvError _INIT(0);
//playlists
TWLED_GLOBAL int16_t currentPlaylist _INIT(-1);
//still used for "PL=~" HTTP API command
TWLED_GLOBAL byte presetCycCurr _INIT(0);
TWLED_GLOBAL byte presetCycMin _INIT(1);
TWLED_GLOBAL byte presetCycMax _INIT(5);

// realtime
TWLED_GLOBAL byte realtimeMode _INIT(REALTIME_MODE_INACTIVE);
TWLED_GLOBAL byte realtimeOverride _INIT(REALTIME_OVERRIDE_NONE);
TWLED_GLOBAL IPAddress realtimeIP _INIT_N(((0, 0, 0, 0)));
TWLED_GLOBAL unsigned long realtimeTimeout _INIT(0);
TWLED_GLOBAL uint8_t tpmPacketCount _INIT(0);
TWLED_GLOBAL uint16_t tpmPayloadFrameSize _INIT(0);
TWLED_GLOBAL bool useMainSegmentOnly _INIT(false);
TWLED_GLOBAL bool realtimeRespectLedMaps _INIT(true);                     // Respect LED maps when receiving realtime data

TWLED_GLOBAL unsigned long lastInterfaceUpdate _INIT(0);
TWLED_GLOBAL byte interfaceUpdateCallMode _INIT(CALL_MODE_INIT);
// alexa udp
TWLED_GLOBAL String escapedMac;
#ifndef TWLED_DISABLE_ALEXA
  TWLED_GLOBAL Espalexa espalexa;
  TWLED_GLOBAL EspalexaDevice* espalexaDevice;
#endif

// dns server
TWLED_GLOBAL DNSServer dnsServer;
// network time
#ifndef TWLED_LAT
  #define TWLED_LAT 0.0f
#endif
#ifndef TWLED_LON
  #define TWLED_LON 0.0f
#endif
#define NTP_NEVER 999000000L
TWLED_GLOBAL bool ntpConnected _INIT(false);
TWLED_GLOBAL time_t localTime _INIT(0);
TWLED_GLOBAL unsigned long ntpLastSyncTime _INIT(NTP_NEVER);
TWLED_GLOBAL unsigned long ntpPacketSentTime _INIT(NTP_NEVER);
TWLED_GLOBAL IPAddress ntpServerIP;
TWLED_GLOBAL uint16_t ntpLocalPort _INIT(2390);
TWLED_GLOBAL uint16_t rolloverMillis _INIT(0);
TWLED_GLOBAL float longitude _INIT(TWLED_LON);
TWLED_GLOBAL float latitude _INIT(TWLED_LAT);
TWLED_GLOBAL time_t sunrise _INIT(0);
TWLED_GLOBAL time_t sunset _INIT(0);
TWLED_GLOBAL Toki toki _INIT(Toki());

// General filesystem
TWLED_GLOBAL size_t fsBytesUsed _INIT(0);
TWLED_GLOBAL size_t fsBytesTotal _INIT(0);
TWLED_GLOBAL unsigned long presetsModifiedTime _INIT(0L);
TWLED_GLOBAL bool doCloseFile _INIT(false);

// presets
TWLED_GLOBAL byte currentPreset _INIT(0);

TWLED_GLOBAL byte errorFlag _INIT(0);

TWLED_GLOBAL String messageHead, messageSub;
TWLED_GLOBAL byte optionType;

TWLED_GLOBAL bool configNeedsWrite  _INIT(false);        // flag to initiate saving of config
TWLED_GLOBAL bool doReboot          _INIT(false);        // flag to initiate reboot from async handlers

// status led
#if defined(STATUSLED)
TWLED_GLOBAL unsigned long ledStatusLastMillis _INIT(0);
TWLED_GLOBAL uint8_t ledStatusType _INIT(0); // current status type - corresponds to number of blinks per second
TWLED_GLOBAL bool ledStatusState _INIT(false); // the current LED state
#endif

// server library objects
TWLED_GLOBAL AsyncWebServer server _INIT_N(((80, {0, TWLED_REQUEST_MAX_QUEUE, TWLED_REQUEST_MIN_HEAP, TWLED_REQUEST_HEAP_USAGE})));
#ifdef TWLED_ENABLE_WEBSOCKETS
TWLED_GLOBAL AsyncWebSocket ws _INIT_N((("/ws")));
#endif
#ifndef TWLED_DISABLE_HUESYNC
TWLED_GLOBAL AsyncClient     *hueClient _INIT(NULL);
#endif
TWLED_GLOBAL AsyncWebHandler *editHandler _INIT(nullptr);

// udp interface objects
TWLED_GLOBAL WiFiUDP notifierUdp, rgbUdp, notifier2Udp;
TWLED_GLOBAL WiFiUDP ntpUdp;
TWLED_GLOBAL ESPAsyncE131 e131 _INIT_N(((handleE131Packet)));
TWLED_GLOBAL ESPAsyncE131 ddp  _INIT_N(((handleE131Packet)));
TWLED_GLOBAL bool e131NewData _INIT(false);

// led fx library object
TWLED_GLOBAL WS2812FX   strip         _INIT(WS2812FX());
TWLED_GLOBAL std::vector<BusConfig> busConfigs;    //temporary, to remember values from network callback until after
TWLED_GLOBAL bool       doInitBusses  _INIT(false);
TWLED_GLOBAL int8_t     loadLedmap    _INIT(-1);
TWLED_GLOBAL uint8_t    currentLedmap _INIT(0);
#ifndef ESP8266
TWLED_GLOBAL char  *ledmapNames[TWLED_MAX_LEDMAPS-1] _INIT_N(({nullptr}));
#endif
#if TWLED_MAX_LEDMAPS>16
TWLED_GLOBAL uint32_t ledMaps _INIT(0); // bitfield representation of available ledmaps
#else
TWLED_GLOBAL uint16_t ledMaps _INIT(0); // bitfield representation of available ledmaps
#endif

// global I2C SDA pin (used for usermods)
#ifndef I2CSDAPIN
TWLED_GLOBAL int8_t i2c_sda  _INIT(-1);
#else
TWLED_GLOBAL int8_t i2c_sda  _INIT(I2CSDAPIN);
#endif
// global I2C SCL pin (used for usermods)
#ifndef I2CSCLPIN
TWLED_GLOBAL int8_t i2c_scl  _INIT(-1);
#else
TWLED_GLOBAL int8_t i2c_scl  _INIT(I2CSCLPIN);
#endif

// global SPI DATA/MOSI pin (used for usermods)
#ifndef SPIMOSIPIN
TWLED_GLOBAL int8_t spi_mosi  _INIT(-1);
#else
TWLED_GLOBAL int8_t spi_mosi  _INIT(SPIMOSIPIN);
#endif
// global SPI DATA/MISO pin (used for usermods)
#ifndef SPIMISOPIN
TWLED_GLOBAL int8_t spi_miso  _INIT(-1);
#else
TWLED_GLOBAL int8_t spi_miso  _INIT(SPIMISOPIN);
#endif
// global SPI CLOCK/SCLK pin (used for usermods)
#ifndef SPISCLKPIN
TWLED_GLOBAL int8_t spi_sclk  _INIT(-1);
#else
TWLED_GLOBAL int8_t spi_sclk  _INIT(SPISCLKPIN);
#endif

// global ArduinoJson buffer
#if defined(ARDUINO_ARCH_ESP32)
TWLED_GLOBAL SemaphoreHandle_t jsonBufferLockMutex _INIT(xSemaphoreCreateRecursiveMutex());
#endif
#ifdef BOARD_HAS_PSRAM
// if board has PSRAM, use it for JSON document (allocated in setup())
TWLED_GLOBAL JsonDocument *pDoc _INIT(nullptr);
#else
TWLED_GLOBAL StaticJsonDocument<JSON_BUFFER_SIZE> gDoc;
TWLED_GLOBAL JsonDocument *pDoc _INIT(&gDoc);
#endif
TWLED_GLOBAL volatile uint8_t jsonBufferLock _INIT(0);

// enable additional debug output
#if defined(TWLED_DEBUG_HOST)
  #include "net_debug.h"
  // On the host side, use netcat to receive the log statements: nc -l 7868 -u
  // use -D TWLED_DEBUG_HOST='"192.168.xxx.xxx"' or FQDN within quotes
  #define DEBUGOUT NetDebug
  TWLED_GLOBAL bool netDebugEnabled _INIT(true);
  TWLED_GLOBAL char netDebugPrintHost[33] _INIT(TWLED_DEBUG_HOST);
  #ifndef TWLED_DEBUG_PORT
    #define TWLED_DEBUG_PORT 7868
  #endif
  TWLED_GLOBAL int netDebugPrintPort _INIT(TWLED_DEBUG_PORT);
#else
  #define DEBUGOUT Serial
#endif

#ifdef TWLED_DEBUG
  #ifndef ESP8266
  #include <rom/rtc.h>
  #endif
  #define DEBUG_PRINT(x) DEBUGOUT.print(x)
  #define DEBUG_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUG_PRINTF(x...) DEBUGOUT.printf(x)
  #define DEBUG_PRINTF_P(x...) DEBUGOUT.printf_P(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
  #define DEBUG_PRINTF_P(x...)
#endif

#ifdef TWLED_DEBUG_FS
  #define DEBUGFS_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGFS_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGFS_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUGFS_PRINT(x)
  #define DEBUGFS_PRINTLN(x)
  #define DEBUGFS_PRINTF(x...)
#endif

// debug macro variable definitions
#ifdef TWLED_DEBUG
  TWLED_GLOBAL unsigned long debugTime _INIT(0);
  TWLED_GLOBAL int lastWifiState _INIT(3);
  TWLED_GLOBAL unsigned long wifiStateChangedTime _INIT(0);
  TWLED_GLOBAL unsigned loops _INIT(0);
#endif

#define TWLED_CONNECTED (Network.isConnected())

#ifndef TWLED_AP_SSID_UNIQUE
  #define TWLED_SET_AP_SSID() do { \
    strcpy_P(apSSID, PSTR(TWLED_AP_SSID)); \
  } while(0)
#else
  #define TWLED_SET_AP_SSID() do { \
    snprintf_P(\
      apSSID, \
      sizeof(apSSID)-1, \
      PSTR("%s-%s"), \
      TWLED_BRAND, \
      escapedMac.c_str()+6 \
    ); \
  } while(0)
#endif

//macro to convert F to const
#define SET_F(x)  (const char*)F(x)

//color mangling macros
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

class TWLED {
public:
  TWLED();
  static TWLED& instance()
  {
    static TWLED instance;
    return instance;
  }

  // boot starts here
  void setup();

  void loop();
  void reset();

  void beginStrip();
  void handleConnection();
  void initAP(bool resetAP = false);
  void initConnection();
  void initInterfaces();
  #if defined(STATUSLED)
  void handleStatusLED();
  #endif
  #if TWLED_WATCHDOG_TIMEOUT > 0
  void enableWatchdog();
  void disableWatchdog();
  #endif
};
#endif        // TWLED_H
