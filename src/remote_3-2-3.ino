/*
   3-2-3 Sketch

   This sketch is based on the 3-2-3-Simple Sketch Shared by Kevin Holme for his 3-2-3 system.

   I have taken the sketch and modified it to work with a Rolling code Remote trigger, as an addition/alternate
   to an RC remote.
   This allows an independent system to trigger the 3-2-3 transition.  A 4 button rolling code remote is used.
   The trasnmitter receiver I used is a CHJ-8802
   https://www.ebay.com/itm/4-Channel-Rolling-Code-Remote-Receiver-and-Transmitter-CHJ-8802/163019935605?ssPageName=STRK%3AMEBIDX%3AIT&_trksid=p2057872.m2749.l2649


   Button A on the remote is used to activate the 3-2-3 system remotely. Without that being pressed, no other
   buttons will trigger a transition. This works similar to a master safety switch. The safety automatically
   times out after 30 seconds so you don't accidentally trigger transitions if you walk away.

   Button B triggers a transition to three leg stance.
   Button C triggers a transition to two leg stance.
   Button D is reserved for future use.

   The Default Pins are for a Pro Micro

   The Sabertooth Libraries can be found here:
   https://www.dimensionengineering.com/info/arduino

   We include the i2c stuff so that we can both receive commands on i2c, and also so that we can talk to
   the LED display via i2c and the two gyro/accelerometer units.  This gives us even more positioning data on
   the 2-3-2 transitions, so that we can know more about what is going on.  It may allow us to "auto restore"
   good state if things are not where we expect them to be. (That's advanced ... and TBD)

   Note that there is no need for these additional sensors.  Everything will work with just the 4 limit switches.

   The Sketch Assumes that the Limit Switches are used in NO mode (Normal Open). This means that when the Switch is
   depressed it reads LOW, and will read HIGH when open (or not pressed).

   Things that need to happen:

   When starting a transition, if the expected Limit switches don't release stop! - TBD
   Add a STOP command, so that if the safety is toggled, the sequence stops immediately - DONE
   Convert ShowTime to be a timer, instead of a counter.  Just use the counter directly. - TBD
   Check for over amperage?? - TBD   
*/

// If using a Waveshare ESP32 MCU/LCD, this uses different pins than the Arduino Pro Micro,
// and uses a different library for controlling the LCD.
#define USE_WAVESHARE_ESP32_LCD

// For ESP32, use Serial1 for Sabertooth instead of the USB CDC Serial port
#ifdef ESP32
#define USBCON // Define USBCON to use Serial1 for Sabertooth on ESP32
#endif

#include <USBSabertooth.h>

/////
// Setup the Comms
/////

USBSabertoothSerial C;
USBSabertooth       ST(C, 128);              // Use address 128.

/////
// Control Mode - Rolling Code Remote
/////
#define ENABLE_ROLLING_CODE_TRIGGER

/*

   LCD Display

*/
#ifdef USE_WAVESHARE_ESP32_LCD
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <FastLED.h>
#define TFT_MOSI  6
#define TFT_SCLK  7
#define TFT_CS    14
#define TFT_DC    15
#define TFT_RST   21
#define TFT_BL    22
#define RGB_PIN   8 // GPIO pin where the WS2812 RGB LED is connected
#define NUM_LEDS  1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
CRGB leds[NUM_LEDS];

#else
#define USE_LCD_DISPLAY
#ifdef USE_LCD_DISPLAY
#include "Adafruit_RGBLCDShield.h"
// Initialise the LCD Display
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
#endif
#endif

/////
// Timing Variables
/////
const int StanceInterval = 100;
const int ShowTimeInterval = 100;
unsigned long currentMillis = 0;      // stores the value of millis() in each iteration of loop()
unsigned long PreviousStanceMillis = 0;
unsigned long PreviousShowTimeMillis = 0;
unsigned long ShowTime = 0;


/////
// Define the pins that we use.  Gives a single place to change them if desired.
/////
#ifdef USE_WAVESHARE_ESP32_LCD
// Valid input pins on the Waveshare ESP32 LCD are 0,1,2,3,4,18,19,20,23.  The rest are used for the SD/LCD/UART or not recommended for other reasons.
#define TiltUpPin 0   //Limit switch input pin, Grounded when closed
#define TiltDnPin 1   //Limit switch input pin, Grounded when closed
#define LegUpPin  2   //Limit switch input pin, Grounded when closed
#define LegDnPin  3   //Limit switch input pin, Grounded when closed
#define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
#define ROLLING_CODE_BUTTON_B_PIN 20 // Transition between 2 and 3 legs.
#define ROLLING_CODE_BUTTON_C_PIN 19 // Transition between 3 and 2 legs.
#define ROLLING_CODE_BUTTON_D_PIN 18
#else
#define TiltUpPin 6   //Limit switch input pin, Grounded when closed
#define TiltDnPin 7   //Limit switch input pin, Grounded when closed
#define LegUpPin  8   //Limit switch input pin, Grounded when closed
#define LegDnPin  9   //Limit switch input pin, Grounded when closed
#define ROLLING_CODE_BUTTON_A_PIN 4 // Used as a killswitch / activate button
#define ROLLING_CODE_BUTTON_B_PIN 5 // Transition between 2 and 3 legs.
#define ROLLING_CODE_BUTTON_C_PIN 18 // Transition between 3 and 2 legs.
#define ROLLING_CODE_BUTTON_D_PIN 19
#endif

