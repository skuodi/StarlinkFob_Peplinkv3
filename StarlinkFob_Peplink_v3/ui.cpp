#include "M5StickCPlus2.h"
#include <WiFi.h>
#include <ArduinoUniqueID.h>

#include "Minu/minu.hpp"
#include "ui.h"
#include "utils.h"
#include "config.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

/// @brief Conditions that trigger the countdown page
typedef enum
{
  UI_COUNTDOWN_TYPE_WIFI = 1,     // Wi-Fi STA connecting countdown
  UI_COUNTDOWN_TYPE_SHUTDOWN,
  UI_COUNTDOWN_TYPE_REBOOT,
  UI_COUNTDOWN_TYPE_FACTORY_RESET,
} UiCountdownType;

/// @brief Defines the data to be fetched periodically
typedef enum
{
  UI_UPDATE_TYPE_TIME = 1,
  UI_UPDATE_TYPE_ROUTER_INFO,
  UI_UPDATE_TYPE_FOB_INFO,
  UI_UPDATE_TYPE_PING,
  UI_UPDATE_TYPE_WAN
} UiUpdateType;

/// @brief Information about a Wi-Fi network found during a Wi-Fi scan
typedef struct
{
  String ssid;
  int32_t rssi;
  uint8_t encType;
  uint8_t mac[10];
  int channel;
} UiWiFiScannedNetwork;

TaskHandle_t countdownTaskHandle = NULL;
TaskHandle_t screenWatchTaskHandle = NULL;
TaskHandle_t screenUpdateTaskHandle = NULL;
TaskHandle_t buttonWatchTaskHandle = NULL;
TaskHandle_t dataUpdateTaskHandle = NULL;
TaskHandle_t wifiWatchTaskHandle = NULL;

size_t homePageId;
size_t wifiPageId;
size_t wifiPromptPageId;
size_t scanResultPageId;
size_t saveSSIDAsPageId;
size_t pingTargetsPageId;
size_t routerPageId;
size_t routerInfoPageId;
size_t routerLocationPageId;
size_t routerWANListPageId;
size_t routerWANInfoPageId;
size_t routerWANSummaryPageId;
size_t timePageId;
size_t countdownPageId;
size_t fobInfoPageId;
size_t factoryResetPageId;
size_t simListPageId;
size_t simInfoPageId;
size_t lastVisitedPageId;

static size_t wifiStatusItem;
static ssize_t homepageWifiItem;

static bool booting = true;           // Set at boot to initiate the automated changing of pages after Wi-Fi connects
static bool primarySsidValid = true;  // Whether the or the secondary SSID should be used for Wi-Fi STA connection
bool wifiTimeout = false;             // Indicates if Wi-Fi has timed out waiting for STA connection

long lastButtonADownTime = 0;
long lastButtonBDownTime = 0;
long lastButtonCDownTime = 0;
long currentTime = 0;
bool longPressA;
bool longPressB;
bool longPressC;
bool shortPressA;
bool shortPressB;

static String tempSSID;
static String lastSelectedWAN;
static size_t lastSelectedSim;

void buttonWatchTask(void *arg);
void screenWatchTask(void *arg);
void screenUpdateTask(void *arg);
void countdownTask(void *arg);
void dataUpdateTask(void *arg);
void wifiWatchTask(void* arg);

void goToFobInfoPage(void *arg = NULL)
{
  menu.goToPage(fobInfoPageId);
}

void goToHomePage(void *arg = NULL)
{
  booting = false;
  menu.goToPage(homePageId);
}

void goToWiFiPage(void *arg = NULL)
{
  menu.goToPage(wifiPageId);
}

void goToTimePage(void *arg = NULL)
{
  menu.goToPage(timePageId);
}

void goToWiFiPromptPage(void *arg = NULL)
{
  menu.goToPage(wifiPromptPageId);
}

void goToScanResultPage(void *arg = NULL)
{
  menu.goToPage(scanResultPageId);
}

void goToCountdownPage(void *arg = NULL)
{
  menu.goToPage(countdownPageId);
}

void goToRouterPage(void *arg = NULL)
{
  menu.goToPage(routerPageId);
}

void goToRouterInfoPage(void *arg = NULL)
{
  menu.goToPage(routerInfoPageId);
}

void goToRouterLocationPage(void *arg = NULL)
{
  menu.goToPage(routerLocationPageId);
}

void goToRouterWANListPage(void *arg = NULL)
{
  menu.goToPage(routerWANListPageId);
}

void goToRouterWANInfoPage(void *arg = NULL)
{
  menu.goToPage(routerWANInfoPageId);
}

void goToWANSummaryPage(void* arg = NULL)
{
  menu.goToPage(routerWANSummaryPageId);
}

void goToPingTargetsPage(void *arg = NULL)
{
  menu.goToPage(pingTargetsPageId);
}

void goToFactoryResetPage(void* arg = NULL)
{
  menu.goToPage(factoryResetPageId);
}

void goToSimListPage(void* arg = NULL)
{
  menu.goToPage(simListPageId);
}

void goToSimInfoPage(void* arg = NULL)
{
  menu.goToPage(simInfoPageId);
}

/// @brief Generic function to be called just after the current active page changes
/// @param arg A pointer to the currently active page is passed
void pageOpenedCallback(void *arg)
{
  if (!arg)
    return;
  MinuPage *thisPage = (MinuPage *)arg;
  thisPage->highlightItem(0);
  Serial.printf("Opened page: %s\n", thisPage->title());
}

/// @brief Generic function to be called just before the current active page changes
/// @param arg A pointer to the old active page is passed
void pageClosedCallback(void *arg)
{
  if (!arg)
    return;
  MinuPage *thisPage = (MinuPage *)arg;

  Serial.printf("Closed page: %s\n", thisPage->title());
}

/// @brief Generic function called when the current active page is rendered
/// @param arg A pointer to the currently active page is passed
void pageRenderedCallback(void *arg)
{
  if (!arg)
    return;
  MinuPage *thisPage = (MinuPage *)arg;

  Serial.printf("Rendered page: %s\n", thisPage->title());
}

