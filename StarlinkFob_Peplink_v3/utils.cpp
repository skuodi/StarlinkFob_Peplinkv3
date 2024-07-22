/**
 * @file  utils.h
 * @brief Contains common user definitions and convenience fucntions
 */

#include "stdint.h"
#include "M5StickCPlus2.h"
#include <Preferences.h>
#include "config.h"
#include "utils.h"

void resetPreferences()
{
    fob.wifi.ssidStaPrimary = WIFI1_SSID_DEFAULT;
    fob.wifi.passwordStaPrimary = WIFI1_PASSWORD_DEFAULT;
    fob.wifi.ssidStaSecondary = WIFI2_SSID_DEFAULT;
    fob.wifi.passwordStaSecondary = WIFI2_PASSWORD_DEFAULT;
    fob.wifi.ssidSoftAp = WIFI_SSID_SOFTAP_DEFAULT;
    fob.wifi.passwordSoftAp = WIFI_PASSWORD_SOFTAP_DEFAULT;
    fob.routers.ip = ROUTER_IP_DEFAULT;
    fob.routers.port = ROUTER_PORT_DEFAULT;
    fob.routers.username = ROUTER_USERNAME_DEFAULT;
    fob.routers.password = ROUTER_PASSWORD_DEFAULT;
    fob.routers.clientName = CLIENT_NAME_DEFAULT;
    fob.routers.clientScope = CLIENT_SCOPE_DEFAULT;
    fob.wifi.timeoutMs = WIFI_TIMEOUT_DEFAULT;
}

bool savePreferences()
{
    bool ret = false;
    PeplinkAPI_Settings_t newSettings =
        {
            .port = fob.routers.port,
            .clientScope = fob.routers.clientScope,
            .wifiTimeoutMs = fob.wifi.timeoutMs,
        };
    strncpy(newSettings.ssidStaPrimary, fob.wifi.ssidStaPrimary.c_str(), sizeof(newSettings.ssidStaPrimary));
    strncpy(newSettings.passwordStaPrimary, fob.wifi.passwordStaPrimary.c_str(), sizeof(newSettings.passwordStaPrimary));
    strncpy(newSettings.ssidStaSecondary, fob.wifi.ssidStaSecondary.c_str(), sizeof(newSettings.ssidStaSecondary));
    strncpy(newSettings.passwordStaSecondary, fob.wifi.passwordStaSecondary.c_str(), sizeof(newSettings.passwordStaSecondary));
    strncpy(newSettings.ssidSoftAp, fob.wifi.ssidSoftAp.c_str(), sizeof(newSettings.ssidSoftAp));
    strncpy(newSettings.passwordSoftAp, fob.wifi.passwordSoftAp.c_str(), sizeof(newSettings.passwordSoftAp));
    strncpy(newSettings.ip, fob.routers.ip.c_str(), sizeof(newSettings.ip));
    strncpy(newSettings.username, fob.routers.username.c_str(), sizeof(newSettings.username));
    strncpy(newSettings.password, fob.routers.password.c_str(), sizeof(newSettings.password));
    strncpy(newSettings.clientName, fob.routers.clientName.c_str(), sizeof(newSettings.clientName));

    Preferences newPrefs;
    if (newPrefs.begin(PREFERENCES_NAMESPACE, false))
    {
        newPrefs.clear();
        ret = newPrefs.putBytes(PREFERENCES_NAMESPACE, &newSettings, sizeof(newSettings));
        if(ret)
            Serial.println("Saved preferences to storage");
    }

    newPrefs.end();
    return ret;
}

