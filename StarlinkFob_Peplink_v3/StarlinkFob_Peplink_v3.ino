#include "M5StickCPlus2.h"
#include "WiFi.h"
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include "time.h"
#include "esp_sntp.h"
#include "SHT3X.h"
#include "QMP6988.h"

#include "PeplinkAPI.h"
#include "secrets.h"
#include "config.h"
#include "utils.h"
#include "logo.h"

#include "Minu/minu.hpp"
#include "ui.h"
#include <vector>

StarlinkFob_GlobalState_t fob;

static IPAddress RouterExternalIP(209,55,112,219);
static IPAddress RouterLocalIP(192,168,50,1);
static IPAddress StarlinkIP(192, 168, 100, 1);
static IPAddress subnet(255, 255, 255, 0);
static IPAddress primaryDNS(8, 8, 8, 8);   // optional
static IPAddress secondaryDNS(8, 8, 4, 4); // optional

// static IPAddress rvRouterIP(192, 168, 1, 254);
//static IPAddress linkyM5IP(192, 168, 8, 10);
//static IPAddress linkyIP(192, 168, 100, 1);

static PingTarget routerExternal = {.displayHostname="Router External", .useIP=true, .pingIP=RouterExternalIP, .pinged=false, .pingOK=false};
static PingTarget routerLocal = {.displayHostname="Peplink Router", .useIP=true, .pingIP=RouterLocalIP, .pinged=false, .pingOK=false};
static PingTarget starlinkDish = {.displayHostname="Starlink Dish", .useIP=true, .pingIP=StarlinkIP, .pinged=false, .pingOK=false};
static PingTarget dns = {.displayHostname="DNS server", .useIP=true, .pingIP=primaryDNS, .pinged=false, .pingOK=false};
static PingTarget provisioning = {.displayHostname="Provisioning", .useIP=false, .fqn="m1p.vippbx.com", .pinged=false, .pingOK=false};
static PingTarget starlink = {.displayHostname="Starlink.com", .useIP=false, .fqn="www.starlink.com", .pinged=false, .pingOK=false};
static PingTarget google = {.displayHostname="Google.com", .useIP=false, .fqn="www.google.com", .pinged=false, .pingOK=false};

//static PingTarget linkyM5 = {.displayHostname="linkyM5stick", .useIP=true, .pingIP=linkyM5IP, .pinged=false, .pingOK=false};
//static PingTarget linky = {.displayHostname="linky", .useIP=true, .pingIP=linkyIP, .pinged=false, .pingOK=false};

#ifndef MIN
  #define MIN(x, y) (x < y) ? x : y
#endif

void printText(const char *msg, uint8_t len, uint16_t fore = 0xFFFF, uint16_t back = 0x0000)
{
  // Check that the message and message length are valid
  if (!msg || !len)
    return;
  
  // Set printing colours
  M5.Lcd.setTextColor(fore, back);

  // If printing the entire string, no modification is is necessary
  if (strlen(msg) == len)
  {
    M5.Lcd.print(msg);
    return;
  }

  // Otherwise, print the string.
  // If the string is too short, pad it with spaces and if too long, truncate it
  char buff[len + 1];
  memset(buff, ' ', sizeof(buff));
  memcpy(buff, msg, MIN(len, strlen(msg)));
  buff[len] = 0;

  M5.Lcd.print(buff);
}

void printTextInverted(const char *msg, uint8_t len, uint16_t fore = 0xFFFF, uint16_t back = 0x0000)
{
  // Check that the message and message length are valid
  if (!msg || !len)
    return;

  // Set printing colours
  M5.Lcd.setTextColor(back, fore);

  // If printing the entire string, no modification is is necessary
  if (strlen(msg) == len)
  {
    M5.Lcd.print(msg);
    return;
  }

  // Otherwise, print the string.
  // If the string is too short, pad it with spaces and if too long, truncate it
  char buff[len + 1];
  memset(buff, ' ', sizeof(buff));
  memcpy(buff, msg, MIN(len, strlen(msg)));
  buff[len] = 0;

  M5.Lcd.print(buff);
}