/// @brief Changes the colour of the Wi-Fi status indicator on the homepage
/// AP  (active) = Green
/// STA (not connected) = Red
/// STA (connected) = Green
/// Wi-Fi not initialized = Grey
void updateWiFiItem(void *arg)
{
  if (!arg)
    return;

  MinuPageItem *thisItem = (MinuPageItem *)arg;

  if (WiFi.getMode() == WIFI_MODE_AP)
    thisItem->setAuxTextBackground(GREEN);
  else if (WiFi.getMode() == WIFI_MODE_STA)
  {
    if (WiFi.status() == WL_CONNECTED)
      thisItem->setAuxTextBackground(GREEN);
    else
      thisItem->setAuxTextBackground(RED);
  }
  else
    thisItem->setAuxTextBackground(TFT_GREY);
}

/// @brief Prints the Wi-Fi status and details when the Wi-Fi status item (<--) on the Wi-Fi page is highlighted
void lcdPrintWiFiStatus(void *page)
{
  if (page)
    if (menu.currentPageId() == wifiPageId && (*(MinuPage *)page).highlightedIndex() != wifiStatusItem)
      return;

  M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
  if (WiFi.getMode() == WIFI_MODE_NULL)
  {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("WiFi not INIT!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  else
    M5.Lcd.printf("\n%s:", WiFi.getMode() == WIFI_MODE_AP ? "(AP) " : "(STA)");

  if ((WiFi.status() == WL_CONNECTED) || WiFi.getMode() == WIFI_MODE_AP)
  {
    M5.Lcd.setTextColor(BLACK, GREEN);
    M5.Lcd.println(" ");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);

    M5.Lcd.print("SSID :");
    M5.Lcd.println((WiFi.getMode() == WIFI_MODE_AP) ? WiFi.softAPSSID().c_str() : WiFi.SSID().c_str());

    M5.Lcd.print("IPAdr:");
    M5.Lcd.println((WiFi.getMode() == WIFI_MODE_AP) ? WiFi.softAPIP().toString().c_str() : WiFi.localIP().toString().c_str());

    return;
  }
  M5.Lcd.setTextColor(BLACK, RED);
  M5.Lcd.println(" ");
  M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
}

/// @brief Fetch and print the router hardware information
void lcdPrintRouterInfo(void *arg = NULL)
{
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  M5.Lcd.println("Fetching...");

  if (!router.isAvailable() || !router.getInfo())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.printf("Name  :%s\n", router.info().name.c_str());
  M5.Lcd.printf("Uptime:%ld\n", router.info().uptime);
  M5.Lcd.printf("Serial:%s\n", router.info().serial.c_str());
  M5.Lcd.printf("Fw Ver:%s\n", router.info().fwVersion.c_str());
  M5.Lcd.printf("P Code:%s\n", router.info().productCode.c_str());
  M5.Lcd.printf("hw Rev:%s\n", router.info().hardwareRev.c_str());
}

/// @brief Fetch and print the router location information
void lcdPrintRouterLocation(void *arg = NULL)
{
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  M5.Lcd.println("Fetching...");

  if (!router.isAvailable() || !router.getLocation())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.printf("Longitude:%s\n", router.location().longitude.c_str());
  M5.Lcd.printf("Latitude :%s\n", router.location().latitude.c_str());
  M5.Lcd.printf("Altitude :%s\n", router.location().altitude.c_str());
}

/// @brief Fetch and print the selected WAN information
void lcdPrintRouterWANInfo(void *arg = NULL)
{
  if(!router.getWanStatus())
  {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }

  std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
  if (!wanList.size())
  {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("No WAN found!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }

  for (PeplinkAPI_WAN *wan : wanList)
  {
    if (!strcmp(wan->name.c_str(), lastSelectedWAN.c_str()))
    {
      M5.Lcd.printf("Name:%s\n", wan->name.c_str());
      M5.Lcd.print("Type:");
      switch (wan->type)
      {
      case PEPLINKAPI_WAN_TYPE_ETHERNET:
        M5.Lcd.print("ETHERNET\n");
        break;
      case PEPLINKAPI_WAN_TYPE_CELLULAR:
        M5.Lcd.print("CELLULAR\n");
        M5.Lcd.printf("Carr:%s %s\n",((PeplinkAPI_WAN_Cellular *)wan)->carrier.c_str(), ((PeplinkAPI_WAN_Cellular *)wan)->networkType);
        break;
      case PEPLINKAPI_WAN_TYPE_WIFI:
        M5.Lcd.print("WIFI\n");
        break;
      }
      M5.Lcd.printf("Stat:%s\n", wan->status.c_str());

      if(wan->status == "Disabled")
        break;
      M5.Lcd.printf("IP  :%s\n", wan->ip.c_str());
      M5.Lcd.print("U/D :");
      M5.Lcd.printf("%ld/%ld %s     \n", wan->upload, wan->download, wan->unit);
    }
  }
}

/// @brief Sync RTC time with NTP server and RTC time
void lcdPrintTime(void *arg = NULL)
{
	syncNtpToRtc(TIMEZONE);
  auto dt = M5.Rtc.getDateTime();
  Serial.printf("%02d:%02d:%02dH\n%02d/%02d/%04d\n",
                dt.time.hours, dt.time.minutes, dt.time.seconds,
                dt.date.date, dt.date.month, dt.date.year);
  M5.Lcd.printf("%02d:%02d:%02dH\n%02d/%02d/%04d\n",
                dt.time.hours, dt.time.minutes, dt.time.seconds,
                dt.date.date, dt.date.month, dt.date.year);
}

/// @brief Get the status of WAN connections
void printRouterWanStatus(void* arg = NULL)
{
  Serial.println("Getting WAN list - ");
  M5.Lcd.setCursor(0, 0, 1);
  M5.Lcd.println("Getting WAN list");

  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  if (!router.isAvailable() || !router.getWanStatus())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  else
  {
    M5.Lcd.clear();
    M5.Lcd.setCursor(cursorX, cursorY);
    std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
    Serial.println(String(wanList.size()) + " elements:");
    for (PeplinkAPI_WAN *wan : wanList)
    {
      Serial.print("\tname : " + wan->name);
      Serial.print("\tstatus : " + wan->status);
      // Serial.print("\tstatusLed : " + wan->statusLED);
      // M5.Lcd.print(wan->name +" " + wan->status + " " + wan->statusLED + "\n");
      if (wan->status != "Disabled")
      {
        // M5.Lcd.print(wan->name +" " + wan->statusLED + "\n");
        if (wan->statusLED == "red")
          M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
        if (wan->statusLED == "yellow")
          M5.Lcd.setTextColor(TFT_WHITE, TFT_YELLOW);
        if (wan->statusLED == "green")
          M5.Lcd.setTextColor(TFT_WHITE, TFT_GREEN);
        if (wan->statusLED == "flash")
          M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
        M5.Lcd.print(" ");
        // M5.Lcd.print(wan->name);
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.print(" " + wan->name + "\n");
        M5.Lcd.println("" + wan->status);
        // M5.Lcd.print(wan->name +" " + wan->status + "\n");
      }
      else
      {
        // M5.Lcd.print(wan->name +" " + wan->statusLED +" " + wan->status + "\n");
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREY);
        M5.Lcd.print(" ");
        M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
        M5.Lcd.print(" " + wan->name + "\n");
        // M5.Lcd.setTextColor(TFT_BLACK,TFT_WHITE);
        // M5.Lcd.print(wan->name);
        // M5.Lcd.print(" " + wan->status + "\n");
      }
      switch (wan->type)
      {
      case PEPLINKAPI_WAN_TYPE_ETHERNET:
        Serial.print("\ttype : ethernet");
        if (wan->status != "Disabled")
          Serial.print("\tip : " + wan->ip);
        // M5.Lcd.print("IP: " + wan->ip + "\n");
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
            M5.Lcd.print("" + ((PeplinkAPI_WAN_Cellular *)wan)->carrier + " " + ((PeplinkAPI_WAN_Cellular *)wan)->networkType + " Bars:" + ((PeplinkAPI_WAN_Cellular *)wan)->signalLevel + "\n");
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
}

/// @brief Print information about the fob's current status
void lcdPrintFobInfo(void *arg = NULL)
{
  M5.Lcd.printf("Name:" PEPLINK_FOB_NAME "\n");
  M5.Lcd.printf("Time:%ld\n", millis());
  M5.Lcd.printf("HWID:0x");
  for (size_t i = 0; i < UniqueIDsize; i++)
    M5.Lcd.print(UniqueID[i], HEX);
  M5.Lcd.printf("\nBATT:%u%%\n", M5.Power.getBatteryLevel());
}

/// @brief Stop the task that periodically performs HTTP requests
void stopDataUpdate(void *arg = NULL)
{
  if (dataUpdateTaskHandle)
  {
    vTaskDelete(dataUpdateTaskHandle);
    dataUpdateTaskHandle = NULL;
  }
}

/// @brief Check whether ping targets can be reached
void updatePingTargetsStatus(void *arg = NULL)
{
  size_t targetCount = pingTargets.size();
  Serial.printf("Pinging %d targets...\n", targetCount);

  for (size_t i = 0; i < targetCount; ++i)
  {
    if (pingTargets[i].useIP)
      pingTargets[i].pingOK = Ping.ping(pingTargets[i].pingIP, 5);
    else
      pingTargets[i].pingOK = Ping.ping(pingTargets[i].fqn.c_str(), 5);
    pingTargets[i].pinged = true;

    if (pingTargets[i].pingOK)
      menu.pages()[pingTargetsPageId]->items()[i].setAuxTextBackground(GREEN);
    else
      menu.pages()[pingTargetsPageId]->items()[i].setAuxTextBackground(RED);
    Serial.printf("Target %d(%s) -> ping %s\n", i, pingTargets[i].pingIP.toString().c_str(), (pingTargets[i].pingOK) ? "OK" : "FAIL");
    if (screenUpdateTaskHandle)
      xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
  }
}

/// @brief Starts the task that periodically performs HTTP requests
void startDataUpdate(void *arg = NULL)
{
  UiUpdateType ut;
  if (menu.currentPageId() == timePageId)
    ut = UI_UPDATE_TYPE_TIME;
  else if (menu.currentPageId() == routerWANInfoPageId)
    ut = UI_UPDATE_TYPE_ROUTER_INFO;
  else if (menu.currentPageId() == fobInfoPageId)
    ut = UI_UPDATE_TYPE_FOB_INFO;
  else if (menu.currentPageId() == pingTargetsPageId)
  {
    ut = UI_UPDATE_TYPE_PING;
    for (auto item : menu.currentPage()->items())
      item.setAuxTextBackground(MINU_BACKGROUND_COLOUR_DEFAULT);
    menu.currentPage()->highlightItem(0);
    if (screenUpdateTaskHandle)
      xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
  }
  else if(menu.currentPageId() == routerWANSummaryPageId)
    ut = UI_UPDATE_TYPE_WAN;
  else
    return;

  // If the data update task is already running, delete and create it afresh
  stopDataUpdate();
  xTaskCreatePinnedToCore(dataUpdateTask, "Data Update", 4096, (void *)ut, 2, &dataUpdateTaskHandle, ARDUINO_RUNNING_CORE);
}

void startWiFiConnectCountdown(void *arg = NULL)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    goToHomePage();
    return;
  }
  menu.pages()[countdownPageId]->setTitle("WAITING FOR WIFI");

  wifiTimeout = false;
  if (menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (countdownTaskHandle)
    vTaskDelete(countdownTaskHandle);
  xTaskCreatePinnedToCore(countdownTask, "WiFi Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_WIFI, 2, &countdownTaskHandle, ARDUINO_RUNNING_CORE);

  if (WiFi.getMode() != WIFI_MODE_STA)
  {
    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    WiFi.mode(WIFI_MODE_STA);

    if (booting && !primarySsidValid)
    {
      Serial.printf("Connecting to Wi-Fi: SSID - '%s', Pass - '%s'\n", wifi2Ssid.c_str(), wifi2Password.c_str());
      WiFi.begin(wifi2Ssid.c_str(), wifi2Password.c_str());
    }
    else
    {
      Serial.printf("Connecting to Wi-Fi: SSID - '%s', Pass - '%s'\n", wifi1Ssid.c_str(), wifi1Password.c_str());
      WiFi.begin(wifi1Ssid.c_str(), wifi1Password.c_str());
    }
  }
}

/// @brief Start the Wi-Fi access point
void startWiFiAP(void *arg)
{
  if (WiFi.getMode() != WIFI_MODE_AP)
  {
    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    WiFi.mode(WIFI_MODE_AP);
    Serial.printf("Starting softAP...\tSSID:'%s'\tPassword:'%s'\n", wifiSsidSoftAp.c_str(), wifiPasswordSoftAp.c_str());
    WiFi.softAP(wifiSsidSoftAp.c_str(), wifiPasswordSoftAp.c_str());
    wifiTimeout = true;
  }
  delay(1000);
  goToWiFiPage();
}

/// @brief Fetch and display list of available sim cards
void showSimList(void *arg)
{
  while (!menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching SIM list");
  
  std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
  if (!wanList.size())
  {
    menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
    Serial.println("No WAN found!");
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.println("No WAN found!");
    return;
  }
  ssize_t thisItem;
  for (PeplinkAPI_WAN *wan : wanList)
  {
    if(wan->type == PEPLINKAPI_WAN_TYPE_CELLULAR)
    {
      std::vector<PeplinkAPI_WAN_Cellular_SIM> &simList = ((PeplinkAPI_WAN_Cellular *)wan)->simCards;
        ssize_t thisItem;

      for (auto it = 0; it < simList.size(); ++it)
      {
          thisItem = menu.pages()[simListPageId]->addItem(goToSimInfoPage, String("SIM" + String(it)).c_str(), " ");
        
        if(!simList[it].detected)
            menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(TFT_GREY);
        else if (simList[it].active)
            menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(GREEN);
        else
          menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(RED);
      }    
    }

  }
  menu.pages()[simListPageId]->addItem(goToRouterPage, "<--", NULL);
  menu.pages()[simListPageId]->highlightItem(0);
  if (screenUpdateTaskHandle)
    xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
}

/// @brief Show information about a particular SIM card
void showSimInfo(void* arg = NULL)
{
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching SIM INFO");
  
  std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
  if (!wanList.size())
  {
    Serial.println("No WAN found!");
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.println("No WAN found!");
    return;
  }
  ssize_t thisItem;
  for (PeplinkAPI_WAN *wan : wanList)
  {
    if(wan->type == PEPLINKAPI_WAN_TYPE_CELLULAR)
    {
      Serial.println("Got Cellular WAN");

      std::vector<PeplinkAPI_WAN_Cellular_SIM> &simList = ((PeplinkAPI_WAN_Cellular *)wan)->simCards;

      M5.Lcd.setCursor(cursorX, cursorY);
      if(lastSelectedSim >= simList.size())
      {
        Serial.println("No SIM found!");
        M5.Lcd.println("No SIM found!");
        return;
      }
      M5.Lcd.printf("Name  :SIM%d\n", lastSelectedSim);
      M5.Lcd.printf("Active:%s\n", simList[lastSelectedSim].active ? "true" : "false");
      M5.Lcd.printf("ICCID :%s\n", simList[lastSelectedSim].iccid.c_str());  
      break;
    }
  }
}

/// @brief Create the list of available WANs
void getWanList(void *arg)
{
  while (!menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching WAN list");
  M5.Lcd.println("Fetching...");

  if (!router.isAvailable() || !router.getWanStatus())
  {
    Serial.println("Fetching WAN list failed!");
    menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }

  std::vector<PeplinkAPI_WAN *> wanList = router.wanStatus();
  if (!wanList.size())
  {
    menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
    Serial.println("No WAN found!");
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.println("No WAN found!");
    return;
  }
  ssize_t thisItem;
  for (PeplinkAPI_WAN *wan : wanList)
  {
    thisItem = menu.pages()[routerWANListPageId]->addItem(goToRouterWANInfoPage, wan->name.c_str(), " ");

    if (!strcmp(wan->statusLED.c_str(), "red"))
      menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(RED);
    else if (!strcmp(wan->statusLED.c_str(), "yellow"))
      menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(YELLOW);
    else if (!strcmp(wan->statusLED.c_str(), "green"))
      menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(GREEN);
    else if (!strcmp(wan->statusLED.c_str(), "flash"))
      menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(WHITE);
    else
      menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(TFT_GREY);
  }
  menu.pages()[routerWANListPageId]->addItem(goToWANSummaryPage, "WAN Summary", NULL);
  menu.pages()[routerWANListPageId]->addItem(goToSimListPage, "SIM Cards", NULL);
  menu.pages()[routerWANListPageId]->addItem(goToRouterPage, "<--", NULL);
  menu.pages()[routerWANListPageId]->highlightItem(0);
  if (screenUpdateTaskHandle)
    xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
}

/// @brief Stop waiting for Wi-Fi to connect and go to homepage
void cancelWiFiSetup(void *arg)
{
  WiFi.mode(WIFI_MODE_NULL);
  menu.goToPage(lastVisitedPageId);
}

/// @brief Save the selected SSID as primary or secondary
void saveSSID(void *arg)
{
  if (!arg)
    return;

  MinuPageItem *thisItem = (MinuPageItem *)arg;

  Serial.printf("Saved %s SSID: '%s'\n", thisItem->mainText(), tempSSID.c_str());
  if (!strcmp(thisItem->mainText(), "Primary"))
  {
    wifi1Ssid = tempSSID;
    primarySsidValid = true;
  }
  else
    wifi2Ssid = tempSSID;
  savePreferences();
  goToWiFiPage();
}

/// @brief Save selected SSID
void saveItemSSID(void *arg)
{
  if (!arg)
    return;

  MinuPageItem *thisItem = (MinuPageItem *)arg;
  tempSSID = thisItem->mainText();

  menu.goToPage(saveSSIDAsPageId);
}

/// @brief Delete all child items of a menu page
void deleteAllPageItems(void *arg)
{
  if (!arg)
    return;

  MinuPage *thisPage = (MinuPage *)arg;
  thisPage->removeAllItems();
}

void wanListPageClosed(void *arg)
{
  if (!arg)
    return;
  MinuPage *thisPage = (MinuPage *)arg;
  lastSelectedWAN = thisPage->highlightedItem().mainText();
  deleteAllPageItems(arg);
}

void simListPageClosed(void *arg)
{
  if (!arg)
    return;
  MinuPage *thisPage = (MinuPage *)arg;
  lastSelectedSim = thisPage->highlightedIndex();
  deleteAllPageItems(arg);
}

/// @brief Perform a Wi-Fi scan
void startWiFiScan(void *arg)
{
  goToScanResultPage();
  if (screenUpdateTaskHandle)
    xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
  while (!menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  wifiTimeout = true;
  WiFi.mode(WIFI_MODE_NULL);
  delay(100);
  WiFi.mode(WIFI_MODE_STA);

  M5.Lcd.println("Scanning...");
  int n = WiFi.scanNetworks();
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.print("Scan ");

  if (n == 0)
    M5.Lcd.println("done. 0 found.");
  else if (n < 0)
    M5.Lcd.printf("error %d!\n", n);
  else if (n > 0)
  {
    M5.Lcd.printf("done. %d found\n", n);

    for (int i = 0; i < n; ++i)
      menu.pages()[scanResultPageId]->addItem(saveItemSSID, WiFi.SSID(i).c_str(), String(WiFi.RSSI(i)).c_str());

    WiFi.scanDelete();
  }
    
  delay(3000);
  menu.pages()[scanResultPageId]->addItem(goToWiFiPage, "<--", NULL);
  if (screenUpdateTaskHandle)
    xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
    
  while (!menu.rendered())
    delay(10);
}

void startShutdownCountdown(void *arg = NULL)
{
  menu.pages()[countdownPageId]->setTitle("SHUTDOWN");
  if (menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (countdownTaskHandle)
    vTaskDelete(countdownTaskHandle);
  xTaskCreatePinnedToCore(countdownTask, "Shutdown Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_SHUTDOWN, 2, &countdownTaskHandle, ARDUINO_RUNNING_CORE);
}

void startRebootCountdown(void *arg = NULL)
{
  menu.pages()[countdownPageId]->setTitle("REBOOT");
  if (menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (countdownTaskHandle)
    vTaskDelete(countdownTaskHandle);
  xTaskCreatePinnedToCore(countdownTask, "Reboot Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_REBOOT, 2, &countdownTaskHandle, ARDUINO_RUNNING_CORE);
}

void startFactoryResetCountdown(void *arg = NULL)
{
  menu.pages()[countdownPageId]->setTitle("FACTORY RESET REBOOT");
  if (menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (countdownTaskHandle)
    vTaskDelete(countdownTaskHandle);
  xTaskCreatePinnedToCore(countdownTask, "Reset Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_FACTORY_RESET, 2, &countdownTaskHandle, ARDUINO_RUNNING_CORE);
}

void initiateFactoryReset(void *arg = NULL)
{
  resetPreferences();
  savePreferences();
  startFactoryResetCountdown();
}

void uiMenuInit(void)
{
  MinuPage countdownPage(NULL, menu.numPages());
  countdownPage.setOpenedCallback(pageOpenedCallback);
  countdownPage.setClosedCallback(pageClosedCallback);
  countdownPage.setRenderedCallback(pageRenderedCallback);
  countdownPageId = menu.addPage(countdownPage);

  MinuPage homePage("1SIMPLECONNECT", menu.numPages());
  homepageWifiItem = homePage.addItem(goToWiFiPage, "Wi-Fi", " ", updateWiFiItem);
  homePage.addItem(goToPingTargetsPage, "Network Diags", NULL);
  homePage.addItem(goToRouterPage, "Router", NULL);
  homePage.addItem(goToTimePage, "Time", NULL);
  homePage.addItem(goToFobInfoPage, "Fob Info", NULL);
  homePage.addItem(startShutdownCountdown, "ShutDown", NULL);
  homePage.addItem(startRebootCountdown, "Reboot", NULL);
  homePage.addItem(goToFactoryResetPage, "Factory Reset", NULL);
  homePage.setOpenedCallback(pageOpenedCallback);
  homePage.setClosedCallback(pageClosedCallback);
  homePage.setRenderedCallback(pageRenderedCallback);
  homePageId = menu.addPage(homePage);

  MinuPage wifiPage("WI-FI", menu.numPages());
  wifiPage.addItem(startWiFiConnectCountdown, "Connect STA", NULL);
  wifiPage.addItem(startWiFiAP, "Start AP", NULL);
  wifiPage.addItem(startWiFiScan, "Scan", NULL);
  wifiStatusItem = wifiPage.addItem(goToHomePage, "<--", NULL);
  wifiPage.setOpenedCallback(pageOpenedCallback);
  wifiPage.setClosedCallback(pageClosedCallback);
  wifiPage.setRenderedCallback(lcdPrintWiFiStatus);
  wifiPageId = menu.addPage(wifiPage);

  MinuPage wifiPromptPage("WI-FI DISCON", menu.numPages());
  wifiPromptPage.addItem(startWiFiConnectCountdown, "Retry STA", NULL);
  wifiPromptPage.addItem(startWiFiAP, "Start AP", NULL);
  wifiPromptPage.addItem(cancelWiFiSetup, "Cancel", NULL);
  wifiPromptPage.setOpenedCallback(pageOpenedCallback);
  wifiPromptPage.setClosedCallback(pageClosedCallback);
  wifiPromptPage.setRenderedCallback(pageRenderedCallback);
  wifiPromptPageId = menu.addPage(wifiPromptPage);

  MinuPage saveSSIDAsPage("SAVE SSID AS", menu.numPages());
  saveSSIDAsPage.addItem(saveSSID, "Primary", NULL);
  saveSSIDAsPage.addItem(saveSSID, "Secondary", NULL);
  saveSSIDAsPage.addItem(goToWiFiPage, "<--", NULL);
  saveSSIDAsPage.setOpenedCallback(pageOpenedCallback);
  saveSSIDAsPage.setClosedCallback(pageClosedCallback);
  saveSSIDAsPage.setRenderedCallback(pageRenderedCallback);
  saveSSIDAsPageId = menu.addPage(saveSSIDAsPage);

  MinuPage scanResultPage("SCAN RESULT", menu.numPages());
  scanResultPage.setOpenedCallback(pageOpenedCallback);
  scanResultPage.setClosedCallback(deleteAllPageItems);
  scanResultPage.setRenderedCallback(pageRenderedCallback);
  scanResultPageId = menu.addPage(scanResultPage);

  MinuPage pingTargetsPage("NETWORK DIAG", menu.numPages());
  for (PingTarget target : pingTargets)
    pingTargetsPage.addItem(NULL, target.displayHostname.c_str(), " ");
  pingTargetsPage.addItem(goToHomePage, "<--", NULL);
  pingTargetsPage.setOpenedCallback(startDataUpdate);
  pingTargetsPage.setClosedCallback(stopDataUpdate);
  pingTargetsPage.setRenderedCallback(pageRenderedCallback);
  pingTargetsPageId = menu.addPage(pingTargetsPage);

  MinuPage routerPage("ROUTER", menu.numPages());
  routerPage.addItem(goToRouterInfoPage, "Info", NULL);
  routerPage.addItem(goToRouterLocationPage, "Location", NULL);
  routerPage.addItem(goToRouterWANListPage, "WAN Status", NULL);
  routerPage.addItem(goToHomePage, "<--", NULL);
  routerPage.setOpenedCallback(pageOpenedCallback);
  routerPage.setClosedCallback(pageClosedCallback);
  routerPage.setRenderedCallback(pageRenderedCallback);
  routerPageId = menu.addPage(routerPage);

  MinuPage routerInfoPage("ROUTER INFO", menu.numPages(), true);
  routerInfoPage.addItem(goToRouterPage, NULL, NULL);
  routerInfoPage.setOpenedCallback(pageOpenedCallback);
  routerInfoPage.setClosedCallback(pageClosedCallback);
  routerInfoPage.setRenderedCallback(lcdPrintRouterInfo);
  routerInfoPageId = menu.addPage(routerInfoPage);

  MinuPage routerLocationPage("ROUTER LOCATION", menu.numPages(), true);
  routerLocationPage.addItem(goToRouterPage, NULL, NULL);
  routerLocationPage.setOpenedCallback(pageOpenedCallback);
  routerLocationPage.setClosedCallback(pageClosedCallback);
  routerLocationPage.setRenderedCallback(lcdPrintRouterLocation);
  routerLocationPageId = menu.addPage(routerLocationPage);

  MinuPage routerWANListPage("WAN LIST", menu.numPages());
  routerWANListPage.setOpenedCallback(getWanList);
  routerWANListPage.setClosedCallback(wanListPageClosed);
  routerWANListPage.setRenderedCallback(pageRenderedCallback);
  routerWANListPageId = menu.addPage(routerWANListPage);

  MinuPage routerWANInfoPage("WAN STATUS", menu.numPages(), true);
  routerWANInfoPage.addItem(goToRouterWANListPage, NULL, NULL);
  routerWANInfoPage.setOpenedCallback(pageOpenedCallback);
  routerWANInfoPage.setClosedCallback(stopDataUpdate);
  routerWANInfoPage.setRenderedCallback(startDataUpdate);
  routerWANInfoPageId = menu.addPage(routerWANInfoPage);

  MinuPage routerWANSummaryPage("WAN STATUS", menu.numPages(), true);
  routerWANSummaryPage.addItem(goToRouterWANListPage, NULL, NULL);
  routerWANSummaryPage.setOpenedCallback(pageOpenedCallback);
  routerWANSummaryPage.setClosedCallback(stopDataUpdate);
  routerWANSummaryPage.setRenderedCallback(startDataUpdate);
  routerWANSummaryPageId = menu.addPage(routerWANSummaryPage);

  MinuPage simListPage("SIM LIST", menu.numPages());
  simListPage.setOpenedCallback(showSimList);
  simListPage.setClosedCallback(simListPageClosed);
  simListPage.setRenderedCallback(pageRenderedCallback);
  simListPageId = menu.addPage(simListPage);

  MinuPage simInfoPage("SIM STATUS", menu.numPages(), true);
  simInfoPage.addItem(goToSimListPage, NULL, NULL);
  simInfoPage.setOpenedCallback(pageOpenedCallback);
  simInfoPage.setClosedCallback(pageClosedCallback);
  simInfoPage.setRenderedCallback(showSimInfo);
  simInfoPageId = menu.addPage(simInfoPage);

  MinuPage timePage("TIME", menu.numPages(), true);
  timePage.addItem(goToHomePage, NULL, NULL);
  timePage.setOpenedCallback(pageOpenedCallback);
  timePage.setClosedCallback(stopDataUpdate);
  timePage.setRenderedCallback(startDataUpdate);
  timePageId = menu.addPage(timePage);

  MinuPage fobInfoPage("FOB INFO", menu.numPages(), true);
  fobInfoPage.addItem(goToHomePage, NULL, NULL);
  fobInfoPage.setOpenedCallback(pageOpenedCallback);
  fobInfoPage.setClosedCallback(stopDataUpdate);
  fobInfoPage.setRenderedCallback(startDataUpdate);
  fobInfoPageId = menu.addPage(fobInfoPage);

  MinuPage factoryResetPage("FACTORY RESET", menu.numPages());
  factoryResetPage.addItem(initiateFactoryReset, "Factory Reset", NULL);
  factoryResetPage.addItem(goToHomePage, "Cancel", NULL);
  factoryResetPage.setOpenedCallback(pageOpenedCallback);
  factoryResetPage.setClosedCallback(pageClosedCallback);
  factoryResetPage.setRenderedCallback(pageRenderedCallback);
  factoryResetPageId = menu.addPage(factoryResetPage);

  String cookie;
   if ( homePageId < 0 ||
        wifiPageId < 0 ||
        wifiPromptPageId < 0 ||
        scanResultPageId < 0 ||
        saveSSIDAsPageId < 0 ||
        routerPageId < 0 ||
        routerInfoPageId < 0 ||
        routerLocationPageId < 0 ||
        routerWANListPageId < 0 ||
        routerWANInfoPageId < 0 ||
        timePageId < 0 ||
        countdownPageId < 0 ||
        fobInfoPageId < 0)
      goto err;
      
   lastVisitedPageId = wifiPageId;

  xTaskCreatePinnedToCore(screenWatchTask, "Screen Watch Task", 4096, NULL, 1, &screenWatchTaskHandle, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(screenUpdateTask, "Screen Update Task", 4096, NULL, 1, &screenUpdateTaskHandle, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(buttonWatchTask, "Button Task", 4096, NULL, 1, &buttonWatchTaskHandle, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(wifiWatchTask, "WiFi Watch Task", 4096, NULL, 1, &wifiWatchTaskHandle, ARDUINO_RUNNING_CORE);
  startWiFiConnectCountdown();

  while(booting)
    delay(10);

  if (WiFi.status() == WL_CONNECTED)
  {
    router.setIP(routerIP);
    router.setPort(routerPort);
    cookie = router.begin(routerUsername, routerPassword, routerClientName, routerClientScope, true);
    if (!cookie.length())
      Serial.println("!!Router init Fail!!");
    else
    {
      Serial.println("Router connected!");
      goToRouterWANListPage();
    }
  }
  return;

err:
  while (1)
  {
    delay(1000);
    Serial.println("Failed to init menu!");
  }
}

/// @brief Watch for any detected button press and manipulate UI accordingly
void screenWatchTask(void *arg)
{
  Serial.println("Started screenWatchTask");
  ssize_t lastSelectedPage = menu.currentPageId();
  ssize_t lastHighlightedItem = menu.currentPage()->highlightedIndex();

  bool updateScreen = false;
  long lastStackCheckTime = 0;

  for (;;)
  {
    // If a short press of A is detected, highlight the next item
    if (shortPressA)
    {
#ifdef UI_BEEP
      M5.Speaker.tone(8000, 30);
#endif
      if (menu.currentPageId() != countdownPageId)
      {  
        menu.currentPage()->highlightNextItem();
        if(lastHighlightedItem != menu.currentPage()->highlightedIndex())
          updateScreen = true;
      }
      shortPressA = false;
    }

    // If a long press of A or short press of B is detected, select the currently highlighted item
    if (longPressA || shortPressB)
    {
#ifdef UI_BEEP
      M5.Speaker.tone(5000, 30);
#endif
      if (menu.currentPageId() != countdownPageId && menu.currentPage()->highlightedIndex() >= 0)
      {
        MinuPageItem highlightedItem = menu.currentPage()->highlightedItem();
        if (highlightedItem.link())
          highlightedItem.link()(&highlightedItem);
#ifdef UI_DEBUG_LOG
        Serial.printf("Executed selected item link = %p\n", highlightedItem.link());
#endif
      }

      longPressA = false;
      shortPressB = false;
    }

    // If a long press of B is detected, cancel any ongoing shutdown
    if (longPressB)
    {
      if (countdownTaskHandle)
        xTaskNotify(countdownTaskHandle, 1, eSetValueWithOverwrite);

      longPressB = false;
    }

    // If the current page has changed, log the change
    if (lastSelectedPage != menu.currentPageId())
    {
#ifdef UI_DEBUG_LOG
      Serial.printf("Changed from page %d to page %d\n", lastSelectedPage, menu.currentPageId());
#endif
      lastSelectedPage = menu.currentPageId();
      lastHighlightedItem = menu.currentPage()->highlightedIndex();
    }

    // If the current page has changed, log the change
    if (lastHighlightedItem != menu.currentPage()->highlightedIndex())
    {
#ifdef UI_DEBUG_LOG
      Serial.printf("Highlighted item changed: Old = %d | New = %d\n", lastHighlightedItem, menu.currentPage()->highlightedIndex());
#endif
      lastHighlightedItem = menu.currentPage()->highlightedIndex();
    }

    if (updateScreen || !menu.rendered())
    {
      updateScreen = false;
      if (countdownTaskHandle && menu.currentPageId() != countdownPageId)
      {
        vTaskDelete(countdownTaskHandle);
        countdownTaskHandle = NULL;
      }

      if (screenUpdateTaskHandle)
        xTaskNotify(screenUpdateTaskHandle, 1, eSetValueWithOverwrite);
      while(!menu.rendered())
        delay(10);
    }

#ifdef UI_DEBUG_LOG
    if (lastStackCheckTime + 10000 < millis())
    {
      Serial.printf("Screen Watch Task available stack:  %d * %d bytes\n", uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
      lastStackCheckTime = millis();
    }
#endif
  }
}

void buttonWatchTask(void *arg)
{
  Serial.println("buttonWatchTask started");

  long lastStackCheckTime = 0;

  for (;;)
  {
    M5.update();
    currentTime = millis();
    if (M5.BtnA.wasPressed())
      lastButtonADownTime = currentTime;

    if (M5.BtnA.wasReleased())
    {
      if (lastButtonADownTime + LONG_PRESS_THRESHOLD_MS < currentTime)
      {
        longPressA = true;
#ifdef UI_DEBUG_LOG
        Serial.print("Long");
#endif
      }
      else
      {
        shortPressA = true;
#ifdef UI_DEBUG_LOG
        Serial.print("Short");
#endif
      }
#ifdef UI_DEBUG_LOG
      Serial.printf(" press A: %lums\n", currentTime - lastButtonADownTime);
#endif
    }

    if (M5.BtnB.wasPressed())
      lastButtonBDownTime = currentTime;

    if (M5.BtnB.wasReleased())
    {
      if (lastButtonBDownTime + LONG_PRESS_THRESHOLD_MS < currentTime)
      {
        longPressB = true;
#ifdef UI_DEBUG_LOG
        Serial.print("Long");
#endif
      }
      else
      {
        shortPressB = true;
#ifdef UI_DEBUG_LOG
        Serial.print("Short");
#endif
      }
#ifdef UI_DEBUG_LOG
      Serial.printf(" press B: %ldms\n", currentTime - lastButtonBDownTime);
#endif
    }
#ifdef UI_DEBUG_LOG
    if (lastStackCheckTime + 10000 < currentTime)
    {
      Serial.printf("Button Watch Task available stack:  %d * %d bytes\n", uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
      lastStackCheckTime = currentTime;
    }
#endif
  }
}

void countdownTask(void *arg)
{
  int8_t sec = 0, min = 0;
  const int countdownType = (int)arg;
  uint32_t stopCountdown;

  if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
  {
    min = wifiTimeoutMs ? (wifiTimeoutMs / 1000) / 60 : WIFI_COUNTDOWN_SECONDS / 60;
    sec = wifiTimeoutMs ? (wifiTimeoutMs / 1000) % 60 : WIFI_COUNTDOWN_SECONDS % 60;
  }
  else if (countdownType == UI_COUNTDOWN_TYPE_SHUTDOWN)
  {
    min = SHUTDOWN_COUNTDOWN_SECONDS / 60;
    sec = SHUTDOWN_COUNTDOWN_SECONDS % 60;
  }
  else if (countdownType == UI_COUNTDOWN_TYPE_REBOOT)
  {
    min = REBOOT_COUNTDOWN_SECONDS / 60;
    sec = REBOOT_COUNTDOWN_SECONDS % 60;
  }
  else if( countdownType == UI_COUNTDOWN_TYPE_FACTORY_RESET)
  {
    min = REBOOT_COUNTDOWN_SECONDS / 60;
    sec = REBOOT_COUNTDOWN_SECONDS % 60;
  }
  else
  {
#ifdef UI_DEBUG_LOG
    Serial.println("Unspecified countdown type!");
#endif
    vTaskDelete(NULL);
  }

  while (!menu.rendered())
    delay(10);

  if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
    M5.Lcd.printf("\nSSID: %s\n\n", WiFi.SSID().c_str());

  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();


  M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);

  while (min >= 0 || sec >= 0)
  {
    if (countdownType == UI_COUNTDOWN_TYPE_WIFI && WiFi.status() == WL_CONNECTED)
      break;

    if (--sec < 0)
    {
      if (min > 0)
        sec = 59;
      else
      {
        sec = 0;
        break;
      }
      if (--min < 0)
        min = 0;
    }

    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextSize(7);
    M5.Lcd.printf(" %d:%02d\n\n", min, sec);
#ifdef UI_BEEP
    M5.Speaker.tone(9000, 50);
#endif
    stopCountdown = ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));
    if (stopCountdown == 1)
      break;
  }

  M5.Lcd.setTextSize(TEXT_SIZE_DEFAULT);

  if (sec <= 0 && min <= 0)
  {
    if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
    {
      if (booting && primarySsidValid)
      {
        primarySsidValid = false;
        WiFi.mode(WIFI_MODE_NULL);
        delay(100);
      }
      else
        wifiTimeout = true;

      goToWiFiPromptPage();
    }
    else if (countdownType == UI_COUNTDOWN_TYPE_SHUTDOWN)
      M5.Power.powerOff();
    else
      ESP.restart();
  }
  else
  {
    if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
    {
      if (stopCountdown)
      {
        wifiTimeout = true;
        goToWiFiPromptPage();
      }
      else if (booting)
      {
        goToPingTargetsPage();
        xTaskNotify(wifiWatchTaskHandle, 1, eSetValueWithOverwrite);
      }
      else
        menu.goToPage(lastVisitedPageId);
    }
    else if (countdownType == UI_COUNTDOWN_TYPE_SHUTDOWN || countdownType == UI_COUNTDOWN_TYPE_REBOOT)
      goToHomePage();
  }

#ifdef UI_BEEP
  M5.Speaker.tone(9000, 300);
#endif

  countdownTaskHandle = NULL;
  vTaskDelete(NULL);
}

void dataUpdateTask(void *arg)
{
  while (!menu.rendered())
    delay(10);
  
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  int updateType = (int)arg;
#ifdef UI_DEBUG_LOG
  Serial.printf("Started data update task: type %d\n", updateType);
#endif
  M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);

  while (1)
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    Serial.printf("Update type %d started\n", updateType);

    if (updateType == UI_UPDATE_TYPE_TIME)
      lcdPrintTime();
    else if (updateType == UI_UPDATE_TYPE_ROUTER_INFO)
      lcdPrintRouterWANInfo();
    else if (updateType == UI_UPDATE_TYPE_FOB_INFO)
      lcdPrintFobInfo();
    else if (updateType == UI_UPDATE_TYPE_PING)
      updatePingTargetsStatus();
    else if (updateType == UI_UPDATE_TYPE_WAN)
      printRouterWanStatus();

    Serial.printf("Update type %d done\n", updateType);
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (updateType == UI_UPDATE_TYPE_PING && booting && pingTargets[0].pingOK)
    {
      booting = false;
      break;
    }
  }

  dataUpdateTaskHandle = NULL;
  vTaskDelete(NULL);
}

/// @brief Performs the actual rendering of the screen upon receiving a task notification
void screenUpdateTask(void *arg)
{
  uint32_t renderMenu;
  while (1)
  {
    renderMenu = ulTaskNotifyTake(false, portMAX_DELAY);
    if (renderMenu == 1)
    {
      M5.Lcd.clear();
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(TEXT_SIZE_DEFAULT);
      menu.render(MINU_ITEM_MAX_COUNT);
    }
  }
  vTaskDelete(NULL);
}

/// @brief Watches for Wi-Fi disconnection in STA mode and initiates a reconnect countdown
void wifiWatchTask(void* arg)
{
  ulTaskNotifyTake(false, portMAX_DELAY);
  for(;;)
  {
    if (WiFi.getMode() == WIFI_MODE_STA && WiFi.status() != WL_CONNECTED && !countdownTaskHandle && !wifiTimeout)
    {
      lastVisitedPageId = menu.currentPageId();
      startWiFiConnectCountdown(NULL);
    }
  }
}