/////
// Variables to check R2 state for transitions
/////
int TiltUp;
int TiltDn;
int LegUp;
int LegDn;
int Stance;
int StanceTarget;
int previousStance = -1;  // Track previous stance for display updates
char stanceName[16] = "Three Legs.";
bool LegMoving;  // False if leg is at target, True if leg is moving
bool TiltMoving; // False if tilt is at target, True if tilt is moving
int rollCodeA;
int rollCodeB;
int rollCodeC;
int rollCodeD;
bool enableRollCodeTransitions = false;
unsigned long rollCodeTransitionTimeout; // Used to auto disable the enable signal after a set time
#define COMMAND_ENABLE_TIMEOUT 30000 // Default timeout of 30 seconds for performing a transition.
bool killDebugSent = false;

/////
// Let's define some human friendly names for the various stances.
/////
enum Stance
{
    STANCE_NO_TARGET = 0,
    TWO_LEG_STANCE = 1,
    THREE_LEG_STANCE = 2,
    STANCE_ERROR_LEG_UP_TILT_UNKNOWN = 3,
    STANCE_ERROR_LEG_UNKNOWN_TILT_UP = 4,
    STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN = 5,
    STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN = 6,
    STANCE_ERROR_ALL_UNKNOWN = 7,
    STANCE_ERROR_LEG_DOWN_TILT_UP = 8,
    STANCE_ERROR_LEG_UP_TILT_DOWN = 9
};

/////
// Debounce for the Rolling code.
/////
#ifdef ENABLE_ROLLING_CODE_TRIGGER
#define BUTTON_DEBOUNCE_TIME 500
int buttonALastState;
int buttonBLastState;
int buttonCLastState;
int buttonDLastState;
unsigned long buttonATimeout;
unsigned long buttonBTimeout;
unsigned long buttonCTimeout;
unsigned long buttonDTimeout;
#endif

/////
// DEBUG Control
/////
#define DEBUG
#define DEBUG_VERBOSE  // Enable this to see all debug status on the Serial Monitor.

/////
// Setup Debug Print stuff
// This gives me a nice way to enable/disable debug outputs.
/////
#ifdef DEBUG
    #define DEBUG_PRINT_LN(msg)  Serial.println(msg)
    #define DEBUG_PRINT(msg)  Serial.print(msg)
#else
    #define DEBUG_PRINT_LN(msg)
    #define DEBUG_PRINT(msg)
#endif // DEBUG

/*

   LCD Display Settings

*/
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
#elif defined(USE_LCD_DISPLAY)
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

///////////////////////////////////////////////////////////////////////////////
// setLCDText
// Write the text to the Waveshare LCD, handling newlines 
#ifdef USE_WAVESHARE_ESP32_LCD
char LCDText[256];
void SetLCDText(const char* message)
{
    int cx = 10;
    int cy = 10;
    int lineHeight = 24;
    tft.setCursor(cx, cy);

    const char* lineStart = message;
    while (*lineStart)
    {
        const char* lineEnd = strchr(lineStart, '\n');
        if (lineEnd)
        {
            tft.write(lineStart, lineEnd - lineStart);
            cy += lineHeight;
            tft.setCursor(10, cy);
            lineStart = lineEnd + 1;
        }
        else
        {
            tft.print(lineStart);
            break;
        }
    }
}
#endif

