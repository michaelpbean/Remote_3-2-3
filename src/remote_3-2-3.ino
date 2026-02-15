/*
    3-2-3 Sketch

    This sketch is based on the 3-2-3-Simple Sketch Shared by Kevin Holme for his 3-2-3 system, and
    modified by Neil H to work with a rolling code remote trigger instead of an RC remote.

    Kevin and Neil's version used an Arduino Pro Micro and an Adafruit RGB LCD Shield for the display.

    I've (Michael Bean) modified Neil's version to work with a Waveshare ESP32-C6-LCD which has a built in TFT display
    and RGB LED, so the code is a bit different to support both types of displays. I also used a Qiachip RX480E for
    the rolling code remote receiver, which has a slightly different pinout but that's handled by the PCB and
    no changes to the code here are needed to handle it.

    ----------------------------------------------------------------------------
    Neil's original comments are included below:

    I have taken the sketch and modified it to work with a Rolling code Remote trigger, as an addition/alternate
    to an RC remote.
    This allows an independent system to trigger the 3-2-3 transition.  A 4 button rolling code remote is used.
    The transmitter receiver I used is a CHJ-8802
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

#include "config.h"
#include "display.h"
#include <USBSabertooth.h>

///////////////////////////////////////////////////////////////////////////////
// Sabertooth setup
// On ESP32 with USB CDC on boot, Serial = USB and Serial0 = UART0 (TX0/RX0 pins).
// On Pro Micro (ATmega32U4), Serial = USB and Serial1 = the only hardware UART.
#ifdef USE_WAVESHARE_ESP32_LCD
    USBSabertoothSerial C(Serial0);
#else
    USBSabertoothSerial C(Serial1);
#endif
USBSabertooth       ST(C, 128);              // Use address 128.

// Control Mode - Rolling Code Remote
// NOTE - RC and serial control modes removed to simplify the code and avoid confusion. The rolling code remote is the only control mode supported in this version of the sketch.
#define ENABLE_ROLLING_CODE_TRIGGER

// Display Manager
DisplayManager display;

// Timing Variables
const int StanceInterval = 100;
const int ShowTimeInterval = 100;
unsigned long currentMillis = 0;      // stores the value of millis() in each iteration of loop()
unsigned long PreviousStanceMillis = 0;
unsigned long PreviousShowTimeMillis = 0;
unsigned long ShowTime = 0;

// Variables to check R2 state for transitions
enum StanceState
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

int TiltUp;
int TiltDn;
int LegUp;
int LegDn;
StanceState currentStance;
StanceState StanceTarget;
int previousStance = -1;  // Track previous stance for display updates
char stanceName[16] = "No Target";
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

// Button detection and debounce for the rolling code remote
#ifdef ENABLE_ROLLING_CODE_TRIGGER
    #define BUTTON_DEBOUNCE_TIME 150
    int buttonALastState;
    int buttonBLastState;
    int buttonCLastState;
    int buttonDLastState;
    unsigned long buttonATimeout;
    unsigned long buttonBTimeout;
    unsigned long buttonCTimeout;
    unsigned long buttonDTimeout;
#endif

// Debug Print stuff
#define DEBUG
#define DEBUG_VERBOSE  // Enable this to see all debug status on the Serial Monitor.
#ifdef DEBUG
    #define DEBUG_PRINT_LN(msg)  Serial.println(msg)
    #define DEBUG_PRINT(msg)  Serial.print(msg)
#else
    #define DEBUG_PRINT_LN(msg)
    #define DEBUG_PRINT(msg)
#endif

/*
    Setup

    Basic pin setup to read the various sensors.
    Enable the Serial communication to the Sabertooth and for debug output.
    Initialize the display.
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

    // Initialize the Sabertooth serial port (UART0 on ESP32, UART1 on Pro Micro)
    #ifdef USE_WAVESHARE_ESP32_LCD
        Serial0.begin(9600); // UART0 TX0/RX0 pins on Waveshare ESP32
    #else
        Serial1.begin(9600); // Hardware UART TX/RX pins on Pro Micro
    #endif

    // Setup the USB Serial Port for debug output.
    #ifdef USE_WAVESHARE_ESP32_LCD
        Serial.begin(115200); // USB CDC on the Waveshare ESP32
    #else
        Serial.begin(9600); // USB on Pro Micro
    #endif

    // Initialize the display
    display.begin();

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

/*
    ReadRollingCodeTrigger

    This code will read the signals from the rolling code remote receiver, and set the StanceTarget variable accordingly.
*/
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
        if (now >= buttonATimeout && rollCodeA == HIGH && buttonALastState == LOW)
        {
            buttonATimeout = now + BUTTON_DEBOUNCE_TIME;
            if (!enableRollCodeTransitions)
            {
                enableRollCodeTransitions = true;
                killDebugSent = false;
                rollCodeTransitionTimeout = now + COMMAND_ENABLE_TIMEOUT;
                display.showRollCodeEnabled(true);
                DEBUG_PRINT_LN("Rolling Code Transmitter Transitions Enabled");
            }
            else
            {
                enableRollCodeTransitions = false;
                killDebugSent = false;
                display.showRollCodeEnabled(false);
                DEBUG_PRINT_LN("Rolling Code Transmitter Transitions Disabled");
            }
        }
        // Always track the actual signal state so edges aren't missed during debounce
        buttonALastState = rollCodeA;

        // Button B: Transition to three leg stance.
        if (now >= buttonBTimeout && enableRollCodeTransitions && rollCodeB == HIGH && buttonBLastState == LOW)
        {
            buttonBTimeout = now + BUTTON_DEBOUNCE_TIME;
            StanceTarget = THREE_LEG_STANCE;
            DEBUG_PRINT_LN("Moving to Three Leg Stance.");
        }
        buttonBLastState = rollCodeB;

        // Button C: Transition to two leg stance.
        if (now >= buttonCTimeout && enableRollCodeTransitions && rollCodeC == HIGH && buttonCLastState == LOW)
        {
            buttonCTimeout = now + BUTTON_DEBOUNCE_TIME;
            StanceTarget = TWO_LEG_STANCE;
            DEBUG_PRINT_LN("Moving to Two Leg Stance.");
        }
        buttonCLastState = rollCodeC;

        // Button D: Reserved for future use.
        if (now >= buttonDTimeout && enableRollCodeTransitions && rollCodeD == HIGH && buttonDLastState == LOW)
        {
            buttonDTimeout = now + BUTTON_DEBOUNCE_TIME;
            DEBUG_PRINT_LN("Button D Pressed");
        }
        buttonDLastState = rollCodeD;

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
    // Update the LCD display with current status
    display.showStatus(currentStance, stanceName);

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
        DEBUG_PRINT(currentStance); DEBUG_PRINT(": "); DEBUG_PRINT_LN(stanceName);
        DEBUG_PRINT("Stance Target : ");
        DEBUG_PRINT_LN(StanceTarget);
        DEBUG_PRINT("Leg Moving    : ");
        DEBUG_PRINT_LN(LegMoving);
        DEBUG_PRINT("Tilt Moving   : ");
        DEBUG_PRINT_LN(TiltMoving);
        DEBUG_PRINT("Show Time     : ");
        DEBUG_PRINT_LN(ShowTime);
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
    display.showTransition(StanceTarget);

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
    display.showTransition(StanceTarget);

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
    CheckStance

    This is simply taking all of the possibilities of the switch positions and giving them a number.
    The loop only runs when both (Leg/Tilt)Moving flags are false, meaning that it does not run in the middle of
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
            currentStance = TWO_LEG_STANCE;
            strcpy(stanceName, "Two Legs.    "); // LUTU
            return;
        }

        // Center leg is down, Body is tilted.  This is a 3 leg stance
        if (LegUp == HIGH && LegDn == LOW && TiltUp == HIGH && TiltDn == LOW)
        {
            currentStance = THREE_LEG_STANCE;
            strcpy(stanceName, "Three Legs   "); // LDTD
            return;
        }

        // Leg is up.  Body switches are open.
        // The body is somewhere between straight and tilted.
        if (LegUp == LOW && LegDn == HIGH && TiltUp == HIGH && TiltDn == HIGH)
        {
            currentStance = STANCE_ERROR_LEG_UP_TILT_UNKNOWN;
            strcpy(stanceName, "Error - LUT?");
        }

        // Leg switches are both open.  The leg is somewhere between up and down
        // Body is straight for a 2 leg stance.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == LOW && TiltDn == HIGH)
        {
            currentStance = STANCE_ERROR_LEG_UNKNOWN_TILT_UP;
            strcpy(stanceName, "Error - L?TU");
        }

        // Leg is down, and the body is straight for a 2 leg stance
        // The droid is balanced on the center foot. (Probably about to fall over)
        if (LegUp == HIGH && LegDn == LOW && TiltUp == LOW && TiltDn == HIGH)
        {
            currentStance = STANCE_ERROR_LEG_DOWN_TILT_UP;
            strcpy(stanceName, "Error - LDTU");
        }

        // Leg is down.  Body switches are both open.
        // The body is somewhere between straight and 18 degrees
        if (LegUp == HIGH && LegDn == LOW && TiltUp == HIGH && TiltDn == HIGH)
        {
            currentStance = STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN;
            strcpy(stanceName, "Error - LDT?");
        }

        // Leg switches are both open.
        // Body is tilted for a 3 leg stance.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == HIGH && TiltDn == LOW)
        {
            currentStance = STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN;
            strcpy(stanceName, "Error - L?TD");
        }

        // All 4 limit switches are open.  No idea where we are.
        if (LegUp == HIGH && LegDn == HIGH && TiltUp == HIGH && TiltDn == HIGH)
        {
            currentStance = STANCE_ERROR_ALL_UNKNOWN;
            strcpy(stanceName, "Error - L?T?");
        }

        // Leg is up, body is tilted.  This should not be possible.  If it happens, we are in a bad state.
        if (LegUp == LOW && LegDn == HIGH && TiltUp == HIGH && TiltDn == LOW)
        {
            currentStance = STANCE_ERROR_LEG_UP_TILT_DOWN;
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
    stance (currentStance) based on reading the limit switches ane figuring out where we are.

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
    if (StanceTarget == currentStance)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // Stance 7 is bad, all 4 switches open, no idea where anything is.  do nothing.
    if (currentStance == STANCE_ERROR_ALL_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // if you are in three legs and told to go to 2
    if (StanceTarget == TWO_LEG_STANCE && currentStance == THREE_LEG_STANCE)
    {
        LegMoving = true;
        TiltMoving = true;
        ThreeToTwo();
    }
    // This is the first of the slight unknowns, target is two legs,  look up to stance 3, the center leg is up, but the tilt is unknown.
    // You are either standing on two legs, or already in a pile on the ground. Cant hurt to try tilting up.
    if (StanceTarget == TWO_LEG_STANCE && currentStance == STANCE_ERROR_LEG_UP_TILT_UNKNOWN)
    {
        TiltMoving = true;
        MoveTiltUp();
    }
    // target two legs, tilt is up, center leg unknown, Can not hurt to try and lift the leg again.
    if (StanceTarget == TWO_LEG_STANCE && ((currentStance == STANCE_ERROR_LEG_UNKNOWN_TILT_UP) || (currentStance == STANCE_ERROR_LEG_DOWN_TILT_UP)))
    {
        LegMoving = true;
        MoveLegUp();
    }
    //Target is two legs, center foot is down, tilt is unknown, too risky do nothing.
    if (StanceTarget == TWO_LEG_STANCE && currentStance == STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is two legs, tilt is down, center leg is unknown,  too risky, do nothing.
    if (StanceTarget == TWO_LEG_STANCE && currentStance == STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is three legs, stance is two legs, run two to three.
    if (StanceTarget == THREE_LEG_STANCE && currentStance == TWO_LEG_STANCE)
    {
        LegMoving = true;
        TiltMoving = true;
        TwoToThree();
    }
    //Target is three legs. center leg is up, tilt is unknown, safer to do nothing, Recover from stance 3 with the up command
    if (StanceTarget == THREE_LEG_STANCE && currentStance == STANCE_ERROR_LEG_UP_TILT_UNKNOWN)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // target is three legs, but don't know where the center leg is.   Best to not try this,
    // recover from stance 4 with the up command,
    if (StanceTarget == THREE_LEG_STANCE && currentStance == STANCE_ERROR_LEG_UNKNOWN_TILT_UP)
    {
        ST.motor(1, 0);
        ST.motor(2, 0);
        LegMoving = false;
        TiltMoving = false;
        return;
    }
    // Target is three legs, the center foot is down, tilt is unknown. either on 3 legs now, or a smoking mess,
    // nothing to lose in trying to tilt down again
    if (StanceTarget == THREE_LEG_STANCE && currentStance == STANCE_ERROR_LEG_DOWN_TILT_UNKNOWN)
    {
        TiltMoving = true;
        MoveTiltDn();
    }
    // kinda like above, Target is 3 legs, tilt is down, center leg is unknown, ......got nothing to lose.
    if (StanceTarget == THREE_LEG_STANCE && currentStance == STANCE_ERROR_LEG_UNKNOWN_TILT_DOWN)
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
    if (currentStance != previousStance)
    {
        previousStance = currentStance;
        Display();
    }

    // Check if the rolling code transition timeout has expired.
    if (enableRollCodeTransitions && (currentMillis >= rollCodeTransitionTimeout))
    {
        // We have exceeded the time to do a transition start.
        // Auto Disable the safety so we don't accidentally trigger the transition.
        enableRollCodeTransitions = false;
        display.showRollCodeEnabled(false);
        //DEBUG_PRINT_LN("Warning: Transition Enable Timeout reached.  Disabling Rolling Code Transitions.");
    }

    Move();

    // Once we have moved, check to see if we've reached the target.
    // If we have then we reset the Target, so that we don't keep
    // trying to move motors (This was a bug found in testing!)
    if (currentStance == StanceTarget)
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
