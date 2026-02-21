#include "config.h"

#ifdef USE_WAVESHARE_ESP32_LCD

#include "webconfig.h"

// Access global state variables from remote_3-2-3.ino for status display
extern int currentStance;
extern int StanceTarget;
extern char stanceName[];
extern bool LegMoving;
extern bool TiltMoving;
extern bool enableRollCodeTransitions;
extern int LegUp, LegDn, TiltUp, TiltDn;
extern int webMoveActive;
#ifdef USE_WAVESHARE_ESP32_S3_LCD
extern float imuTiltAngleDeg;
extern bool imuTiltValid;
#endif

WebConfigServer::WebConfigServer(SettingsManager& settingsManager)
    : settingsMgr(settingsManager), server(80), pendingCommand(WEB_CMD_NONE)
{
}

void WebConfigServer::Begin()
{
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

    Serial.print("WiFi AP started. SSID: ");
    Serial.println(WIFI_AP_SSID);
    Serial.print("Config page: http://");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, [this]() { HandleRoot(); });
    server.on("/save", HTTP_POST, [this]() { HandleSave(); });
    server.on("/reset", HTTP_POST, [this]() { HandleReset(); });
    server.on("/status", HTTP_GET, [this]() { HandleStatus(); });
    server.on("/cmd", HTTP_POST, [this]() { HandleCommand(); });
    server.begin();
}

void WebConfigServer::HandleClient()
{
    server.handleClient();
}

void WebConfigServer::SendHtmlHeader(const char* title)
{
    server.sendContent(F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>R2D2 3-2-3 Config</title>"
        "<style>"
        "body{font-family:sans-serif;margin:20px;max-width:600px;}"
        "h1{font-size:1.4em;}"
        "h2{font-size:1.1em;margin-top:20px;}"
        "table{border-collapse:collapse;width:100%;}"
        "td{padding:4px 8px;}"
        "td:first-child{white-space:nowrap;}"
        "input[type=number]{width:80px;}"
        ".default{color:#888;font-size:0.85em;}"
        "button,input[type=submit]{padding:8px 16px;margin:8px 4px;font-size:1em;cursor:pointer;}"
        ".save{background:#4CAF50;color:white;border:none;border-radius:4px;}"
        ".reset{background:#f44336;color:white;border:none;border-radius:4px;}"
        "#status-panel{border:2px solid #333;border-radius:8px;padding:12px;margin-bottom:16px;}"
        "#status-panel table{width:auto;}"
        "#status-panel td{padding:2px 8px;}"
        ".status-ok{background:#e3f2fd;border-color:#1976D2;}"
        ".status-error{background:#ffebee;border-color:#d32f2f;}"
        ".status-moving{background:#e8f5e9;border-color:#388E3C;}"
        ".sw-closed{color:#4CAF50;font-weight:bold;}"
        ".sw-open{color:#888;}"
        ".cmd-btn{padding:10px 16px;margin:4px;font-size:1em;border:none;border-radius:4px;"
        "color:white;cursor:pointer;min-width:120px;}"
        ".cmd-btn:disabled{opacity:0.4;cursor:not-allowed;}"
        ".cmd-move{background:#1976D2;}"
        ".cmd-transition{background:#388E3C;}"
        ".cmd-stop{background:#d32f2f;font-weight:bold;}"
        "#control-panel{margin-bottom:16px;}"
        "#control-panel h2{margin-top:0;}"
        "</style></head><body>"));
    server.sendContent(F("<h1>"));
    server.sendContent(title);
    server.sendContent(F("</h1>"));
}

void WebConfigServer::SendHtmlFooter()
{
    server.sendContent(F("</body></html>"));
}

