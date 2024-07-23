/**
 * @file config.h
 * @brief Contains compile-time known definitions that override the functionality of the device
 */

#ifndef _PEPLINK_ESP32_CONF_H_
#define _PEPLINK_ESP32_CONF_H_

#include <Arduino.h>
#include "PeplinkAPI.h"
#include "TZ.h"         // Contains timezone definitions Source: https://github.com/esp8266/Arduino/blob/master/cores/esp8266/TZ.h

// Uncomment the following line to enable verbose debug logging
// #define ENABLE_VERBOSE_DEBUG_LOG
// #ifdef ENABLE_VERBOSE_DEBUG_LOG
// #define PEPLINK_DEBUG_LOG
#define UI_DEBUG_LOG
// #define WEBSERVER_DEBUG_LOG
// #endif

// Uncomment the following line to enable beeping on every button press
// #define UI_BEEP

// Uncomment to use Logo instead of text on splash screen
#define USE_LOGO

/// @brief Name used as the Wi-Fi hostname and device name
#define PEPLINK_FOB_NAME        "PeplinkFob"

/// @brief Name used when creating a new router client
#define CLIENT_NAME_DEFAULT     "Client1"
/// @brief Access rights used for a new created client
#define CLIENT_SCOPE_DEFAULT    CLIENT_SCOPE_READ_WRITE

/// @brief Millisecond duration to wait for Wi-Fi to connect in station mode before timing out
#define WIFI_TIMEOUT_DEFAULT    300000

/// @brief Maximum length used for Wi-Fi SSID's and WWW host names
#define NAME_MAX_LEN            32 

/// @brief Maximum length used for passwords 
#define PASSWORD_MAX_LEN        64

/// @brief Maximum length of an IPV4 address
#define IPV4_MAX_LEN            16

/// @brief Port used for the device's local HTTP server
#define HTTP_PORT               80

/// @brief Uncomment this to enable HTTPS
//#define PEPLINK_USE_HTTPS

#ifdef PEPLINK_USE_HTTPS
    #define ROUTER_PORT_DEFAULT 8443
#else
    #define ROUTER_PORT_DEFAULT 88
#endif

/// @brief Namespace where router-assigned credentials (cookies and tokens) are stored in NVS
/// A separate namespace is used since for router-assigned credentials
/// since they are modified under different conditions from user-defined credentials
#define COOKIES_NAMESPACE       "cookie-jar"

/// @brief Namespace where user-defined credentials are stored in NVS
#define PREFERENCES_NAMESPACE   "settings"

/// @brief Namespace where timestamps are stored in NVS
#define TIMESTAMP_NAMESPACE   "timestamps"

/// @brief Namespace where over-temperature alert logs are stored in NVS
#define TEMPERATURE_ALERT_NAMESPACE   "alert-log"

#define TEMPERATURE_ALERT_THRESH_F      120.f // Degrees farenheight

/// @brief Define NTP servers for time sync
#define NTP_SERVER1             "pool.ntp.org"
#define NTP_SERVER2             "time.nist.gov"

/// @brief Offset from GMT time in seconds
#define GMT_OFFSET_SEC          (3 * 3600)  

/// @brief Daylight saving time
#define DAYLIGHT_OFFSET_SEC     0     

/// @brief Timezone definition
#define TIMEZONE                TZ_America_Chicago


/// @brief Define the default text size for the menu system
///        This also defines the default length of the mennu item main and auxiliary text
#define TEXT_SIZE_DEFAULT 2

#if (TEXT_SIZE_DEFAULT == 2)
#define MINU_MAIN_TEXT_LEN 15
#define MINU_AUX_TEXT_LEN 5
#define MINU_ITEM_MAX_COUNT 6
#elif (TEXT_SIZE_DEFAULT == 3)
#define MINU_MAIN_TEXT_LEN 9
#define MINU_AUX_TEXT_LEN 3
#define MINU_ITEM_MAX_COUNT 3
#else
#error "TEXT_SIZE_DEFAULT must be defined with a value of either 2 or 3"
#endif

#define LONG_PRESS_THRESHOLD_MS     300
#define SHUTDOWN_PRESS_THRESHOLD_MS 1000

#define WIFI_COUNTDOWN_SECONDS      5 * 60
#define SHUTDOWN_COUNTDOWN_SECONDS  5
#define REBOOT_COUNTDOWN_SECONDS    5

#endif