#ifdef USE_WAVESHARE_ESP32_LCD

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Preferences.h>

struct ControllerSettings
{
    // Global power multiplier (0-100 percent, applied to all motor power values)
    uint8_t powerMultiplier;

    // Motor power settings (-2047 to +2047)
    int16_t moveLegDnPower;
    int16_t moveLegUpPower;
    int16_t moveTiltDnPower;
    int16_t moveTiltUpPower;
    int16_t twoToThreeLegPower;
    int16_t twoToThreeTiltPower;
    int16_t threeToTwoLegSlowPower;
    int16_t threeToTwoLegFastPower;
    int16_t threeToTwoTiltPower;

    // Timing settings (milliseconds)
    uint16_t stanceInterval;
    uint16_t showTimeInterval;
    uint32_t commandEnableTimeout;
    uint16_t buttonDebounceTime;

    // ThreeToTwo phase timing (ShowTime tick counts)
    uint16_t phase1Start;
    uint16_t phase1End;
    uint16_t phase2Start;
};

class SettingsManager
{
    public:
        SettingsManager();

        // Load settings from NVS. If no saved settings exist, loads defaults.
        void Load();

        // Save current settings to NVS.
        void Save();

        // Reset settings to compiled defaults and save to NVS.
        void ResetToDefaults();

        // The active settings.
        ControllerSettings settings;

        // Flag indicating new settings need to be applied when motors are idle.
        bool pendingApply;

    private:
        Preferences preferences;
        void SetDefaults();
};

#endif // SETTINGS_H
#endif // USE_WAVESHARE_ESP32_LCD