void dumpPreferences()
{
    Serial.println("Router SSID1 : " + fob.wifi.ssidStaPrimary);
    Serial.println("Router Password1 :" + fob.wifi.passwordStaPrimary);
    Serial.println("Router SSID2 : " + fob.wifi.ssidStaSecondary);
    Serial.println("Router Password2 :" + fob.wifi.passwordStaSecondary);
    Serial.println("Soft AP SSID : " + fob.wifi.ssidSoftAp);
    Serial.println("Soft AP Password :" + fob.wifi.passwordSoftAp);
    Serial.println("Router IP : " + fob.routers.ip);
    Serial.println("Router Port : " + String(fob.routers.port));
    Serial.println("Router API Username : " + fob.routers.username);
    Serial.println("Router API Password : " + fob.routers.password);
    Serial.println("Router Client Name : " + fob.routers.clientName);
    Serial.print("Router Client Scope : ");
    if (fob.routers.clientScope == CLIENT_SCOPE_READ_WRITE)
        Serial.println("read-write");
    else
        Serial.println("read-only");
    Serial.println("Wi-Fi connect timeout : " + String(fob.wifi.timeoutMs));
}

void retrieveStoredCredentials()
{
    Preferences cookiePrefs;
    if (cookiePrefs.begin(COOKIES_NAMESPACE))
    {
        size_t cookieJarSize = cookiePrefs.getBytesLength(COOKIES_NAMESPACE);

        if (cookieJarSize == sizeof(PeplinkAPI_CookieJar_t)) // If the stored data is of the right size, extract it
        {
            uint8_t cookieBuffer[sizeof(PeplinkAPI_CookieJar_t)];
            cookiePrefs.getBytes(COOKIES_NAMESPACE, cookieBuffer, sizeof(cookieBuffer));
        
            PeplinkAPI_CookieJar_t *jar = (PeplinkAPI_CookieJar_t *)cookieBuffer;
            if (jar->magic == 0xDEADBEEF) // If the cookie jar is valid, extract the cookie
            {
                fob.routers.router.setCookie(String(jar->cookie));
                Serial.print("Got cookie from storage: ");
                Serial.println(jar->cookie);
                fob.routers.router.setToken(String(jar->token));
                Serial.print("Got token from storage: ");
                Serial.println(jar->token);
            }
        }
        cookiePrefs.end();
    }
}

void restorePreferences()
{
    resetPreferences();
    Preferences routerPrefs;

    if (routerPrefs.begin(PREFERENCES_NAMESPACE))
    {
        size_t settingsSize = routerPrefs.getBytesLength(PREFERENCES_NAMESPACE);

        uint8_t settingsBuffer[settingsSize];
        routerPrefs.getBytes(PREFERENCES_NAMESPACE, settingsBuffer, sizeof(settingsBuffer));
        routerPrefs.end();
        if (settingsSize % sizeof(PeplinkAPI_Settings_t) == 0)
        {
            PeplinkAPI_Settings_t *settings = (PeplinkAPI_Settings_t *)settingsBuffer;
            if (String(settings->ssidStaPrimary).length() && String(settings->ssidStaSecondary).length() &&
            String(settings->ssidSoftAp).length() && String(settings->ip).length() &&
            String(settings->port).length() && String(settings->username).length() &&
            String(settings->password).length() && String(settings->clientName).length()
            )
            {
                fob.wifi.ssidStaPrimary = settings->ssidStaPrimary;
                fob.wifi.passwordStaPrimary = settings->passwordStaPrimary;
                fob.wifi.ssidStaSecondary = settings->ssidStaSecondary;
                fob.wifi.passwordStaSecondary = settings->passwordStaSecondary;
                fob.wifi.ssidSoftAp = settings->ssidSoftAp;
                fob.wifi.passwordSoftAp = settings->passwordSoftAp;
                fob.routers.ip = settings->ip;
                fob.routers.port = settings->port;
                fob.routers.username = settings->username;
                fob.routers.password = settings->password;
                fob.routers.clientName = settings->clientName;
                fob.routers.clientScope = settings->clientScope;
                fob.wifi.timeoutMs = settings->wifiTimeoutMs;
                Serial.println("Got preferences from storage");
                dumpPreferences();
            }
            else
            {
                resetPreferences();
                savePreferences();
            }
        }
        else
            savePreferences();
    }
    else
        savePreferences();
}

