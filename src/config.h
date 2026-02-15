#ifndef CONFIG_H
#define CONFIG_H

///////////////////////////////////////////////////////////////////////////////
// Hardware Configuration
// Choose ONE of the following hardware configurations:
// Option 1: Waveshare ESP32-C6-LCD-1.47 (172x320 TFT, ST7789, 1x WS2812 LED)
#define USE_WAVESHARE_ESP32_C6_LCD

// Option 2: Waveshare ESP32-S3-LCD-1.9 (170x320 TFT, ST7789, 2x WS2812 LEDs)
//#define USE_WAVESHARE_ESP32_S3_LCD

// Option 3: Arduino Pro Micro + Adafruit RGB LCD Shield (16x2 character LCD)
//#define USE_ARDUINO_PRO_MICRO

// Define helper macro for any Waveshare ESP32 LCD board
#if defined(USE_WAVESHARE_ESP32_C6_LCD) || defined(USE_WAVESHARE_ESP32_S3_LCD)
    #define USE_WAVESHARE_ESP32_LCD
#endif

///////////////////////////////////////////////////////////////////////////////
// Pin Definitions
#ifdef USE_WAVESHARE_ESP32_C6_LCD
    // Valid input pins on the Waveshare ESP32-C6 LCD are 0,1,2,3,4,18,19,20,23.
    // The rest are used for the SD/LCD/UART or not recommended for other reasons.
    #define TiltUpPin 0   //Limit switch input pin, Grounded when closed
    #define TiltDnPin 1   //Limit switch input pin, Grounded when closed
    #define LegUpPin  2   //Limit switch input pin, Grounded when closed
    #define LegDnPin  3   //Limit switch input pin, Grounded when closed
    #define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
    #define ROLLING_CODE_BUTTON_B_PIN 20 // Transition between 2 and 3 legs.
    #define ROLLING_CODE_BUTTON_C_PIN 19 // Transition between 3 and 2 legs.
    #define ROLLING_CODE_BUTTON_D_PIN 18

    // TFT display pin definitions for Waveshare ESP32-C6-LCD-1.47
    #define TFT_MOSI  6
    #define TFT_SCLK  7
    #define TFT_CS    14
    #define TFT_DC    15
    #define TFT_RST   21
    #define TFT_BL    22

    // RGB LED configuration for Waveshare ESP32-C6-LCD-1.47
    #define NUM_LEDS  1
    #define RGB_PIN   8 // GPIO pin where the WS2812 RGB LED is connected
#elif defined(USE_WAVESHARE_ESP32_S3_LCD)
    // Waveshare ESP32-S3-LCD-1.9 pin assignments
    // Valid input pins on the Waveshare ESP32-S3 LCD are 1,2,3,5,6,15,16,17,18,21,42,45,46.
    // The rest are used for the SD/LCD/UART/IMU or not recommended for other reasons.    
    #define TiltUpPin 1   //Limit switch input pin, Grounded when closed
    #define TiltDnPin 2   //Limit switch input pin, Grounded when closed
    #define LegUpPin  3   //Limit switch input pin, Grounded when closed
    #define LegDnPin  5   //Limit switch input pin, Grounded when closed
    #define ROLLING_CODE_BUTTON_A_PIN 15 // Used as a killswitch / activate button
    #define ROLLING_CODE_BUTTON_B_PIN 16 // Transition between 2 and 3 legs.
    #define ROLLING_CODE_BUTTON_C_PIN 17 // Transition between 3 and 2 legs.
    #define ROLLING_CODE_BUTTON_D_PIN 18 // Reserved for future use

    // TFT display pin definitions for Waveshare ESP32-S3-LCD-1.9
    #define TFT_MOSI  13   // LCD_DIN
    #define TFT_SCLK  10   // LCD_CLK
    #define TFT_CS    12   // LCD_CS
    #define TFT_DC    11   // LCD_DC
    #define TFT_RST   9    // LCD_RST
    #define TFT_BL    14   // LCD_BL

    // RGB LED configuration for Waveshare ESP32-S3-LCD-1.9
    #define NUM_LEDS  2  // Two WS2812 LEDs on the back of the board
    #define RGB_PIN   15   // GPIO pin where the WS2812 RGB LEDs are connected
#elif defined(USE_ARDUINO_PRO_MICRO)
    // Arduino Pro Micro pins
    #define TiltUpPin 6   //Limit switch input pin, Grounded when closed
    #define TiltDnPin 7   //Limit switch input pin, Grounded when closed
    #define LegUpPin  8   //Limit switch input pin, Grounded when closed
    #define LegDnPin  9   //Limit switch input pin, Grounded when closed
    #define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
    #define ROLLING_CODE_BUTTON_B_PIN 5 // Transition between 2 and 3 legs.
    #define ROLLING_CODE_BUTTON_C_PIN 18 // Transition between 3 and 2 legs.
    #define ROLLING_CODE_BUTTON_D_PIN 19
#else
    #error "No hardware configuration selected! Please define USE_WAVESHARE_ESP32_C6_LCD, USE_WAVESHARE_ESP32_S3_LCD, or USE_ARDUINO_PRO_MICRO in config.h"
#endif

#endif // CONFIG_H
