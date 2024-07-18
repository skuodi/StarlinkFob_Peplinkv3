/**
 * @file  utils.h
 * @brief Contains common user definitions
 */

#ifndef _STARLINKFOB_UTILS_H_
#define _STARLINKFOB_UTILS_H_

#include <stdint.h>

#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include "PeplinkAPI.h"
#include "config.h"

#define TFT_GREY 0x5AEB  // New colour

/// @brief User-defined credentials 
typedef struct
{
    // SSID of the first-priority Wi-Fi network the fob attempts to connect to in station mode
    char wifi1Ssid[NAME_MAX_LEN];             
    char wifi1Password[PASSWORD_MAX_LEN];
    // SSID of the second-priority Wi-Fi network the fob attempts to connect to in station mode after
    // connecting to the first-priority network fails
    char wifi2Ssid[NAME_MAX_LEN];
    char wifi2Password[PASSWORD_MAX_LEN];
    // SSID used for the local AP 
    char wifiSsidSoftAp[NAME_MAX_LEN];
    char wifiPasswordSoftAp[PASSWORD_MAX_LEN];
    // Router IP address
    char ip[IPV4_MAX_LEN];
    // Router port
    uint16_t port;
    // Administrator username for logging into the router
    char username[NAME_MAX_LEN];
    // Administrator password
    char password[PASSWORD_MAX_LEN];
    // Default name used when creating a new client
    char clientName[NAME_MAX_LEN];
    // Default access rights for a newly created client
    PeplinkAPI_ClientScope_t clientScope;
    // Millisecond duration to wait for Wi-Fi to connect in station mode before timing out
    long wifiTimeoutMs;
}PeplinkAPI_Settings_t;

struct PingTarget {
  String displayHostname; // User-friendly name of the target
  bool useIP;             // ping by IP if true, by FQN if false
  IPAddress pingIP;       // IP to use when pinging
  String fqn;             // Fully-qualified name to use when pinging
  bool pinged;            // Whether or not a ping has been sent to this host
  bool pingOK;            // result of ping
};

typedef struct 
{
  char lastShutdownTime[20];
  char lastShutdownTimezone[20];
  uint64_t lastShutdownRuntime;
} ShutdownTimestamp_t;

/// @brief Handle of the local HTTP server
extern WebServer httpServer;

/// @brief Handle used for firmware updates over HTTP
extern HTTPUpdateServer httpUpdater;

/// @brief List of targets to be pinged
extern std::vector<PingTarget> pingTargets;

/// Runtime variables used to hold user-defined credentials.
extern String wifi1Ssid;
extern String wifi1Password;
extern String wifi2Ssid;
extern String wifi2Password;
extern String wifiSsidSoftAp;
extern String wifiPasswordSoftAp;
extern String routerIP;
extern uint16_t routerPort;
extern String routerUsername;
extern String routerPassword;
extern String routerClientName;
extern PeplinkAPI_ClientScope_t routerClientScope;
extern long wifiTimeoutMs;
extern long wifiTimestamp;

extern String lastShutdownTime;
extern String lastShutdownTimezone;
extern uint64_t lastShutdownRuntime;

extern Preferences cookiePrefs;
extern Preferences routerPrefs;
extern PeplinkRouter router;

extern bool httpServerStarted;

/// @brief Overwrite user credentials in runtime variables with their hard-coded default values 
void resetPreferences();

/// @brief Save the current values of user-defined credentials in runtime variables to non-volatile storage
/// @return true, on success
/// @return false, on fail
bool savePreferences();

/// @brief Print out the current contents of user-defined credentials in runtime variables to the serial console
void dumpPreferences();

/// @brief Copy router-assigned credentials from non-volatile storage to their corresponding runtime variables
void retrieveStoredCredentials();

/// @brief Copy user-defined credentials from non-volatile storage to their corresponding runtime variables
void restorePreferences();

void retrieveLastShutdownInfo();

/// @brief Start the local webserver
void startHttpServer();

/// @brief Modify the timezone of network-synced time
void setTimezone(const char* tzone);

/// @brief Synchronize local RTC time to network time while setting the default timezone
bool syncNtpToRtc(const char* tzone);

/// @brief Convenience function to print router information to serial 
void printRouterInfo(PeplinkRouter &router);

/// @brief Convenience function to print router location to serial 
void printRouterLocation(PeplinkRouter &router);

/// @brief Convenience function to print router clients to serial 
void printRouterClients(PeplinkRouter &router);

/// @brief Convenience function to print router WAN information to serial 
void printRouterWanStatus(PeplinkRouter &router);

/// @brief Convenience function to print router cellular WAN information to serial 
void printSimCards(std::vector<PeplinkAPI_WAN_Cellular_SIM> &simList);

#endif