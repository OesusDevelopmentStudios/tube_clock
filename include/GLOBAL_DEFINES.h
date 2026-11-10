/*
 * Tube Clock firmware - hardware definitions.
 *
 * Pin mappings are taken from EleksTubeHAX (https://github.com/aly-fly/EleksTubeHAX),
 * by Mark Smith (SmittyHalibut), Aljaz Ogrin (aly-fly), Martinius79 and contributors.
 *
 * This file is also copied into lib/modified_TFT_eSPI as User_Setup.h by
 * scripts/script_configure_tft_lib.py, so it configures the display driver too.
 * The hardware variant is selected by the PlatformIO environment (-D HARDWARE_...).
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef GLOBAL_DEFINES_H_
#define GLOBAL_DEFINES_H_

#include <Arduino.h>
#include <stdint.h>

#define VALUE(string) #string
#define TO_LITERAL(string) VALUE(string)
#define FIRMWARE_VERSION TO_LITERAL(BUILDVER)

#ifdef HARDWARE_PUNKCYBER_CLOCK
// Same board as the EleksTube, but the tubes are wired in reversed order.
#define HARDWARE_ELEKSTUBE_CLOCK
#endif

#if !defined(HARDWARE_ELEKSTUBE_CLOCK) && !defined(HARDWARE_ELEKSTUBE_GEN2_CLOCK) && !defined(HARDWARE_SI_HAI_CLOCK)
#error "No clock hardware selected. Build with one of the PlatformIO environments."
#endif

// ************* Tubes *************
#define NUM_DIGITS (6)

// Physical index of a tube as used by the chip select shift register and the LED string.
// Index 0 is the bit/LED the hardware addresses first; the firmware only uses
// "logical screens" (0 = leftmost tube) and converts with SCREEN_TO_DIGIT().
#ifdef HARDWARE_PUNKCYBER_CLOCK
#define SCREEN_TO_DIGIT(screen) (screen)
#else
#define SCREEN_TO_DIGIT(screen) (NUM_DIGITS - 1 - (screen))
#endif

// Display power MOSFET levels (shift register clocks).
#define ACTIVATEDISPLAYS HIGH
#define DEACTIVATEDISPLAYS LOW

/************************
 *    EleksTube IPS     *
 ************************/
#if defined(HARDWARE_ELEKSTUBE_CLOCK)
#ifdef HARDWARE_PUNKCYBER_CLOCK
#define DEVICE_MODEL "PunkCyber"
#define DEVICE_MODEL_NAME "PunkCyber IPS Tube Clock"
#else
#define DEVICE_MODEL "EleksTube"
#define DEVICE_MODEL_NAME "EleksTube IPS Tube Clock"
#endif

#define BACKLIGHTS_PIN (12)
#define NUM_BACKLIGHT_LEDS (6)

// Buttons, active low, externally pulled up.
#define BUTTON_LEFT_PIN (33)
#define BUTTON_MODE_PIN (32)
#define BUTTON_RIGHT_PIN (35)
#define BUTTON_POWER_PIN (34)

// I2C to DS3231 RTC.
#define HAS_RTC_DS3231
#define RTC_SCL_PIN (22)
#define RTC_SDA_PIN (21)

// Chip select shift register (74HC595).
#define CSSR_DATA_PIN (14)
#define CSSR_CLOCK_PIN (16)
#define CSSR_LATCH_PIN (17)

#define TFT_ENABLE_PIN (27)

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_DC 25
#define TFT_RST 26
#endif // HARDWARE_ELEKSTUBE_CLOCK

/*************************
 *    EleksTube Gen2     *
 *************************/
#if defined(HARDWARE_ELEKSTUBE_GEN2_CLOCK)
#define DEVICE_MODEL "EleksTube_Gen2"
#define DEVICE_MODEL_NAME "EleksTube IPS Tube Clock Gen2"

#define BACKLIGHTS_PIN (12)
#define NUM_BACKLIGHT_LEDS (6)

#define BUTTON_LEFT_PIN (33)
#define BUTTON_MODE_PIN (32)
#define BUTTON_RIGHT_PIN (35)
#define BUTTON_POWER_PIN (34)

#define HAS_RTC_DS3231
#define RTC_SCL_PIN (22)
#define RTC_SDA_PIN (21)

#define CSSR_DATA_PIN (14)
#define CSSR_CLOCK_PIN (9)
#define CSSR_LATCH_PIN (10)

#define TFT_ENABLE_PIN (27)

#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_DC 25
#define TFT_RST 26
#endif // HARDWARE_ELEKSTUBE_GEN2_CLOCK

/***********************
 *    Si Hai Clone     *
 ***********************/
#if defined(HARDWARE_SI_HAI_CLOCK)
#define DEVICE_MODEL "SI_HAI"
#define DEVICE_MODEL_NAME "Si Hai IPS Tube Clock"

#define BACKLIGHTS_PIN (32)
#define NUM_BACKLIGHT_LEDS (6)

#define BUTTON_LEFT_PIN (35)
#define BUTTON_MODE_PIN (34)
#define BUTTON_RIGHT_PIN (39)
#define BUTTON_POWER_PIN (36)

// 3-wire DS1302 RTC.
#define HAS_RTC_DS1302
#define DS1302_SCLK (33)
#define DS1302_IO (25)
#define DS1302_CE (26)

#define CSSR_DATA_PIN (4)
#define CSSR_CLOCK_PIN (22)
#define CSSR_LATCH_PIN (21)

#define TFT_ENABLE_PIN (2)

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS -1
#define TFT_DC 16
#define TFT_RST 23
#endif // HARDWARE_SI_HAI_CLOCK

// ************* TFT_eSPI configuration (all supported clocks) *************
// ST7789 135 x 240, no per-display chip select pin (selected via the shift register).
#define ST7789_DRIVER
#define TFT_WIDTH 135
#define TFT_HEIGHT 240
#define CGRAM_OFFSET
#define TFT_SDA_READ

#define LOAD_FONT2 // 16 px, menu details
#define LOAD_FONT4 // 26 px, menu titles and legend
#define LOAD_FONT6 // 48 px digits, menu values
#define LOAD_FONT7 // 48 px 7-segment digits, fallback clock face
#define SMOOTH_FONT

#define SPI_FREQUENCY 40000000

#define DISABLE_ALL_LIBRARY_WARNINGS
#define USER_SETUP_LOADED // stop TFT_eSPI from loading its own setup

#endif // GLOBAL_DEFINES_H_
