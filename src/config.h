#ifndef CONFIG_H
#define CONFIG_H

// Hardware Configuration
// If using a Waveshare ESP32 MCU/LCD, this uses different pins than the Arduino Pro Micro,
// and uses a different library for controlling the LCD.
#define USE_WAVESHARE_ESP32_LCD

// For ESP32, use Serial1 for Sabertooth instead of the USB CDC Serial port
#ifdef ESP32
    #define USBCON // Define USBCON to use Serial1 for Sabertooth on ESP32
#endif

/////
// Pin Definitions
// Define the pins that we use. Gives a single place to change them if desired.
/////

#ifdef USE_WAVESHARE_ESP32_LCD
    // Valid input pins on the Waveshare ESP32 LCD are 0,1,2,3,4,18,19,20,23.
    // The rest are used for the SD/LCD/UART or not recommended for other reasons.
    #define TiltUpPin 0   //Limit switch input pin, Grounded when closed
    #define TiltDnPin 1   //Limit switch input pin, Grounded when closed
    #define LegUpPin  2   //Limit switch input pin, Grounded when closed
    #define LegDnPin  3   //Limit switch input pin, Grounded when closed
    #define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
    #define ROLLING_CODE_BUTTON_B_PIN 20 // Transition between 2 and 3 legs.
    #define ROLLING_CODE_BUTTON_C_PIN 19 // Transition between 3 and 2 legs.
    #define ROLLING_CODE_BUTTON_D_PIN 18

    // TFT display pin definitions for Waveshare ESP32-C6-LCD
    #define TFT_MOSI  6
    #define TFT_SCLK  7
    #define TFT_CS    14
    #define TFT_DC    15
    #define TFT_RST   21
    #define TFT_BL    22

    // RGB LED configuration for Waveshare ESP32-C6-LCD
    #define NUM_LEDS  1
    #define RGB_PIN 8 // GPIO pin where the WS2812 RGB LED is connected
#else
    // Arduino Pro Micro pins
    #define TiltUpPin 6   //Limit switch input pin, Grounded when closed
    #define TiltDnPin 7   //Limit switch input pin, Grounded when closed
    #define LegUpPin  8   //Limit switch input pin, Grounded when closed
    #define LegDnPin  9   //Limit switch input pin, Grounded when closed
    #define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
    #define ROLLING_CODE_BUTTON_B_PIN 5 // Transition between 2 and 3 legs.
    #define ROLLING_CODE_BUTTON_C_PIN 18 // Transition between 3 and 2 legs.
    #define ROLLING_CODE_BUTTON_D_PIN 19
#endif

#endif // CONFIG_H
