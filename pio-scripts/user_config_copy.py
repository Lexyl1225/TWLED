Import('env')
import os
import shutil

# copy TWLED00/my_config_sample.h to TWLED00/my_config.h
if os.path.isfile("twled00/my_config.h"):
    print ("*** use existing my_config.h ***")
else: 
    shutil.copy("twled00/my_config_sample.h", "twled00/my_config.h")