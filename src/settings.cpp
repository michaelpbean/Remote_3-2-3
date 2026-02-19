#include "config.h"

#ifdef USE_WAVESHARE_ESP32_LCD

#include "settings.h"

static const char* NVS_NAMESPACE = "r2d2cfg";

SettingsManager::SettingsManager()
    : pendingApply(false)
{
    SetDefaults();
}

void SettingsManager::SetDefaults()
{
    settings.powerMultiplier         = DEFAULT_POWER_MULTIPLIER;
    settings.moveLegDnPower          = DEFAULT_MOVE_LEG_DN_POWER;
    settings.moveLegUpPower          = DEFAULT_MOVE_LEG_UP_POWER;
    settings.moveTiltDnPower         = DEFAULT_MOVE_TILT_DN_POWER;
    settings.moveTiltUpPower         = DEFAULT_MOVE_TILT_UP_POWER;
    settings.twoToThreeLegPower      = DEFAULT_TWO_TO_THREE_LEG_POWER;
    settings.twoToThreeTiltPower     = DEFAULT_TWO_TO_THREE_TILT_POWER;
    settings.threeToTwoLegSlowPower  = DEFAULT_THREE_TO_TWO_LEG_SLOW_POWER;
    settings.threeToTwoLegFastPower  = DEFAULT_THREE_TO_TWO_LEG_FAST_POWER;
    settings.threeToTwoTiltPower     = DEFAULT_THREE_TO_TWO_TILT_POWER;

    settings.stanceInterval          = DEFAULT_STANCE_INTERVAL;
    settings.showTimeInterval        = DEFAULT_SHOWTIME_INTERVAL;
    settings.commandEnableTimeout    = DEFAULT_COMMAND_ENABLE_TIMEOUT;
    settings.buttonDebounceTime      = DEFAULT_BUTTON_DEBOUNCE_TIME;

    settings.phase1Start             = DEFAULT_PHASE1_START;
    settings.phase1End               = DEFAULT_PHASE1_END;
    settings.phase2Start             = DEFAULT_PHASE2_START;
}

void SettingsManager::Load()
{
    // Start with defaults so any missing keys get default values
    SetDefaults();

    preferences.begin(NVS_NAMESPACE, true); // read-only

    settings.powerMultiplier         = preferences.getUChar("pwrMult",     settings.powerMultiplier);
    settings.moveLegDnPower          = preferences.getShort("legDnPwr",    settings.moveLegDnPower);
    settings.moveLegUpPower          = preferences.getShort("legUpPwr",    settings.moveLegUpPower);
    settings.moveTiltDnPower         = preferences.getShort("tiltDnPwr",   settings.moveTiltDnPower);
    settings.moveTiltUpPower         = preferences.getShort("tiltUpPwr",   settings.moveTiltUpPower);
    settings.twoToThreeLegPower      = preferences.getShort("23legPwr",    settings.twoToThreeLegPower);
    settings.twoToThreeTiltPower     = preferences.getShort("23tiltPwr",   settings.twoToThreeTiltPower);
    settings.threeToTwoLegSlowPower  = preferences.getShort("32legSlwPwr", settings.threeToTwoLegSlowPower);
    settings.threeToTwoLegFastPower  = preferences.getShort("32legFstPwr", settings.threeToTwoLegFastPower);
    settings.threeToTwoTiltPower     = preferences.getShort("32tiltPwr",   settings.threeToTwoTiltPower);

    settings.stanceInterval          = preferences.getUShort("stanceInt",  settings.stanceInterval);
    settings.showTimeInterval        = preferences.getUShort("showTimeInt", settings.showTimeInterval);
    settings.commandEnableTimeout    = preferences.getULong("cmdTimeout",  settings.commandEnableTimeout);
    settings.buttonDebounceTime      = preferences.getUShort("btnDebounce", settings.buttonDebounceTime);

    settings.phase1Start             = preferences.getUShort("ph1Start",   settings.phase1Start);
    settings.phase1End               = preferences.getUShort("ph1End",     settings.phase1End);
    settings.phase2Start             = preferences.getUShort("ph2Start",   settings.phase2Start);

    preferences.end();
}

void SettingsManager::Save()
{
    preferences.begin(NVS_NAMESPACE, false); // read-write

    preferences.putUChar("pwrMult",     settings.powerMultiplier);
    preferences.putShort("legDnPwr",    settings.moveLegDnPower);
    preferences.putShort("legUpPwr",    settings.moveLegUpPower);
    preferences.putShort("tiltDnPwr",   settings.moveTiltDnPower);
    preferences.putShort("tiltUpPwr",   settings.moveTiltUpPower);
    preferences.putShort("23legPwr",    settings.twoToThreeLegPower);
    preferences.putShort("23tiltPwr",   settings.twoToThreeTiltPower);
    preferences.putShort("32legSlwPwr", settings.threeToTwoLegSlowPower);
    preferences.putShort("32legFstPwr", settings.threeToTwoLegFastPower);
    preferences.putShort("32tiltPwr",   settings.threeToTwoTiltPower);

    preferences.putUShort("stanceInt",  settings.stanceInterval);
    preferences.putUShort("showTimeInt", settings.showTimeInterval);
    preferences.putULong("cmdTimeout",  settings.commandEnableTimeout);
    preferences.putUShort("btnDebounce", settings.buttonDebounceTime);

    preferences.putUShort("ph1Start",   settings.phase1Start);
    preferences.putUShort("ph1End",     settings.phase1End);
    preferences.putUShort("ph2Start",   settings.phase2Start);

    preferences.end();

    pendingApply = true;
}

void SettingsManager::ResetToDefaults()
{
    preferences.begin(NVS_NAMESPACE, false);
    preferences.clear();
    preferences.end();

    SetDefaults();
    pendingApply = true;
}

#endif // USE_WAVESHARE_ESP32_LCD
