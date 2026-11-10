# SPDX-License-Identifier: GPL-3.0-or-later
# PlatformIO pre-build script: copies include/GLOBAL_DEFINES.h into the local
# TFT_eSPI copy as User_Setup.h, so the display driver uses the clock's pins.
import os
import shutil

Import("env")  # noqa: F821 - provided by PlatformIO/SCons

local_lib_dir = "lib/modified_TFT_eSPI/"
if os.path.exists(local_lib_dir):
    target_dir = local_lib_dir
else:
    target_dir = ".pio/libdeps/" + env.subst("$PIOENV") + "/TFT_eSPI/"  # noqa: F821

# copy2 keeps the timestamp, so the library is only rebuilt when the defines change.
shutil.copy2("./include/GLOBAL_DEFINES.h", target_dir + "User_Setup.h")
print("script_configure_tft_lib.py: GLOBAL_DEFINES.h -> " + target_dir + "User_Setup.h")