void WebConfigServer::SendNumberRow(WiFiClient& client, const char* label,
    const char* name, int value, int defaultValue, int minVal, int maxVal)
{
    // Build one table row: label, input, default hint
    String row = "<tr><td>";
    row += label;
    row += "</td><td><input type='number' name='";
    row += name;
    row += "' value='";
    row += value;
    row += "' min='";
    row += minVal;
    row += "' max='";
    row += maxVal;
    row += "'></td><td class='default'>default: ";
    row += defaultValue;
    row += "</td></tr>";
    server.sendContent(row);
}

void WebConfigServer::HandleRoot()
{
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    SendHtmlHeader("R2D2 3-2-3 Configuration");

    // Live status panel
    server.sendContent(F(
        "<div id='status-panel'>"
        "<h2 style='margin-top:0'>Live Status</h2>"
        "<table>"
        "<tr><td>Status:</td><td id='st-status'>--</td></tr>"
        "<tr><td>Stance:</td><td id='st-stance'>--</td></tr>"
        "<tr><td>Target:</td><td id='st-target'>--</td></tr>"
        "<tr><td>Remote Armed:</td><td id='st-armed'>--</td></tr>"
        "<tr><td>Tilt Angle:</td><td id='st-tilt'>--</td></tr>"
        "<tr><td>Limit Switches:</td><td id='st-switches'>--</td></tr>"
        "<tr><td>Web Move:</td><td id='st-webmove'>--</td></tr>"
        "</table>"
        "</div>"));

    // Motor control buttons
    server.sendContent(F(
        "<div id='control-panel'>"
        "<h2>Motor Control</h2>"
        "<div>"
        "<button class='cmd-btn cmd-move' id='btn-legup' onclick=\"sendCmd('legup')\">Leg Up</button>"
        "<button class='cmd-btn cmd-move' id='btn-legdn' onclick=\"sendCmd('legdn')\">Leg Down</button>"
        "<button class='cmd-btn cmd-move' id='btn-tiltup' onclick=\"sendCmd('tiltup')\">Tilt Up</button>"
        "<button class='cmd-btn cmd-move' id='btn-tiltdn' onclick=\"sendCmd('tiltdn')\">Tilt Down</button>"
        "</div><div style='margin-top:8px'>"
        "<button class='cmd-btn cmd-transition' id='btn-23' onclick=\"sendCmd('twotothree')\">2-Leg &rarr; 3-Leg</button>"
        "<button class='cmd-btn cmd-transition' id='btn-32' onclick=\"sendCmd('threetotwo')\">3-Leg &rarr; 2-Leg</button>"
        "</div><div style='margin-top:8px'>"
        "<button class='cmd-btn cmd-stop' id='btn-stop' onclick=\"sendCmd('stop')\">EMERGENCY STOP</button>"
        "</div></div>"));

    // JavaScript for status polling and command buttons
    server.sendContent(F(
        "<script>"
        "function sendCmd(c){"
        "fetch('/cmd',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'cmd='+c}).then(r=>r.json()).then(d=>{"
        "if(!d.ok)alert(d.msg||'Command rejected');"
        "}).catch(()=>{});"
        "}"
        "function poll(){"
        "fetch('/status').then(r=>r.json()).then(d=>{"
        "var p=document.getElementById('status-panel');"
        "p.className=d.moving?'status-moving':(d.stance>2?'status-error':'status-ok');"
        "document.getElementById('st-status').textContent=d.moving?'Moving':(d.stance>2?'Error':'OK');"
        "document.getElementById('st-stance').textContent=d.stanceName+' ('+d.stance+')';"
        "var tgt={0:'None',1:'Two Legs',2:'Three Legs'};"
        "document.getElementById('st-target').textContent=tgt[d.target]||'Stance '+d.target;"
        "document.getElementById('st-armed').textContent=d.armed?'YES':'No';"
        "document.getElementById('st-tilt').textContent=d.tiltValid?(d.tiltDeg.toFixed(1)+' deg'):'--';"
        "var sw='';"
        "sw+='LegUp:'+(d.legUp?'<span class=sw-open>OPEN</span>':'<span class=sw-closed>CLOSED</span>');"
        "sw+=' LegDn:'+(d.legDn?'<span class=sw-open>OPEN</span>':'<span class=sw-closed>CLOSED</span>');"
        "sw+=' TiltUp:'+(d.tiltUp?'<span class=sw-open>OPEN</span>':'<span class=sw-closed>CLOSED</span>');"
        "sw+=' TiltDn:'+(d.tiltDn?'<span class=sw-open>OPEN</span>':'<span class=sw-closed>CLOSED</span>');"
        "document.getElementById('st-switches').innerHTML=sw;"
        "var wm={0:'None',1:'Leg Up',2:'Leg Down',3:'Tilt Up',4:'Tilt Down'};"
        "document.getElementById('st-webmove').textContent=wm[d.webMove]||'Unknown';"
        // Update button enabled/disabled states
        // Leg up: disabled if leg is already up (LegUp switch closed = 0)
        "document.getElementById('btn-legup').disabled=d.moving||!d.legUp;"
        // Leg down: disabled if leg is already down (LegDn switch closed = 0)
        "document.getElementById('btn-legdn').disabled=d.moving||!d.legDn;"
        // Tilt up: disabled if tilt is already up (TiltUp switch closed = 0)
        "document.getElementById('btn-tiltup').disabled=d.moving||!d.tiltUp;"
        // Tilt down: disabled if tilt is already down (TiltDn switch closed = 0)
        "document.getElementById('btn-tiltdn').disabled=d.moving||!d.tiltDn;"
        // 2->3: only enabled in two leg stance and not moving
        "document.getElementById('btn-23').disabled=d.moving||d.stance!=1;"
        // 3->2: only enabled in three leg stance and not moving
        "document.getElementById('btn-32').disabled=d.moving||d.stance!=2;"
        // Emergency stop: always enabled
        "}).catch(()=>{});"
        "}"
        "poll();setInterval(poll,1000);"
        "</script>"));

    server.sendContent(F("<p>Settings apply when motors are idle.</p>"
        "<form method='POST' action='/save'>"));

    WiFiClient client = server.client();
    ControllerSettings& s = settingsMgr.settings;

    // Global power multiplier
    server.sendContent(F("<h2>Global Power Scale</h2><table>"));
    SendNumberRow(client, "Power Multiplier (%)", "pwrMult", s.powerMultiplier, DEFAULT_POWER_MULTIPLIER, 0, 100);
    server.sendContent(F("</table>"));

    // Motor Power Settings
    server.sendContent(F("<h2>Motor Power (-2047 to 2047)</h2><table>"));

    SendNumberRow(client, "Leg Down",          "legDnPwr",    s.moveLegDnPower,         DEFAULT_MOVE_LEG_DN_POWER,           -2047, 2047);
    SendNumberRow(client, "Leg Up",            "legUpPwr",    s.moveLegUpPower,         DEFAULT_MOVE_LEG_UP_POWER,           -2047, 2047);
    SendNumberRow(client, "Tilt Down",         "tiltDnPwr",   s.moveTiltDnPower,        DEFAULT_MOVE_TILT_DN_POWER,          -2047, 2047);
    SendNumberRow(client, "Tilt Up",           "tiltUpPwr",   s.moveTiltUpPower,        DEFAULT_MOVE_TILT_UP_POWER,          -2047, 2047);

    server.sendContent(F("</table><h2>Transition: 2-Leg to 3-Leg</h2><table>"));

    SendNumberRow(client, "Leg Power",         "23legPwr",    s.twoToThreeLegPower,     DEFAULT_TWO_TO_THREE_LEG_POWER,      -2047, 2047);
    SendNumberRow(client, "Tilt Power",        "23tiltPwr",   s.twoToThreeTiltPower,    DEFAULT_TWO_TO_THREE_TILT_POWER,     -2047, 2047);

    server.sendContent(F("</table><h2>Transition: 3-Leg to 2-Leg</h2><table>"));

    SendNumberRow(client, "Leg Slow Power",    "32legSlwPwr", s.threeToTwoLegSlowPower, DEFAULT_THREE_TO_TWO_LEG_SLOW_POWER, -2047, 2047);
    SendNumberRow(client, "Leg Fast Power",    "32legFstPwr", s.threeToTwoLegFastPower, DEFAULT_THREE_TO_TWO_LEG_FAST_POWER, -2047, 2047);
    SendNumberRow(client, "Tilt Power",        "32tiltPwr",   s.threeToTwoTiltPower,    DEFAULT_THREE_TO_TWO_TILT_POWER,     -2047, 2047);

    server.sendContent(F("</table><h2>3-to-2 Phase Timing (ShowTime ticks)</h2><table>"));

    SendNumberRow(client, "Phase 1 Start",     "ph1Start",    s.phase1Start,            DEFAULT_PHASE1_START,                0, 100);
    SendNumberRow(client, "Phase 1 End",       "ph1End",      s.phase1End,              DEFAULT_PHASE1_END,                  0, 100);
    SendNumberRow(client, "Phase 2 Start",     "ph2Start",    s.phase2Start,            DEFAULT_PHASE2_START,                0, 100);

    server.sendContent(F("</table><h2>Timing (milliseconds)</h2><table>"));

    SendNumberRow(client, "Stance Interval",       "stanceInt",  s.stanceInterval,       DEFAULT_STANCE_INTERVAL,        10, 1000);
    SendNumberRow(client, "ShowTime Interval",     "showTimeInt", s.showTimeInterval,     DEFAULT_SHOWTIME_INTERVAL,      10, 1000);
    SendNumberRow(client, "Command Enable Timeout", "cmdTimeout", s.commandEnableTimeout, DEFAULT_COMMAND_ENABLE_TIMEOUT, 1000, 120000);
    SendNumberRow(client, "Button Debounce",       "btnDebounce", s.buttonDebounceTime,   DEFAULT_BUTTON_DEBOUNCE_TIME,   50, 500);

    server.sendContent(F("</table><br>"
        "<input type='submit' value='Save Settings' class='save'>"
        "</form>"
        "<form method='POST' action='/reset'>"
        "<button type='submit' class='reset' "
        "onclick=\"return confirm('Reset all settings to defaults?')\">Reset to Defaults</button>"
        "</form>"));

    SendHtmlFooter();
}

