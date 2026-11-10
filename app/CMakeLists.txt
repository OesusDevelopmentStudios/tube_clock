# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(tubeclock VERSION 1.0.0 LANGUAGES C CXX)

set(APP_ID "io.github.OesusDevelopmentStudios.TubeClock")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(CTest)

option(TUBECLOCK_BUILD_GUI "Build the Kirigami application (needs KF6 Kirigami, I18n, CoreAddons, DNSSD, QtKeychain)" ON)
# STRING, not PATH: relative values are meant relative to the repository root.
set(TUBECLOCK_FIRMWARE_DIR "" CACHE STRING "Firmware bundle to install (output of scripts/collect_firmware.py)")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network SerialPort)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CLOCKCORE ${REPO_ROOT}/lib/clockcore)

# ---- vendored littlefs (BSD-3-Clause) ------------------------------------------------
add_library(littlefs STATIC 3rdparty/littlefs/lfs.c 3rdparty/littlefs/lfs_util.c)
target_include_directories(littlefs PUBLIC 3rdparty/littlefs)
# LFS_MULTIVERSION lets us write the littlefs 2.0 disk format the clock's core expects.
target_compile_definitions(littlefs PUBLIC LFS_MULTIVERSION LFS_NO_DEBUG LFS_NO_WARN)
set_target_properties(littlefs PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- core library (no UI; unit tested) --------------------------------------------------
add_library(tubeclock_core STATIC
  src/core/ClockConfig.cpp
  src/core/ClockClient.cpp
  src/core/ClockFilesystem.cpp
  src/core/EspImage.cpp
  src/core/Esptool.cpp
  src/core/FirmwareBundle.cpp
  src/core/ImageSet.cpp
  src/core/LittleFsImage.cpp
  src/core/SerialDiscovery.cpp
  src/core/TimeZones.cpp
  # shared with the firmware
  ${CLOCKCORE}/src/Validation.cpp
  ${CLOCKCORE}/src/ImageSet.cpp
)
target_include_directories(tubeclock_core PUBLIC src/core ${CLOCKCORE}/include)
target_link_libraries(tubeclock_core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::SerialPort PRIVATE littlefs)
target_compile_options(tubeclock_core PRIVATE -Wall -Wextra)

# ---- application ------------------------------------------------------------------------
if(TUBECLOCK_BUILD_GUI)
  add_subdirectory(src/ui)
endif()

# ---- data -------------------------------------------------------------------------------
install(DIRECTORY ${REPO_ROOT}/data/ DESTINATION ${CMAKE_INSTALL_DATADIR}/tubeclock/imagesets
        FILES_MATCHING PATTERN "*.png")
if(TUBECLOCK_FIRMWARE_DIR)
  if(NOT IS_ABSOLUTE "${TUBECLOCK_FIRMWARE_DIR}")
    set(TUBECLOCK_FIRMWARE_DIR "${REPO_ROOT}/${TUBECLOCK_FIRMWARE_DIR}")
  endif()
  if(NOT EXISTS "${TUBECLOCK_FIRMWARE_DIR}/manifest.json")
    message(FATAL_ERROR "No firmware bundle in ${TUBECLOCK_FIRMWARE_DIR} - run scripts/collect_firmware.py")
  endif()
  install(DIRECTORY ${TUBECLOCK_FIRMWARE_DIR}/ DESTINATION ${CMAKE_INSTALL_DATADIR}/tubeclock/firmware)
else()
  message(WARNING "TUBECLOCK_FIRMWARE_DIR not set: the app will be installed without firmware")
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