///////////////////////////////////////////////////////////////////////////////
void SetBacklightColor(int color)
{
    #ifdef USE_WAVESHARE_ESP32_LCD
    static int lastColor = -1;
    
    // Only update the screen if the color has actually changed
    if (color != lastColor)
    {
        tft.fillScreen(color);
        tft.setTextColor(ST77XX_WHITE, color);
        lastColor = color;

        // Since the screen is cleared when we change the backlight color, we need to redraw the existing text.
        SetLCDText(LCDText);
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
    #elif defined(USE_LCD_DISPLAY)
    lcd.setBacklight(color);
    #endif
}


/*
   Setup

   Basic pin setup to read the various sensors
   Enable the Serial communication to the Sabertooth and Tx/Rx on the Arduino
   Enable the i2c so that we can talk to the Gyro's and Screen.

*/
void setup()
{
    pinMode(TiltUpPin, INPUT_PULLUP);  // Limit Switch for body tilt (upper)
    pinMode(TiltDnPin, INPUT_PULLUP);  // Limit Switch for body tilt (lower)
    pinMode(LegUpPin,  INPUT_PULLUP);  // Limit Switch for leg lift (upper)
    pinMode(LegDnPin,  INPUT_PULLUP);  // Limit Switch for leg lift (lower)

    #ifdef ENABLE_ROLLING_CODE_TRIGGER
    // Rolling Code Remote Pins
    pinMode(ROLLING_CODE_BUTTON_A_PIN, INPUT_PULLUP);  // Rolling code enable/disable pin
    pinMode(ROLLING_CODE_BUTTON_B_PIN, INPUT_PULLUP);  // Pins to trigger transitions.
    pinMode(ROLLING_CODE_BUTTON_C_PIN, INPUT_PULLUP);
    pinMode(ROLLING_CODE_BUTTON_D_PIN, INPUT_PULLUP);
    #endif //ENABLE_ROLLING_CODE_TRIGGER

    SabertoothTXPinSerial.begin(9600); // 9600 is the default baud rate for Sabertooth Packet Serial.
    // The Sabertooth library uses the Serial TX pin to send data to the motor controller

    // Setup the USB Serial Port for debug output.
    #ifdef USE_WAVESHARE_ESP32_LCD
    Serial.begin(115200); // This is the USB Port on the Waveshare ESP32-C6.
    #else
    Serial.begin(9600); // This is the USB Port on a Pro Micro.
    #endif

    // Init the display
    #ifdef USE_WAVESHARE_ESP32_LCD
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);   // Enable backlight

    // Initialize SPI with our pins
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

    // Initialize for 172x320
    tft.init(172, 320);
    tft.setRotation(1);

    //tft.fillScreen(ST77XX_BLUE);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLUE);
    tft.setTextSize(3); // Text height = 8 * scale

    FastLED.addLeds<WS2812, RGB_PIN, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(100); // 0–255

    // Start with LED off
    //leds[0] = CRGB::Black;
    //FastLED.show();

    SetBacklightColor(BLUE);
    snprintf(LCDText, sizeof(LCDText), "Holme 3-2-3\nv1.0");
    SetLCDText(LCDText);

    #elif defined(USE_LCD_DISPLAY)
    lcd.begin(16, 2);
    lcd.setBacklight(BLUE);
    lcd.setCursor(2, 0);
    lcd.print("Holme 2-3-2");
    lcd.setCursor(0, 1);
    lcd.print("by Neil H v1.0");
    #endif

    // Setup the Target as no-target to begin.
    StanceTarget = STANCE_NO_TARGET;
}


/*
   ReadLimitSwitches

   This code will read the signal from the four limit switches installed in the body.
   The Limit switches are expected to be installed in NO Mode (Normal Open) so that
   when the switch is depressed, the signal will be pulled LOW.

*/
void ReadLimitSwitches()
{
    TiltUp = digitalRead(TiltUpPin);
    TiltDn = digitalRead(TiltDnPin);
    LegUp = digitalRead(LegUpPin);
    LegDn = digitalRead(LegDnPin);
}