void setup()
{
  // Setup the serial terminal 
  Serial.begin(115200);

  // Initialize the filesystem, formatting a new filesystem on fail
  SPIFFS.begin(true);

  // Initialize the global M5 API object for the M5StickCPlus2
  auto cfg = M5.config();
  StickCP2.begin(cfg);

  // Set the orientation of the screen
  M5.Lcd.setRotation(1);
  // Set Lcd brightness. Doesn't seem to actually work
  M5.Lcd.setBrightness(1);
  
  Serial.println("Starting");
  showSplashScreen();

  // After showing splash screen, reset text settings to defaults before initializing the menu
  M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
  M5.Lcd.setTextSize(TEXT_SIZE_DEFAULT);
  M5.Lcd.setTextWrap(false, false);

  /// @brief Create the menu, defining the length of the main and auxiliary texe sections
  fob.menu.setPrintFunctions(printText, printTextInverted);
  fob.menu.setTextLength(MINU_MAIN_TEXT_LEN, MINU_AUX_TEXT_LEN);

  fob.booting = true;              // Set at boot to initiate the automated changing of pages after Wi-Fi connects
  fob.wifi.usePrimarySsid = true;  // Whether the or the secondary SSID should be used for Wi-Fi STA connection
  fob.wifi.timedOut = false;       // Indicates if Wi-Fi has timed out waiting for STA connection

  // Populate the list of network ping targets
  fob.pingTargets.push_back(routerExternal);
  // fob.pingTargets.push_back(routerLocal);
  fob.pingTargets.push_back(starlinkDish);
  fob.pingTargets.push_back(dns);
  fob.pingTargets.push_back(provisioning);    
  fob.pingTargets.push_back(starlink);
  fob.pingTargets.push_back(google);
  
  // Uncomment the following two lines to overwrite NVS values with their defaults on boot
  // in case of first-time flash initialization or corruption recovery
  // resetPreferences();
  // savePreferences();
  
  // Retrieve the cookie and token values from NVS
  retrieveStoredCredentials();

  // Retrieve user-defined credentials from NVS
  restorePreferences();

  // Retrieve last shutdown time information
  retrieveLastShutdownInfo();

  // Retrieve info on last over-temperature alert
  retrieveLastAlertInfo();

  // Set Wi-Fi hostname. This name shows up, for example, on the list of connected devices on the router settings page
  WiFi.setHostname(PEPLINK_FOB_NAME);

  // Initialize the menu system.
  // By the time this function returns, on success, the menu is initialized,
  // Wi-Fi is connected, targets have been pinged, and the list of router WANs has been fetched
  uiMenuInit();

  // If in STA mode but still unconnected, busy wait for connection or Wi-Fi mode change 
  while (WiFi.getMode() == WIFI_MODE_STA && !(WiFi.status() == WL_CONNECTED))
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  if(!fob.servers.started)
  {
    fob.servers.started = true;
    Serial.println("Starting HTTP Server");
    startHttpServer();
  }

  // Set the function to be called when network time is successfully synced
  sntp_set_time_sync_notification_cb([](struct timeval *t)
                                     {
  Serial.println("Got time adjustment from NTP!");
  syncNtpToRtc(TIMEZONE);
  });

  esp_sntp_servermode_dhcp(1);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER1, NTP_SERVER2);
}

void loop()
{
  if(fob.servers.started)
    fob.servers.httpServer.handleClient();
}

void showSplashScreen()
{
  Serial.println("SplashScreen");
  Serial.println("GOBOX PRO");
  M5.Lcd.fillScreen(LOGO_GREEN);
  M5.Lcd.setTextSize(5);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(BLACK, LOGO_GREEN);
  M5.Lcd.println();
  M5.Lcd.println("  GOBOX");
  M5.Lcd.println("   PRO  ");
  delay(6000);
  #ifdef USE_LOGO
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.drawBitmap(0, 5, 240, 93, logo, 0);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setTextColor(LOGO_GREEN, WHITE);
  M5.Lcd.setCursor(0, 108);
  M5.Lcd.println(" 469-257-1111 ");
  #else
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(3);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(BLACK, LOGO_GREEN);
  M5.Lcd.print("1");
  M5.Lcd.setTextColor(LOGO_GREEN, BLACK);
  M5.Lcd.print("SIMPLE\n");
  M5.Lcd.setTextColor(ORANGE, BLACK);
  M5.Lcd.print(" CONNECT\n");
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.print("\n");
  #endif
  M5.Lcd.println(" 469-257-1111 ");
  delay(5000);
  M5.Lcd.clear();
  M5.Lcd.setCursor(0,0);
}