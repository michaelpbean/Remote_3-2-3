# R2D2 3-2-3 Controller - Coding Conventions

## Project Overview
Arduino/ESP32 controller for R2D2's 2-leg/3-leg stance transitions using USBSabertooth motor controllers. Supports dual hardware targets: Waveshare ESP32-C6-LCD-1.47 and Arduino Pro Micro + Adafruit LCD Shield.

## Code Style

### Braces and Indentation
- **Allman style braces**: Opening brace on its own line
- **4-space indentation** (no tabs)
- Maximum line length: 120 characters

```cpp
void Function()
{
    if (condition)
    {
        // code here
    }
}
```

### Preprocessor Directives
- `#ifdef`, `#else`, `#endif` and similar preprocessor sections should match the indentation style of normal if blocks
- Indent preprocessor directives with their contained code

```cpp
#ifdef USE_WAVESHARE_ESP32_LCD
    #include <Adafruit_ST7789.h>

    void initDisplay()
    {
        // Waveshare-specific code
    }
#else
    #include "Adafruit_RGBLCDShield.h"

    void initDisplay()
    {
        // Arduino-specific code
    }
#endif
```

### Class Access Modifiers
- `public:`, `private:`, `protected:` access modifiers are indented one level
- Class members are indented one additional level beyond the access modifier

```cpp
class DisplayManager
{
    public:
        void begin();
        void setBacklightColor(int color);
        void showStatus(int stance, const char* stanceName);

    private:
        int lastColor;
        void internalMethod();
};
```

## Arduino/Embedded Conventions

### Hardware Optimization
- Prefer single class with `#ifdef` internals over abstract base classes (avoids vtable overhead)
- Minimize dynamic memory allocation
- Keep interrupt handlers short and simple

### Digital Pin States
- Always use `LOW`/`HIGH` constants instead of `0`/`1` for digital pin states
- Always use `INPUT_PULLUP`, `INPUT`, `OUTPUT` constants for pinMode

```cpp
// Good
if (digitalRead(LegDnPin) == LOW)

// Bad
if (digitalRead(LegDnPin) == 0)
```

### Hardware Documentation
- Comment hardware-specific sections clearly
- Document pin assignments and their purpose
- Note physical connections and switch types (e.g., "NO mode - Normal Open")

## File Organization

```
src/
├── config.h              # Hardware configuration and pin definitions
├── display.h/.cpp        # DisplayManager class (LCD/TFT display abstraction)
└── remote_3-2-3.ino     # Main program logic
```

### File Responsibilities
- **config.h**: All hardware configuration (`USE_WAVESHARE_ESP32_LCD`), pin definitions for both targets, ESP32-specific defines
- **display.h/cpp**: Complete display abstraction (TFT or character LCD) with hardware-agnostic public interface
- **remote_3-2-3.ino**: Main controller logic, state machine, motor control, sensor reading

## Naming Conventions

- **Functions**: PascalCase (e.g., `ReadLimitSwitches()`, `MoveLegDn()`)
- **Variables**: camelCase (e.g., `currentStance`, `enableRollCodeTransitions`)
- **Enum types**: PascalCase (e.g., `StanceState`)
- **Enum values**: UPPER_SNAKE_CASE with prefix (e.g., `TWO_LEG_STANCE`, `STANCE_ERROR_LEG_UP_TILT_UNKNOWN`)
- **Constants/Defines**: UPPER_SNAKE_CASE (e.g., `COMMAND_ENABLE_TIMEOUT`, `TiltUpPin`)
- **Global objects**: camelCase (e.g., `display`, `ST`)

## Variable Naming Specifics

### Boolean Flags
- Use descriptive names that read naturally in conditionals
- Prefix with verb when appropriate: `enableRollCodeTransitions`, `killDebugSent`
- Use present participle for state: `LegMoving`, `TiltMoving` (not `LegHappy`)

### State Variables
- Be explicit: `currentStance` not just `Stance` (avoids collision with enum)
- Use clear names: `StanceTarget` indicates a desired state vs current state

## Comments

### Comment Philosophy
- Explain **why**, not **what** the code does
- Document non-obvious behavior and hardware interactions
- Keep TODO comments with context about what needs to be done

### Required Comments
- Hardware connections and pin purposes
- Timing-critical sections
- Workarounds and known issues
- State machine transitions and why they work that way

```cpp
// Good - explains why
// When the tilt down switch opens, the timer starts

// Bad - describes what (obvious from code)
// Set ShowTime to 0
```

## Architecture Patterns

### Dual Hardware Support
- Use `#ifdef USE_WAVESHARE_ESP32_LCD` for hardware-specific code
- Keep public interfaces hardware-agnostic
- Centralize all hardware configuration in `config.h`

### State Machine
- Use enums for state values (e.g., `StanceState`)
- Separate current state from target state
- Clear error state naming that describes the problem

### Display Updates
- Update display on state change, not on timer
- Keep display logic separate from controller logic
- Serial debug output remains verbose and on timer

## Refactoring Guidelines

### When Adding Features
- Consider both hardware targets
- Update `config.h` if adding pins or hardware config
- Keep display updates in DisplayManager class
- Don't add "improvements" beyond what was requested

### Code Organization Preferences
- Extract hardware-specific code to appropriate files
- Avoid over-engineering: solve current problem, not future hypotheticals
- Don't add unnecessary abstractions (no helpers for one-time operations)
- No backwards-compatibility hacks for deleted code

## Testing Approach
- Compile for both hardware targets when making changes
- Test state transitions with actual hardware when possible
- Verify display updates on both LCD types
- Check serial debug output remains helpful