void WebConfigServer::HandleStatus()
{
    // Return JSON with current status for live polling
    String json = "{\"stance\":";
    json += currentStance;
    json += ",\"stanceName\":\"";
    json += stanceName;
    json += "\",\"target\":";
    json += StanceTarget;
    json += ",\"moving\":";
    json += (LegMoving || TiltMoving) ? "true" : "false";
    json += ",\"armed\":";
    json += enableRollCodeTransitions ? "true" : "false";
    json += ",\"legUp\":";
    json += LegUp;
    json += ",\"legDn\":";
    json += LegDn;
    json += ",\"tiltUp\":";
    json += TiltUp;
    json += ",\"tiltDn\":";
    json += TiltDn;
    json += ",\"webMove\":";
    json += webMoveActive;
#ifdef USE_WAVESHARE_ESP32_S3_LCD
    json += ",\"tiltDeg\":";
    json += String(imuTiltAngleDeg, 1);
    json += ",\"tiltValid\":";
    json += imuTiltValid ? "true" : "false";
#else
    json += ",\"tiltDeg\":0";
    json += ",\"tiltValid\":false";
#endif
    json += "}";

    server.send(200, "application/json", json);
}

void WebConfigServer::HandleCommand()
{
    if (!server.hasArg("cmd"))
    {
        server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Missing cmd\"}");
        return;
    }

    String cmd = server.arg("cmd");
    WebCommand wc = WEB_CMD_NONE;

    if (cmd == "legup")         wc = WEB_CMD_MOVE_LEG_UP;
    else if (cmd == "legdn")    wc = WEB_CMD_MOVE_LEG_DN;
    else if (cmd == "tiltup")   wc = WEB_CMD_MOVE_TILT_UP;
    else if (cmd == "tiltdn")   wc = WEB_CMD_MOVE_TILT_DN;
    else if (cmd == "twotothree") wc = WEB_CMD_TWO_TO_THREE;
    else if (cmd == "threetotwo") wc = WEB_CMD_THREE_TO_TWO;
    else if (cmd == "stop")     wc = WEB_CMD_EMERGENCY_STOP;

    if (wc == WEB_CMD_NONE)
    {
        server.send(400, "application/json", "{\"ok\":false,\"msg\":\"Unknown command\"}");
        return;
    }

    pendingCommand = wc;
    Serial.print("Web command received: ");
    Serial.println(cmd);
    server.send(200, "application/json", "{\"ok\":true}");
}