void retrieveLastShutdownInfo()
{
    Preferences timePrefs;
    if (timePrefs.begin(TIMESTAMP_NAMESPACE))
    {
        size_t timestampSize = timePrefs.getBytesLength(TIMESTAMP_NAMESPACE);

        if (timestampSize && (timestampSize == sizeof(ShutdownTimestamp_t)))
        {
            uint8_t timestampBuffer[sizeof(ShutdownTimestamp_t)];
            timePrefs.getBytes(TIMESTAMP_NAMESPACE, timestampBuffer,  sizeof(ShutdownTimestamp_t));
            timePrefs.end();
            ShutdownTimestamp_t *timestamp = (ShutdownTimestamp_t*)timestampBuffer;
            fob.timestamps.lastShutdownRuntime = timestamp->lastShutdownRuntime;
            fob.timestamps.lastShutdownTime = timestamp->lastShutdownTime;
            fob.timestamps.lastShutdownTimezone = timestamp->lastShutdownTimezone;
            Serial.println("Got shutdown details:");
            Serial.printf("\tRuntime: %"PRId64"\n", timestamp->lastShutdownRuntime);
            Serial.printf("\tTime: %s\n", timestamp->lastShutdownTime);
            Serial.printf("\tTimezone: %s\n",  timestamp->lastShutdownTimezone);
        }
    }
    timePrefs.end();
}
void retrieveLastAlertInfo()
{
    Preferences alertPrefs;
    if (alertPrefs.begin(TEMPERATURE_ALERT_NAMESPACE))
    {
        size_t timestampSize = alertPrefs.getBytesLength(TEMPERATURE_ALERT_NAMESPACE);

        if (timestampSize && (timestampSize == sizeof(AlertTimestamp_t)))
        {
            uint8_t timestampBuffer[sizeof(AlertTimestamp_t)];
            alertPrefs.getBytes(TEMPERATURE_ALERT_NAMESPACE, timestampBuffer,  sizeof(AlertTimestamp_t));
            alertPrefs.end();
            AlertTimestamp_t *timestamp = (AlertTimestamp_t*)timestampBuffer;
            fob.timestamps.lastTempAlertTime = timestamp->lastTempAlertTime;
            fob.timestamps.lastTempAlertTemp = timestamp->lastTempAlertTemp;
            fob.timestamps.lastTempAlertThresh = timestamp->lastTempAlertThresh;
            Serial.println("Got last temp alert details:");
            Serial.printf("\tTime: %s\n", timestamp->lastTempAlertTime);
            Serial.printf("\tTemp: %fF\n",  timestamp->lastTempAlertTemp);
            Serial.printf("\tThresh: %fF\n",  timestamp->lastTempAlertThresh);
        }
    }
    alertPrefs.end();
}

void setTimezone(const char* tzone)
{
  Serial.printf("Setting Timezone to %s\n",tzone);
  setenv("TZ",tzone,1);
  tzset();
}

bool syncNtpToRtc(const char* tzone)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain NTP time");
    return false;
  }
  setTimezone(tzone);
  time_t syncT = time(nullptr) + 1;   // Advance one second
  while (syncT > time(nullptr));      // and wait for the actual time to catch up
  M5.Rtc.setDateTime(gmtime(&syncT)); // Set the new date and time to RTC
  Serial.println(&timeinfo, "Local time: %I:%M:%S%p %A, %B-%d-%Y ");
  return true;
}

void printRouterInfo(PeplinkRouter &router)
{
    PeplinkRouterInfo rInfo = router.info();
    Serial.println("Router Info:");
    Serial.println("\tName : " + rInfo.name);
    Serial.println("\tUptime : " + String(rInfo.uptime));
    Serial.println("\tSerial : " + rInfo.serial);
    Serial.println("\tFwVersion : " + rInfo.fwVersion);
    Serial.println("\tProduct Code : " + rInfo.productCode);
    Serial.println("\tHardware Rev : " + rInfo.hardwareRev);
}