void ReadRollingCodeTrigger()
{
    #ifdef ENABLE_ROLLING_CODE_TRIGGER

    unsigned long now = millis();
    rollCodeA = digitalRead(ROLLING_CODE_BUTTON_A_PIN);
    rollCodeB = digitalRead(ROLLING_CODE_BUTTON_B_PIN);
    rollCodeC = digitalRead(ROLLING_CODE_BUTTON_C_PIN);
    rollCodeD = digitalRead(ROLLING_CODE_BUTTON_D_PIN);

    // Button A: Toggle rolling code transitions enable/disable.
    // Only triggers on the rising edge (LOW -> HIGH transition).
    if (now >= buttonATimeout)
    {
        if (rollCodeA == HIGH && buttonALastState == LOW)
        {
            buttonATimeout = now + BUTTON_DEBOUNCE_TIME;
            if (!enableRollCodeTransitions)
            {
                enableRollCodeTransitions = true;
                killDebugSent = false;
                rollCodeTransitionTimeout = now + COMMAND_ENABLE_TIMEOUT;
                SetBacklightColor(VIOLET);
                DEBUG_PRINT_LN("Rolling Code Transmitter Transitions Enabled");
            }
            else
            {
                enableRollCodeTransitions = false;
                killDebugSent = false;
                SetBacklightColor(BLUE);
                DEBUG_PRINT_LN("Rolling Code Transmitter Transitions Disabled");
            }
        }
        buttonALastState = rollCodeA;
    }

    // Button B: Transition to three leg stance.
    if (now >= buttonBTimeout)
    {
        if (enableRollCodeTransitions && rollCodeB == HIGH && buttonBLastState == LOW)
        {
            buttonBTimeout = now + BUTTON_DEBOUNCE_TIME;
            StanceTarget = THREE_LEG_STANCE;
            DEBUG_PRINT_LN("Moving to Three Leg Stance.");
        }
        buttonBLastState = rollCodeB;
    }

    // Button C: Transition to two leg stance.
    if (now >= buttonCTimeout)
    {
        if (enableRollCodeTransitions && rollCodeC == HIGH && buttonCLastState == LOW)
        {
            buttonCTimeout = now + BUTTON_DEBOUNCE_TIME;
            StanceTarget = TWO_LEG_STANCE;
            DEBUG_PRINT_LN("Moving to Two Leg Stance.");
        }
        buttonCLastState = rollCodeC;
    }

    // Button D: Reserved for future use.
    if (now >= buttonDTimeout)
    {
        if (enableRollCodeTransitions && rollCodeD == HIGH && buttonDLastState == LOW)
        {
            buttonDTimeout = now + BUTTON_DEBOUNCE_TIME;
            DEBUG_PRINT_LN("Button D Pressed");
        }
        buttonDLastState = rollCodeD;
    }

    #endif
}


/*

   Display

   This will output all debug Variables on the serial monitor if DEBUG_VERBOSE mode is enabled.
   The output can be helpful to verify the limit switch wiring and other inputs prior to installing
   the arduino in your droid.  For normal operation DEBUG_VERBOSE mode should be turned off.

*/
void Display()
{
    // We only output this if DEBUG_VERBOSE mode is enabled.
    #ifdef DEBUG_VERBOSE
    DEBUG_PRINT("Tilt Up       : ");
    TiltUp ? DEBUG_PRINT_LN("Open") : DEBUG_PRINT_LN("Closed");
    DEBUG_PRINT("Tilt Down     : ");
    TiltDn ? DEBUG_PRINT_LN("Open") : DEBUG_PRINT_LN("Closed");
    DEBUG_PRINT("Leg Up        : ");
    LegUp ? DEBUG_PRINT_LN("Open") : DEBUG_PRINT_LN("Closed");
    DEBUG_PRINT("Leg Down      : ");
    LegDn ? DEBUG_PRINT_LN("Open") : DEBUG_PRINT_LN("Closed");
    DEBUG_PRINT("Stance        : ");
    DEBUG_PRINT(Stance); DEBUG_PRINT(": "); DEBUG_PRINT_LN(stanceName);
    DEBUG_PRINT("Stance Target : ");
    DEBUG_PRINT_LN(StanceTarget);
    DEBUG_PRINT("Leg Moving    : ");
    DEBUG_PRINT_LN(LegMoving);
    DEBUG_PRINT("Tilt Moving   : ");
    DEBUG_PRINT_LN(TiltMoving);
    DEBUG_PRINT("Show Time     : ");
    DEBUG_PRINT_LN(ShowTime);
    #endif // DEBUG_VERBOSE

    // Here is the code that will update the LCD Display with the Live System Status
    #ifdef USE_WAVESHARE_ESP32_LCD
    if (Stance <= 2)
    {
        // Stance is good.
        SetBacklightColor(BLUE);
        snprintf(LCDText, sizeof(LCDText), "Status: OK     \n%d: %s", Stance, stanceName);
    }
    else
    {
        // Stance is error.
        SetBacklightColor(RED);
        snprintf(LCDText, sizeof(LCDText), "Status: Error  \n%d: %s", Stance, stanceName);
    }

    SetLCDText(LCDText);

    #elif defined(USE_LCD_DISPLAY)
    //lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Status:"); //7
    if (Stance <= 2)
    {
        lcd.setCursor(8, 0);
        // Stance is good.
        lcd.setBacklight(BLUE);
        lcd.print("OK"); // 10
        lcd.setCursor(10, 0);
        lcd.print("    "); // to end
    }
    else
    {
        lcd.setCursor(8, 0);
        // Stance is error.
        lcd.setBacklight(RED);
        lcd.print("Error");
    }
    lcd.setCursor(0, 1);
    //lcd.print("                ");
    //lcd.setCursor(0,1);
    lcd.print(Stance); // 1 character
    lcd.setCursor(1, 1);
    lcd.print(": "); // 2
    lcd.setCursor(3, 1);
    lcd.print(stanceName);
    #endif
}


/*
   Actual movement commands are here,  when we send the command to move leg down, first it checks the leg down limit switch, if it is closed it
   stops the motor, sets a flag (Moving) and then exits the loop, if it is open the down motor is triggered.
   all 4 work the same way
*/

