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
    wifi1Ssid = WIFI1_SSID_DEFAULT;
    wifi1Password = WIFI1_PASSWORD_DEFAULT;
    wifi2Ssid = WIFI2_SSID_DEFAULT;
    wifi2Password = WIFI2_PASSWORD_DEFAULT;
    wifiSsidSoftAp = WIFI_SSID_SOFTAP_DEFAULT;
    wifiPasswordSoftAp = WIFI_PASSWORD_SOFTAP_DEFAULT;
    routerIP = ROUTER_IP_DEFAULT;
    routerPort = ROUTER_PORT_DEFAULT;
    routerUsername = ROUTER_USERNAME_DEFAULT;
    routerPassword = ROUTER_PASSWORD_DEFAULT;
    routerClientName = CLIENT_NAME_DEFAULT;
    routerClientScope = CLIENT_SCOPE_DEFAULT;
    wifiTimeoutMs = WIFI_TIMEOUT_DEFAULT;
}

bool savePreferences()
{
    bool ret = false;
    PeplinkAPI_Settings_t newSettings =
        {
            .port = routerPort,
            .clientScope = routerClientScope,
            .wifiTimeoutMs = wifiTimeoutMs,
        };
    strncpy(newSettings.wifi1Ssid, wifi1Ssid.c_str(), sizeof(newSettings.wifi1Ssid));
    strncpy(newSettings.wifi1Password, wifi1Password.c_str(), sizeof(newSettings.wifi1Password));
    strncpy(newSettings.wifi2Ssid, wifi2Ssid.c_str(), sizeof(newSettings.wifi2Ssid));
    strncpy(newSettings.wifi2Password, wifi2Password.c_str(), sizeof(newSettings.wifi2Password));
    strncpy(newSettings.wifiSsidSoftAp, wifiSsidSoftAp.c_str(), sizeof(newSettings.wifiSsidSoftAp));
    strncpy(newSettings.wifiPasswordSoftAp, wifiPasswordSoftAp.c_str(), sizeof(newSettings.wifiPasswordSoftAp));
    strncpy(newSettings.ip, routerIP.c_str(), sizeof(newSettings.ip));
    strncpy(newSettings.username, routerUsername.c_str(), sizeof(newSettings.username));
    strncpy(newSettings.password, routerPassword.c_str(), sizeof(newSettings.password));
    strncpy(newSettings.clientName, routerClientName.c_str(), sizeof(newSettings.clientName));

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
    Serial.println("Router SSID1 : " + wifi1Ssid);
    Serial.println("Router Password1 :" + wifi1Password);
    Serial.println("Router SSID2 : " + wifi2Ssid);
    Serial.println("Router Password2 :" + wifi2Password);
    Serial.println("Soft AP SSID : " + wifiSsidSoftAp);
    Serial.println("Soft AP Password :" + wifiPasswordSoftAp);
    Serial.println("Router IP : " + routerIP);
    Serial.println("Router Port : " + String(routerPort));
    Serial.println("Router API Username : " + routerUsername);
    Serial.println("Router API Password : " + routerPassword);
    Serial.println("Router Client Name : " + routerClientName);
    Serial.print("Router Client Scope : ");
    if (routerClientScope == CLIENT_SCOPE_READ_WRITE)
        Serial.println("read-write");
    else
        Serial.println("read-only");
    Serial.println("Wi-Fi connect timeout : " + String(wifiTimeoutMs));
}

void retrieveStoredCredentials()
{
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
                router.setCookie(String(jar->cookie));
                Serial.print("Got cookie from storage: ");
                Serial.println(jar->cookie);
                router.setToken(String(jar->token));
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

    if (routerPrefs.begin(PREFERENCES_NAMESPACE))
    {
        size_t settingsSize = routerPrefs.getBytesLength(PREFERENCES_NAMESPACE);

        uint8_t settingsBuffer[settingsSize];
        routerPrefs.getBytes(PREFERENCES_NAMESPACE, settingsBuffer, sizeof(settingsBuffer));
        routerPrefs.end();
        if (settingsSize % sizeof(PeplinkAPI_Settings_t) == 0)
        {
            PeplinkAPI_Settings_t *settings = (PeplinkAPI_Settings_t *)settingsBuffer;
            if (String(settings->wifi1Ssid).length() && String(settings->wifi2Ssid).length() &&
            String(settings->wifiSsidSoftAp).length() && String(settings->ip).length() &&
            String(settings->port).length() && String(settings->username).length() &&
            String(settings->password).length() && String(settings->clientName).length()
            )
            {
                wifi1Ssid = settings->wifi1Ssid;
                wifi1Password = settings->wifi1Password;
                wifi2Ssid = settings->wifi2Ssid;
                wifi2Password = settings->wifi2Password;
                wifiSsidSoftAp = settings->wifiSsidSoftAp;
                wifiPasswordSoftAp = settings->wifiPasswordSoftAp;
                routerIP = settings->ip;
                routerPort = settings->port;
                routerUsername = settings->username;
                routerPassword = settings->password;
                routerClientName = settings->clientName;
                routerClientScope = settings->clientScope;
                wifiTimeoutMs = settings->wifiTimeoutMs;
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
            lastShutdownRuntime = timestamp->lastShutdownRuntime;
            lastShutdownTime = timestamp->lastShutdownTime;
            lastShutdownTimezone = timestamp->lastShutdownTimezone;
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
            lastAlertTime = timestamp->lastAlertTime;
            lastAlertTemp = timestamp->lastAlertTemp;
            lastAlertThresh = timestamp->lastAlertThresh;
            Serial.println("Got last temp alert details:");
            Serial.printf("\tTime: %s\n", timestamp->lastAlertTime);
            Serial.printf("\tTemp: %fF\n",  timestamp->lastAlertTemp);
            Serial.printf("\tThresh: %fF\n",  timestamp->lastAlertThresh);
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