void printRouterLocation(PeplinkRouter &router)
{
    PeplinkRouterLocation location = router.location();
    Serial.println("Router Location:-");
    Serial.println("\tLongitude : " + location.longitude);
    Serial.println("\tLatitude : " + location.latitude);
    Serial.println("\tAltitude : " + location.altitude);
}

void printRouterClients(PeplinkRouter &router)
{
    std::vector<PeplinkAPI_ClientInfo> clientList = router.clients();
    Serial.printf("%d clients: \n", clientList.size());
    for (PeplinkAPI_ClientInfo routerClient : clientList)
    {
        Serial.print("\tname : " + routerClient.name);
        Serial.print("\tid: " + routerClient.id);
        Serial.print("\tsecret: " + routerClient.secret);
        Serial.print("\tscope: ");
        Serial.print((routerClient.scope == CLIENT_SCOPE_READ_ONLY) ? String("api.read-only") : String("api"));
        Serial.println("\ttoken: " + routerClient.token);
    }
}

void printRouterWanStatus(PeplinkRouter &router)
{
    Serial.print("Getting WAN list - ");

    std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
    Serial.println(String(wanList.size()) + " elements:");
    for (PeplinkAPI_WAN *wan : wanList)
    {
        Serial.print("\tname : " + wan->name);
        Serial.print("\tstatus : " + wan->status);
        Serial.print("\tstatusLed : " + wan->statusLED);
        switch (wan->type)
        {
        case PEPLINKAPI_WAN_TYPE_ETHERNET:
            Serial.print("\ttype : ethernet");
            if (wan->status != "Disabled")
                Serial.print("\tip : " + wan->ip);
            break;
        case PEPLINKAPI_WAN_TYPE_CELLULAR:
        {
            Serial.print("\ttype : cellular");
            if (wan->status != "Disabled")
            {
                Serial.print("\tip : " + wan->ip);
                Serial.print("\tcarrier: " + ((PeplinkAPI_WAN_Cellular *)wan)->carrier);
                Serial.printf("\trssi: %d\n" + ((PeplinkAPI_WAN_Cellular *)wan)->rssi);
                if (((PeplinkAPI_WAN_Cellular *)wan)->carrier != "")
                {
                    Serial.print("" + ((PeplinkAPI_WAN_Cellular *)wan)->carrier + " " + ((PeplinkAPI_WAN_Cellular *)wan)->networkType + " Bars:" + ((PeplinkAPI_WAN_Cellular *)wan)->signalLevel + "\n");
                }
                printSimCards(((PeplinkAPI_WAN_Cellular *)wan)->simCards);
            }
        }
        break;
        case PEPLINKAPI_WAN_TYPE_WIFI:
        {
            Serial.print("\ttype : wifi");
            if (wan->status != "Disabled")
            {
                Serial.print("\tip : " + wan->ip);
                Serial.print("\tstrength: " + ((PeplinkAPI_WAN_WiFi *)wan)->strength);
                Serial.print("\tssid: " + ((PeplinkAPI_WAN_WiFi *)wan)->ssid);
                Serial.print("\tbssid: " + ((PeplinkAPI_WAN_WiFi *)wan)->bssid);
            }
        }
        break;
        }
        Serial.println();
    }
}

void printSimCards(std::vector<PeplinkAPI_WAN_Cellular_SIM> &simList)
{
    Serial.println("\t\tSIM Cards: ");
    for (auto it = 0; it < simList.size(); ++it)
    {
        Serial.printf("\t\tSIM %d: ", it);
        Serial.print((simList[it].detected) ? "detected" : "not detected");
        if (simList[it].detected)
        {
            Serial.print((simList[it].active) ? ", active" : ", inactive");
            Serial.print(", iccid - " + simList[it].iccid);
        }
        Serial.println();
    }
}