/*
    MoveLegDn

     Moves the Center leg down.
*/
void MoveLegDn()
{
    // Read the Pin to see where the leg is.
    LegDn = digitalRead(LegDnPin);

    // If the Limit switch is closed, we should stop the motor.
    if (LegDn == LOW)
    {
        ST.motor(1, 0);     // Stop.
        LegMoving = false;   // Record that we are in a good state.
        return;
    }

    // If the switch is open, then we need to move the motor until
    // the switch is closed.
    if (LegDn == HIGH)
    {
        ST.motor(1, 1024);  // Go forward at half power.
    }
}

/*
    MoveLegUp

    Moves the Center leg up.
*/
void MoveLegUp()
{
    // Read the Pin to see where the leg is.
    LegUp = digitalRead(LegUpPin);

    // If the Limit switch is closed, we should stop the motor.
    if (LegUp == LOW)
    {
        ST.motor(1, 0);     // Stop.
        LegMoving = false;   // Record that we are in a good state.
        return;
    }

    // If the switch is open, then we need to move the motor until
    // the switch is closed.
    if (LegUp == HIGH)
    {
        ST.motor(1, -2047);  // Go backwards at full power.
    }
}

/*
    MoveTiltDn

    Rotates the body toward 18 degrees for a 3 leg stance.
*/
void MoveTiltDn()
{
    // Read the Pin to see where the body tilt is.
    TiltDn = digitalRead(TiltDnPin);

    // If the Limit switch is closed, we should stop the motor.
    if (TiltDn == LOW)
    {
        ST.motor(2, 0);     // Stop.
        TiltMoving = false;  // Record that we are in a good state.
        return;
    }

    // If the switch is open, then we need to move the motor until
    // the switch is closed.
    if (TiltDn == HIGH)
    {
        ST.motor(2, 2047);  // Go forward at full power.
    }
}

/*
    MoveTiltUp

    Rotates the body toward 0 degrees for a 2 leg stance.
*/
void MoveTiltUp()
{
    // Read the Pin to see where the body tilt is.
    TiltUp = digitalRead(TiltUpPin);

    // If the Limit switch is closed, we should stop the motor.
    if (TiltUp == LOW)
    {
        ST.motor(2, 0);     // Stop.
        TiltMoving = false;  // Record that we are in a good state.
        return;
    }

    // If the switch is open, then we need to move the motor until
    // the switch is closed.
    if (TiltUp == HIGH)
    {
        ST.motor(2, -2047);  // Go forward at full power.
    }
}

/*
   displayTransition

   This function will update the LCD display (if enabled)
   during the 2-3-2 transition.

*/
void displayTransition()
{
    #ifdef USE_WAVESHARE_ESP32_LCD
    SetBacklightColor(GREEN);

    if (StanceTarget == TWO_LEG_STANCE)
    {
        snprintf(LCDText, sizeof(LCDText), "Status: Moving  \nGoto Two Legs   ");
    }
    else if (StanceTarget == THREE_LEG_STANCE)
    {
        snprintf(LCDText, sizeof(LCDText), "Status: Moving  \nGoto Three Legs ");
    }
    else
    {
        snprintf(LCDText, sizeof(LCDText), "Status: Moving  \nGoto Unknown    ");
    }

    SetLCDText(LCDText);
    #elif defined(USE_LCD_DISPLAY)
    lcd.setCursor(0, 0);
    lcd.print("Status: Moving  ");
    lcd.setBacklight(GREEN);

    lcd.setCursor(0, 1);

    if (StanceTarget == TWO_LEG_STANCE)
    {
        lcd.print("Goto Two Legs   ");
    }
    else if (StanceTarget == THREE_LEG_STANCE)
    {
        lcd.print("Goto Three Legs ");
    }
    #endif
}

/*
   TwoToThree

   this command to go from two legs to to three, ended up being a combo of tilt down and leg down
   with a last second check each loop on the limit switches.
   timing worked out great, by the time the tilt down needed a center foot, it was there.
*/
void TwoToThree()
{
    // Read the pin positions to check if they are both down
    // before we start moving anything.
    TiltDn = digitalRead(TiltDnPin);
    LegDn = digitalRead(LegDnPin);

    DEBUG_PRINT_LN("  Moving to Three Legs  ");
    displayTransition();

    // If the leg is already down, then we are done.
    if (LegDn == LOW)
    {
        ST.motor(1, 0);    // Stop
        LegMoving = false;  // Record that we are in a good state.
    }
    else if (LegDn == HIGH)
    {
        // If the leg is not down, move the leg motor at full power.
        ST.motor(1, 1024);  // Go forward at half power.
    }

    // If the Body is already tilted, we are done.
    if (TiltDn == LOW)
    {
        ST.motor(2, 0);     // Stop
        TiltMoving = false;  // Record that we are in a good state.
    }
    else if (TiltDn == HIGH)
    {
        // If the body is not tilted, move the tilt motor at full power.
        ST.motor(2, 2047);  // Go forward at full power.
    }
}


