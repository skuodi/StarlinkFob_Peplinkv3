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
#include "SHT3X.h"
#include "QMP6988.h"

#include "PeplinkAPI.h"
#include "config.h"
#include "Minu/minu.hpp"

#define TFT_GREY 0x5AEB  // New colour

/// @brief User-defined credentials 
typedef struct
{
    // SSID of the first-priority Wi-Fi network the fob attempts to connect to in station mode
    char ssidStaPrimary[NAME_MAX_LEN];             
    char passwordStaPrimary[PASSWORD_MAX_LEN];
    // SSID of the second-priority Wi-Fi network the fob attempts to connect to in station mode after
    // connecting to the first-priority network fails
    char ssidStaSecondary[NAME_MAX_LEN];
    char passwordStaSecondary[PASSWORD_MAX_LEN];
    // SSID used for the local AP 
    char ssidSoftAp[NAME_MAX_LEN];
    char passwordSoftAp[PASSWORD_MAX_LEN];
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

typedef struct 
{
  char lastTempAlertTime[20];
  float lastTempAlertThresh;
  float lastTempAlertTemp;
} AlertTimestamp_t;

typedef struct 
{
  String ssidStaPrimary;
  String passwordStaPrimary;
  String ssidStaSecondary;
  String passwordStaSecondary;
  String ssidSoftAp;
  String passwordSoftAp;
  bool usePrimarySsid;
  bool timedOut;
  long timeoutMs;
  long timestamp;
}StarlinkFob_WifiState_t;

typedef struct 
{
  /// @brief Handle of the local HTTP server
  WebServer httpServer;
  /// @brief Handle used for firmware updates over HTTP
  HTTPUpdateServer updateServer;
  bool started;
}StarlinkFob_HttpServerState_t;

typedef struct 
{
  PeplinkRouter router;
  String ip;
  uint16_t port;
  String username;
  String password;
  String clientName;
  PeplinkAPI_ClientScope_t clientScope;
}StarlinkFob_RouterState_t;

typedef struct 
{
  SHT3X sht;
  QMP6988 qmp;
  bool shtAvailable;
  bool qmpAvailable;
}StarlinkFob_SensorState_t;

typedef enum
{
  STARLINKFOB_BUTTONPRESS_NONE,
  STARLINKFOB_BUTTONPRESS_SHORT,
  STARLINKFOB_BUTTONPRESS_LONG,
}StarlinkFob_ButtonPress_t;

typedef struct 
{
  int32_t pressDurationA;
  uint32_t lastPressTimeA;
  StarlinkFob_ButtonPress_t btnPressA;
  bool isPressedA;

  int32_t pressDurationB;
  uint32_t lastPressTimeB;
  StarlinkFob_ButtonPress_t btnPressB;
  bool isPressedB;

  int32_t pressDurationC;
  uint32_t lastPressTimeC;
  StarlinkFob_ButtonPress_t btnPressC;
  bool isPressedC;
}StarlinkFob_ButtonState_t;

typedef struct 
{
  String lastShutdownTime;
  String lastShutdownDate;
  String lastShutdownTimezone;
  uint64_t lastShutdownRuntime;

  String lastTempAlertTime;
  String lastTempAlertDate;
  float lastTempAlertThresh;
  float lastTempAlertTemp;
}StarlinkFob_TimestampState_t;

typedef struct
{
  TaskHandle_t countdown;
  TaskHandle_t screenWatch;
  TaskHandle_t screenUpdate;
  TaskHandle_t buttonWatch;
  TaskHandle_t dataUpdate;
  TaskHandle_t wifiWatch;
}StarlinkFob_TaskState_t;

typedef struct 
{
  bool booting;
  StarlinkFob_WifiState_t wifi;
  StarlinkFob_HttpServerState_t servers;
  StarlinkFob_RouterState_t routers;
  StarlinkFob_SensorState_t sensors;
  StarlinkFob_ButtonState_t buttons;
  StarlinkFob_TimestampState_t timestamps;
  StarlinkFob_TaskState_t tasks;
  Minu menu;
  /// @brief List of targets to be pinged
  std::vector<PingTarget> pingTargets;
}StarlinkFob_GlobalState_t;

extern StarlinkFob_GlobalState_t fob;

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

///@brief Retrieve info on last over-temperature alert
void retrieveLastAlertInfo();

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