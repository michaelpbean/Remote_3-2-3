#ifdef USE_WAVESHARE_ESP32_LCD

#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include "settings.h"

// Web command codes for motor control
enum WebCommand
{
    WEB_CMD_NONE = 0,
    WEB_CMD_MOVE_LEG_UP,
    WEB_CMD_MOVE_LEG_DN,
    WEB_CMD_MOVE_TILT_UP,
    WEB_CMD_MOVE_TILT_DN,
    WEB_CMD_TWO_TO_THREE,
    WEB_CMD_THREE_TO_TWO,
    WEB_CMD_EMERGENCY_STOP
};

class WebConfigServer
{
    public:
        WebConfigServer(SettingsManager& settingsManager);

        // Start the WiFi AP and web server. Call from setup().
        void Begin();

        // Handle incoming HTTP requests. Call from loop().
        void HandleClient();

        // Pending command from web UI, consumed by loop().
        volatile WebCommand pendingCommand;

    private:
        SettingsManager& settingsMgr;
        WebServer server;

        void HandleRoot();
        void HandleSave();
        void HandleReset();
        void HandleStatus();
        void HandleCommand();
        void SendNumberRow(WiFiClient& client, const char* label, const char* name,
                           int value, int defaultValue, int minVal, int maxVal);
        void SendHtmlHeader(const char* title);
        void SendHtmlFooter();
};

#endif // WEBCONFIG_H
#endif // USE_WAVESHARE_ESP32_LCD