/*
   ThreeToTwo

   going from three legs to two needed a slight adjustment. I start a timer, called show time, and use it to
   delay the center foot from retracting.

   In the future, the gyro can be used to start the trigger of the leg lift once the leg/body angle gets to
   a point where the center of mass is close enough to start the retraction.
*/

void ThreeToTwo()
{
    // Read the limit switches to see where we are.
    TiltUp = digitalRead(TiltUpPin);
    LegUp = digitalRead(LegUpPin);
    TiltDn = digitalRead(TiltDnPin);

    DEBUG_PRINT_LN("  Moving to Two Legs  ");
    displayTransition();

    // First if the center leg is up, do nothing.
    if (LegUp == LOW)
    {
        ST.motor(1, 0);    // Stop
        LegMoving = false;  // Record that we are in a good state.
    }

    // TODO:  Convert the counters to just use a timer.
    // If leg up is open AND the timer is in the first 20 steps then lift the center leg at 25 percent speed
    // The intent here is to move the leg slowly so that it pushes the body up until we have reached the balance
    // point for two leg stance.  After that point we can pull the leg up quickly.
    if (LegUp == HIGH &&  ShowTime >= 1 && ShowTime <= 10)
    {
        ST.motor(1, -250);
    }

    //  If leg up is open AND the timer is over 12 steps then lift the center leg at full speed
    if (LegUp == HIGH && ShowTime >= 12)
    {
        ST.motor(1, -2047);
    }

    // at the same time, tilt up till the switch is closed
    if (TiltUp == LOW)
    {
        ST.motor(2, 0);     // Stop
        TiltMoving = false;  // Record that we are in a good state.
    }
    if (TiltUp == HIGH)
    {
        ST.motor(2, -2047);  // Go backward at full power.
    }
}


/*
   Stance Error Code Legend:
   L = Leg position
   T = Tilt position
   U = Up (limit switch closed/pressed)
   D = Down (limit switch closed/pressed)
   ? = Unknown (limit switch open/not pressed)

   Example: "LUT?" means Leg is Up, Tilt is Unknown
*/

/*
   CheckStance

   This is simply taking all of the possibilities of the switch positions and giving them a number.
   The loop only runs when both *Moving flags are false, meaning that it does not run in the middle of
   a transition.

   At any time, including power up, the droid can run a check and come up with a number as to how he is standing.

   The Display code will output the stance number, as well as a code to tell you in a nice human readable format
   what is going on within the droid, so you don't need to physically check (or if there's a problem you can check
   to see why something is not reading correctly.

   The codes are
   L = Leg
   T = Tilt
   U = Up
   D = Down
   ? = Unknown (Neither up nor down)

   so L?TU means that the Tilt is up (two leg stance) but the Leg position is unknown.
   Leg is always reported first, Tilt second.

 */
void CheckStance()
{
    // We only do this if the leg and tilt are NOT moving.
    if (LegMoving == false && TiltMoving == false)
    {
        // Center leg is up, and the body is straight.  This is a 2 leg Stance.
        if (LegUp == LOW && LegDn == HIGH && TiltUp == LOW && TiltDn == HIGH)
        {
            Stance = TWO_LEG_STANCE;
            strcpy(stanceName, "Two Legs.    "); // LUTU
            return;
        }

        // Center leg is down, Body is tilted.  This is a 3 leg stance
        if (LegUp == HIGH && LegDn == LOW && TiltUp == HIGH && TiltDn == LOW)
        {
            Stance = THREE_LEG_STANCE;
            strcpy(stanceName, "Three Legs   "); // LDTD
            return;
        }

        // Leg is up.  Body switches are open.
        // The body is somewhere between straight and tilted.
        if (LegUp == LOW && LegDn == HIGH && TiltUp == HIGH && TiltDn == HIGH)
        {
            Stance = STANCE_ERROR_LEG_UP_TILT_UNKNOWN;
            strcpy(stanceName, "Error - LUT?");
        }

        // Leg switches are both open.  The leg is somewhere between up and down
        // Body is straight for a 2 leg stance.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == LOW && TiltDn == HIGH)
        {
            Stance = STANCE_ERROR_LEG_UNKNOWN_TILT_UP;
            strcpy(stanceName, "Error - L?TU");
        }

        // Leg is down, and the body is straight for a 2 leg stance
        // The droid is balanced on the center foot. (Probably about to fall over)
        if (LegUp == HIGH && LegDn == LOW && TiltUp == LOW && TiltDn == HIGH)
        {
            Stance = STANCE_ERROR_LEG_DOWN_TILT_UP;
            strcpy(stanceName, "Error - LDTU");
        }

        // Leg is down.  Body switches are both open.
        // The body is somewhere between straight and 18 degrees
        if (LegUp == HIGH && LegDn == LOW && TiltUp == HIGH && TiltDn == HIGH)
        {
            Stance = STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN;
            strcpy(stanceName, "Error - LDT?");
        }

        // Leg switches are both open.
        // Body is tilted for a 3 leg stance.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == HIGH && TiltDn == LOW)
        {
            Stance = STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN;
            strcpy(stanceName, "Error - L?TD");
        }

        // All 4 limit switches are open.  No idea where we are.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == HIGH && TiltDn == HIGH)
        {
            Stance = STANCE_ERROR_ALL_UNKNOWN;
            strcpy(stanceName, "Error - L?T?");
        }

        // Leg is up, body is tilted.  This should not be possible.  If it happens, we are in a bad state.
        if (LegUp == LOW && LegDn == HIGH && TiltUp == HIGH && TiltDn == LOW)
        {
            Stance = STANCE_ERROR_LEG_UP_TILT_DOWN;
            strcpy(stanceName, "Error - LUTD");
        }
    }
}