void WebConfigServer::HandleSave()
{
    ControllerSettings& s = settingsMgr.settings;

    if (server.hasArg("pwrMult"))     s.powerMultiplier        = constrain(server.arg("pwrMult").toInt(), 0, 100);
    if (server.hasArg("legDnPwr"))    s.moveLegDnPower         = server.arg("legDnPwr").toInt();
    if (server.hasArg("legUpPwr"))    s.moveLegUpPower         = server.arg("legUpPwr").toInt();
    if (server.hasArg("tiltDnPwr"))   s.moveTiltDnPower        = server.arg("tiltDnPwr").toInt();
    if (server.hasArg("tiltUpPwr"))   s.moveTiltUpPower        = server.arg("tiltUpPwr").toInt();
    if (server.hasArg("23legPwr"))    s.twoToThreeLegPower     = server.arg("23legPwr").toInt();
    if (server.hasArg("23tiltPwr"))   s.twoToThreeTiltPower    = server.arg("23tiltPwr").toInt();
    if (server.hasArg("32legSlwPwr")) s.threeToTwoLegSlowPower = server.arg("32legSlwPwr").toInt();
    if (server.hasArg("32legFstPwr")) s.threeToTwoLegFastPower = server.arg("32legFstPwr").toInt();
    if (server.hasArg("32tiltPwr"))   s.threeToTwoTiltPower    = server.arg("32tiltPwr").toInt();

    if (server.hasArg("stanceInt"))   s.stanceInterval         = server.arg("stanceInt").toInt();
    if (server.hasArg("showTimeInt")) s.showTimeInterval       = server.arg("showTimeInt").toInt();
    if (server.hasArg("cmdTimeout"))  s.commandEnableTimeout   = server.arg("cmdTimeout").toInt();
    if (server.hasArg("btnDebounce")) s.buttonDebounceTime     = server.arg("btnDebounce").toInt();

    if (server.hasArg("ph1Start"))    s.phase1Start            = server.arg("ph1Start").toInt();
    if (server.hasArg("ph1End"))      s.phase1End              = server.arg("ph1End").toInt();
    if (server.hasArg("ph2Start"))    s.phase2Start            = server.arg("ph2Start").toInt();

    settingsMgr.Save();

    Serial.println("Settings saved via web interface.");

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    SendHtmlHeader("Settings Saved");
    server.sendContent(F("<p>Settings saved successfully. They will apply when motors are idle.</p>"
        "<p><a href='/'>Back to configuration</a></p>"));
    SendHtmlFooter();
}

void WebConfigServer::HandleReset()
{
    settingsMgr.ResetToDefaults();

    Serial.println("Settings reset to defaults via web interface.");

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    SendHtmlHeader("Settings Reset");
    server.sendContent(F("<p>All settings have been reset to defaults. They will apply when motors are idle.</p>"
        "<p><a href='/'>Back to configuration</a></p>"));
    SendHtmlFooter();
}

#endif // USE_WAVESHARE_ESP32_LCD
