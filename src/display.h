#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

// Forward declarations for hardware-specific types
#ifdef USE_WAVESHARE_ESP32_LCD
    #include <SPI.h>
    #include <Adafruit_GFX.h>
    #include <Adafruit_ST7789.h>
    #include <FastLED.h>
#else
    #include "Adafruit_RGBLCDShield.h"
#endif

/////
// LCD Display Color Constants
/////
#ifndef OFF
    #ifdef USE_WAVESHARE_ESP32_LCD
        #define OFF ST77XX_BLACK
        #define RED ST77XX_RED
        #define YELLOW ST77XX_YELLOW
        #define GREEN ST77XX_GREEN
        #define TEAL ST77XX_CYAN
        #define BLUE ST77XX_BLUE
        #define VIOLET ST77XX_MAGENTA
        #define WHITE ST77XX_WHITE
    #else
        #define OFF 0x0
        #define RED 0x1
        #define YELLOW 0x3
        #define GREEN 0x2
        #define TEAL 0x6
        #define BLUE 0x4
        #define VIOLET 0x5
        #define WHITE 0x7
    #endif
#endif

class DisplayManager
{
    public:
        DisplayManager();

        // Initialize display hardware
        void begin();

        // Set backlight color (and RGB LED if Waveshare)
        void setBacklightColor(int color);

        // Display current stance status
        void showStatus(int stance, const char* stanceName);

        // Display transition message
        void showTransition(int stanceTarget);

    private:
        #ifdef USE_WAVESHARE_ESP32_LCD
            // Waveshare ESP32 display objects
            Adafruit_ST7789* tft;
            CRGB* leds;
            char lcdText[256];

            // Internal text setter for Waveshare
            void setLCDText(const char* message);
        #else
            // Arduino Pro Micro LCD shield object
            Adafruit_RGBLCDShield* lcd;
        #endif

        int lastColor;
};

#endif // DISPLAY_H