/*
   EmergencyStop

   If we need to stop everything, this will do it!

*/
void EmergencyStop()
{
    ST.motor(1, 0);
    ST.motor(2, 0);
    LegMoving = false;
    TiltMoving = false;

    // Setting StanceTarget to STANCE_NO_TARGET ensures we don't try to restart movement.
    StanceTarget = STANCE_NO_TARGET;

    DEBUG_PRINT_LN("Emergency Stop.");
}

/*
   Each time through the loop this function is called.
   This checks the killswitch status, and if the killswitch is
   toggled, such that we disable the 2-3-2 system this code will
   stop all motors where they are.

   NOTE:  This could lead to a faceplant.  The expectation is that if
   the user has hit the killswitch, it's because something went wrong.
   This is here for safety.

*/
void checkKillSwitch()
{
    bool stopMotors = false;

    #ifdef ENABLE_ROLLING_CODE_TRIGGER
    if (!enableRollCodeTransitions)
    {
        stopMotors = true;
    }
    #endif

    if (stopMotors && !killDebugSent)
    {
        DEBUG_PRINT_LN("Killswitch activated.  Stopping Motors!");
        killDebugSent = true;
        EmergencyStop();
    }
}

/*
   Move

    This function does the checking of the requested stance (StanceTarget) and the current
    stance (Stance) based on reading the limit switches ane figuring out where we are.

    If we are in a good state, then we will trigger the relevant transition.

    This also tries to move to a known good state where it may be possible.  In certain cases
    there are "unknown" states where the requested target may be possible to make a move without
    creating a bigger mess.

    e.g. Request going to two legs, but center leg position is unknown tilt motor is up.
    In this case, we can try to lift the center leg.  We're either already in a heap, or we're
    already on two legs, and the center lift just isn't all the way up.  It's safe, or at best won't
    make anything worse!

    This is just one example, but there's a couple of cases like this where we can try to complete
    the request.

    In the rest of the cases, if we can't move safely, we wont.

 */
