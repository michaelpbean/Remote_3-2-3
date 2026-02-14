#include "config.h"
#include "display.h"

// Enum values for stance comparison (these match the main file's enum)
#define TWO_LEG_STANCE 1
#define THREE_LEG_STANCE 2

DisplayManager::DisplayManager()
    : lastColor(-1)
{
    #ifdef USE_WAVESHARE_ESP32_LCD
        tft = new Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
        leds = new CRGB[NUM_LEDS];
        lcdText[0] = '\0';
    #else
        lcd = new Adafruit_RGBLCDShield();
    #endif
}

void DisplayManager::begin()
{
    #ifdef USE_WAVESHARE_ESP32_LCD
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, HIGH);   // Enable backlight

        // Initialize SPI with our pins
        SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

        // Initialize for 172x320
        tft->init(172, 320);
        tft->setRotation(1);

        tft->setTextColor(ST77XX_WHITE, ST77XX_BLUE);
        tft->setTextSize(3); // Text height = 8 * scale

        FastLED.addLeds<WS2812, RGB_PIN, RGB>(leds, NUM_LEDS);
        FastLED.setBrightness(100); // 0–255

        setBacklightColor(BLUE);
        snprintf(lcdText, sizeof(lcdText), "Holme 3-2-3\nv1.0");
        setLCDText(lcdText);
    #else
        lcd->begin(16, 2);
        lcd->setBacklight(BLUE);
        lcd->setCursor(2, 0);
        lcd->print("Holme 2-3-2");
        lcd->setCursor(0, 1);
        lcd->print("by Neil H v1.0");
    #endif
}

void DisplayManager::setBacklightColor(int color)
{
    #ifdef USE_WAVESHARE_ESP32_LCD
        // Only update the screen if the color has actually changed
        if (color != lastColor)
        {
            tft->fillScreen(color);
            tft->setTextColor(ST77XX_WHITE, color);
            lastColor = color;

            // Since the screen is cleared when we change the backlight color, we need to redraw the existing text.
            setLCDText(lcdText);
        }

        leds[0] = CRGB::Black;
        switch (color)
        {
            case OFF:
                leds[0] = CRGB::Black;
                break;
            case RED:
                leds[0] = CRGB::Red;
                break;
            case YELLOW:
                leds[0] = CRGB::Yellow;
                break;
            case GREEN:
                leds[0] = CRGB::Green;
                break;
            case TEAL:
                leds[0] = CRGB::Cyan;
                break;
            case BLUE:
                leds[0] = CRGB::Blue;
                break;
            case VIOLET:
                leds[0] = CRGB::Purple;
                break;
            case WHITE:
                leds[0] = CRGB::White;
                break;
        }
        FastLED.show();
    #else
        lcd->setBacklight(color);
    #endif
}

void DisplayManager::showStatus(int stance, const char* stanceName)
{
    #ifdef USE_WAVESHARE_ESP32_LCD
        if (stance <= 2)
        {
            // Stance is good.
            setBacklightColor(BLUE);
            snprintf(lcdText, sizeof(lcdText), "Status: OK     \n%d: %s", stance, stanceName);
        }
        else
        {
            // Stance is error.
            setBacklightColor(RED);
            snprintf(lcdText, sizeof(lcdText), "Status: Error  \n%d: %s", stance, stanceName);
        }

        setLCDText(lcdText);
    #else
        lcd->setCursor(0, 0);
        lcd->print("Status:"); //7
        if (stance <= 2)
        {
            lcd->setCursor(8, 0);
            // Stance is good.
            lcd->setBacklight(BLUE);
            lcd->print("OK"); // 10
            lcd->setCursor(10, 0);
            lcd->print("    "); // to end
        }
        else
        {
            lcd->setCursor(8, 0);
            // Stance is error.
            lcd->setBacklight(RED);
            lcd->print("Error");
        }
        lcd->setCursor(0, 1);
        lcd->print(stance); // 1 character
        lcd->setCursor(1, 1);
        lcd->print(": "); // 2
        lcd->setCursor(3, 1);
        lcd->print(stanceName);
    #endif
}

void DisplayManager::showTransition(int stanceTarget)
{
    #ifdef USE_WAVESHARE_ESP32_LCD
        setBacklightColor(GREEN);

        if (stanceTarget == TWO_LEG_STANCE)
        {
            snprintf(lcdText, sizeof(lcdText), "Status: Moving  \nGoto Two Legs   ");
        }
        else if (stanceTarget == THREE_LEG_STANCE)
        {
            snprintf(lcdText, sizeof(lcdText), "Status: Moving  \nGoto Three Legs ");
        }
        else
        {
            snprintf(lcdText, sizeof(lcdText), "Status: Moving  \nGoto Unknown    ");
        }

        setLCDText(lcdText);
    #else
        lcd->setCursor(0, 0);
        lcd->print("Status: Moving  ");
        lcd->setBacklight(GREEN);

        lcd->setCursor(0, 1);

        if (stanceTarget == TWO_LEG_STANCE)
        {
            lcd->print("Goto Two Legs   ");
        }
        else if (stanceTarget == THREE_LEG_STANCE)
        {
            lcd->print("Goto Three Legs ");
        }
    #endif
}

#ifdef USE_WAVESHARE_ESP32_LCD
    void DisplayManager::setLCDText(const char* message)
    {
        int cx = 10;
        int cy = 10;
        int lineHeight = 24;
        tft->setCursor(cx, cy);

        const char* lineStart = message;
        while (*lineStart)
        {
            const char* lineEnd = strchr(lineStart, '\n');
            if (lineEnd)
            {
                tft->write(lineStart, lineEnd - lineStart);
                cy += lineHeight;
                tft->setCursor(10, cy);
                lineStart = lineEnd + 1;
            }
            else
            {
                tft->print(lineStart);
                break;
            }
        }
    }
#endif