void Move()
{
    // there is no stance target 0, so turn off your motors and do nothing.
    if (StanceTarget == STANCE_NO_TARGET)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // if you are told to go where you are, then do nothing
    if (StanceTarget == Stance)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // Stance 7 is bad, all 4 switches open, no idea where anything is.  do nothing.
    if (Stance == STANCE_ERROR_ALL_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // if you are in three legs and told to go to 2
    if (StanceTarget == TWO_LEG_STANCE && Stance == THREE_LEG_STANCE)
    {
        LegMoving = true;
        TiltMoving = true;
        ThreeToTwo();
    }
    // This is the first of the slight unknowns, target is two legs,  look up to stance 3, the center leg is up, but the tilt is unknown.
    //You are either standing on two legs, or already in a pile on the ground. Cant hurt to try tilting up.
    if (StanceTarget == TWO_LEG_STANCE && Stance == STANCE_ERROR_LEG_UP_TILT_UNKNOWN)
    {
        TiltMoving = true;
        MoveTiltUp();
    }
    // target two legs, tilt is up, center leg unknown, Can not hurt to try and lift the leg again.
    if (StanceTarget == TWO_LEG_STANCE && ((Stance == STANCE_ERROR_LEG_UNKNOWN_TILT_UP) || (Stance == STANCE_ERROR_LEG_DOWN_TILT_UP)))
    {
        LegMoving = true;
        MoveLegUp();
    }
    //Target is two legs, center foot is down, tilt is unknown, too risky do nothing.
    if (StanceTarget == TWO_LEG_STANCE && Stance == STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is two legs, tilt is down, center leg is unknown,  too risky, do nothing.
    if (StanceTarget == TWO_LEG_STANCE && Stance == STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is three legs, stance is two legs, run two to three.
    if (StanceTarget == THREE_LEG_STANCE && Stance == TWO_LEG_STANCE)
    {
        LegMoving = true;
        TiltMoving = true;
        TwoToThree();
    }
    //Target is three legs. center leg is up, tilt is unknown, safer to do nothing, Recover from stance 3 with the up command
    if (StanceTarget == THREE_LEG_STANCE && Stance == STANCE_ERROR_LEG_UP_TILT_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is three legs, but don't know where the center leg is.   Best to not try this,
    // recover from stance 4 with the up command,
    if (StanceTarget == THREE_LEG_STANCE && Stance == STANCE_ERROR_LEG_UNKNOWN_TILT_UP)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // Target is three legs, the center foot is down, tilt is unknown. either on 3 legs now, or a smoking mess,
    // nothing to lose in trying to tilt down again
    if (StanceTarget == THREE_LEG_STANCE && Stance == STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN)
    {
        TiltMoving = true;
        MoveTiltDn();
    }
    // kinda like above, Target is 3 legs, tilt is down, center leg is unknown, ......got nothing to lose.
    if (StanceTarget == THREE_LEG_STANCE && Stance == STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN)
    {
        LegMoving = true;
        MoveLegDn();
    }
}


 /*
    loop

    The main processing loop.

    Each time through we check
    1. The killswitch has not been triggered to stop everything.
    2. Read the RC or Rolling code inputs, and the limit switches if the timer has passed (101ms)
    3. Display Debug info / update the LCD display if the timer has expired (5 seconds default)
    4. Check the current stance, based on the limit switch input read above. (100ms default)
    5. Check if the killswitch timeout (Rolling code and Serial comms) has expired (30 seconds default) and disable.
    6. Move based on the inputs
    7. If we have reached the target stance, reset the Target so we don't try moving again if a switch toggles.
    8. Update the timer used to change the center leg lift speed.  (Soon to be replaced with a true timer, not a counter)

 */
void loop()
{
    currentMillis = millis();  // this updates the current time each loop

    // Want to look closely at this.  I think this will reset the ShowTime every time though the loop
    // when the switch is open.  Probably not what was intended!
    if (TiltDn == LOW)
    {
        // when the tilt down switch opens, the timer starts
        ShowTime = 0;
    }

    // Regardless of time passed, we check the killswitch on every loop.
    // If the killswitch has been turned off (to kill the 2-3-2 system)
    // We will stop the motors, regardless of what is being done.
    // The assumption is that if you hit the killswitch, it was for a good reason.
    //checkKillSwitch();

    // Read rolling code buttons and limit switches every loop iteration.
    // These are instant digitalRead() calls and should not be throttled.
    ReadRollingCodeTrigger(); // Only does something if ENABLE_ROLLING_CODE_TRIGGER is defined.
    ReadLimitSwitches();

    if (currentMillis - PreviousStanceMillis >= StanceInterval)
    {
        PreviousStanceMillis = currentMillis;
        CheckStance();
    }

    // Update LCD immediately when stance changes
    if (Stance != previousStance)
    {
        previousStance = Stance;
        Display();
    }

    // Check if the rolling code transition timeout has expired.
    if (enableRollCodeTransitions && (currentMillis >= rollCodeTransitionTimeout))
    {
        // We have exceeded the time to do a transition start.
        // Auto Disable the safety so we don't accidentally trigger the transition.
        enableRollCodeTransitions = false;
        SetBacklightColor(BLUE);
        //DEBUG_PRINT_LN("Warning: Transition Enable Timeout reached.  Disabling Rolling Code Transitions.");
    }

    Move();

    // Once we have moved, check to see if we've reached the target.
    // If we have then we reset the Target, so that we don't keep
    // trying to move motors (This was a bug found in testing!)
    if (Stance == StanceTarget)
    {
        // Transition complete!
        StanceTarget = STANCE_NO_TARGET;
        DEBUG_PRINT_LN("Transition Complete");
    }

    // the following lines triggers my showtime timer to advance one number every 100ms.
    //I find it easier to work with a smaller number, and it is all trial and error anyway.
    if (currentMillis - PreviousShowTimeMillis >= ShowTimeInterval)
    {
        PreviousShowTimeMillis = currentMillis;
        ShowTime++;
        //DEBUG_PRINT("Showtime: ");DEBUG_PRINT_LN(ShowTime);
    }
}