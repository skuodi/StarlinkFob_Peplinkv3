#include "M5StickCPlus2.h"
#include <WiFi.h>
#include <ArduinoUniqueID.h>

#include "Minu/minu.hpp"
#include "ui.h"
#include "utils.h"
#include "config.h"
#include "logo.h"

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

/// @brief Conditions that trigger the countdown page
typedef enum
{
  UI_COUNTDOWN_TYPE_WIFI = 1,     // Wi-Fi STA connecting countdown
  UI_COUNTDOWN_TYPE_ROUTER,
  UI_COUNTDOWN_TYPE_SHUTDOWN,
  UI_COUNTDOWN_TYPE_REBOOT,
  UI_COUNTDOWN_TYPE_FACTORY_RESET,
} UiCountdownType;

/// @brief Defines the data to be fetched periodically
typedef enum
{
  UI_UPDATE_TYPE_TIME = 1,
  UI_UPDATE_TYPE_SENSORS,
  UI_UPDATE_TYPE_WAN_INFO,
  UI_UPDATE_TYPE_FOB_INFO,
  UI_UPDATE_TYPE_PING,
  UI_UPDATE_TYPE_WAN_SUMMARY
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

size_t homePageId;
size_t wifiPageId;
size_t wifiPromptPageId;
size_t scanResultPageId;
size_t saveSSIDAsPageId;
size_t pingTargetsPageId;
size_t routerPageId;
size_t routerInfoPageId;
size_t routerLocationPageId;
size_t routerUnavailablePageId;
size_t routerWANListPageId;
size_t routerWANInfoPageId;
size_t routerWANSummaryPageId;
size_t timePageId;
size_t sensorsPageId;
size_t countdownPageId;
size_t fobInfoPageId;
size_t factoryResetPageId;
size_t simListPageId;
size_t simInfoPageId;
size_t lastVisitedPageId;

static size_t wifiStatusItem;
static ssize_t homepageWifiItem;

static String tempSSID;
static String lastSelectedWAN;
static size_t lastSelectedSim;

void buttonWatchTask(void *arg);
void screenWatchTask(void *arg);
void screenUpdateTask(void *arg);
void countdownTask(void *arg);
void dataUpdateTask(void *arg);
void wifiWatchTask(void* arg);
void routerConnectTask(void* arg);

void goToFobInfoPage(void *arg = NULL)
{
  fob.menu.goToPage(fobInfoPageId);
}

void goToHomePage(void *arg = NULL)
{
  fob.booting = false;
  fob.menu.goToPage(homePageId);
}

void goToWiFiPage(void *arg = NULL)
{
  fob.wifi.usePrimarySsid = true;
  fob.menu.goToPage(wifiPageId);
}

void goToTimePage(void *arg = NULL)
{
  fob.menu.goToPage(timePageId);
}

void goToSensorsPage(void *arg = NULL)
{
  fob.menu.goToPage(sensorsPageId);
}

void goToWiFiPromptPage(void *arg = NULL)
{
  fob.wifi.usePrimarySsid = true;
  fob.menu.goToPage(wifiPromptPageId);
}

void goToScanResultPage(void *arg = NULL)
{
  fob.menu.goToPage(scanResultPageId);
}

void goToCountdownPage(void *arg = NULL)
{
  fob.menu.goToPage(countdownPageId);
}

void goToRouterPage(void *arg = NULL)
{
  fob.menu.goToPage(routerPageId);
}

void goToRouterInfoPage(void *arg = NULL)
{
  fob.menu.goToPage(routerInfoPageId);
}

void goToRouterLocationPage(void *arg = NULL)
{
  fob.menu.goToPage(routerLocationPageId);
}

void goToRouterUnavailablePage(void *arg = NULL)
{
  fob.menu.goToPage(routerUnavailablePageId);
}

void goToRouterWANListPage(void *arg = NULL)
{
  fob.menu.goToPage(routerWANListPageId);
}

void goToRouterWANInfoPage(void *arg = NULL)
{
  fob.menu.goToPage(routerWANInfoPageId);
}

void goToWANSummaryPage(void* arg = NULL)
{
  fob.menu.goToPage(routerWANSummaryPageId);
}

void goToPingTargetsPage(void *arg = NULL)
{
  fob.menu.goToPage(pingTargetsPageId);
}

void goToFactoryResetPage(void* arg = NULL)
{
  fob.menu.goToPage(factoryResetPageId);
}

void goToSimListPage(void* arg = NULL)
{
  fob.menu.goToPage(simListPageId);
}

void goToSimInfoPage(void* arg = NULL)
{
  fob.menu.goToPage(simInfoPageId);
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
    if (fob.menu.currentPageId() == wifiPageId && (*(MinuPage *)page).highlightedIndex() != wifiStatusItem)
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

  M5.Lcd.drawBitmap(0, cursorY, 40, 40, loading, 0);

  if (!fob.routers.router.checkAvailable() || !fob.routers.router.getInfo())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.printf("Name  :%s\n", fob.routers.router.info().name.c_str());
  M5.Lcd.printf("Uptime:%ld\n", fob.routers.router.info().uptime);
  M5.Lcd.printf("Serial:%s\n", fob.routers.router.info().serial.c_str());
  M5.Lcd.printf("Fw Ver:%s\n", fob.routers.router.info().fwVersion.c_str());
  M5.Lcd.printf("P Code:%s\n", fob.routers.router.info().productCode.c_str());
  M5.Lcd.printf("hw Rev:%s\n", fob.routers.router.info().hardwareRev.c_str());
}

/// @brief Fetch and print the router location information
void lcdPrintRouterLocation(void *arg = NULL)
{
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  M5.Lcd.println("Fetching...");

  if (!fob.routers.router.checkAvailable() || !fob.routers.router.getLocation())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  
  M5.Lcd.setCursor(cursorX, cursorY);
  M5.Lcd.printf("Longitude:%s\n", fob.routers.router.location().longitude.c_str());
  M5.Lcd.printf("Latitude :%s\n", fob.routers.router.location().latitude.c_str());
  M5.Lcd.printf("Altitude :%s\n", fob.routers.router.location().altitude.c_str());
}

void lcdPrintRouterUnavailable(void *arg = NULL)
{
  fob.booting = true;
  M5.Lcd.println(" Router Unavailable!\n Unable to continue.\n Please reboot"); 
}

/// @brief Fetch and print the selected WAN information
void lcdPrintRouterWANInfo(void *arg = NULL)
{
  std::vector<PeplinkAPI_WAN *> wanList = fob.routers.router.wanStatus();
  if (!wanList.size())
  {
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("No WAN found!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }

  for (PeplinkAPI_WAN *wan : wanList)
  {
    if (wan->name == lastSelectedWAN)
    {
      if(!fob.routers.router.getWanStatus(wan->id))
      {
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println("Unavailable!");
        M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
        return;
      }
      else
      {
        M5.Lcd.printf("Name:%s      \n", wan->name.c_str());
        M5.Lcd.print("Type:");
        switch (wan->type)
        {
        case PEPLINKAPI_WAN_TYPE_ETHERNET:
          M5.Lcd.print("ETHERNET\n");
          break;
        case PEPLINKAPI_WAN_TYPE_CELLULAR:
          M5.Lcd.print("CELLULAR\n");
          M5.Lcd.printf("Carr:%s ",((PeplinkAPI_WAN_Cellular *)wan)->carrier.c_str());
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
          M5.Lcd.printf("%s", ((PeplinkAPI_WAN_Cellular *)wan)->networkType.c_str());
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
          switch (((PeplinkAPI_WAN_Cellular *)wan)->signalLevel)
          {
            case 0: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars0, 0); break;
            case 1: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars1, 0); break;
            case 2: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars2, 0); break;
            case 3: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars3, 0); break;
            default: M5.Lcd.drawBitmap(M5.Lcd.getCursorX()+ 10, M5.Lcd.getCursorY() - 5, 20, 21, bars4, 0); break;
          }
          M5.Lcd.println(" ");
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
      break;
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
  M5.Lcd.printf("Time:%02d:%02d:%02dH\nDate:%02d/%02d/%04d\n",
                dt.time.hours, dt.time.minutes, dt.time.seconds,
                dt.date.date, dt.date.month, dt.date.year);

  if(fob.timestamps.lastShutdownTime.length())  
    M5.Lcd.printf("Last PWR:%s\n", fob.timestamps.lastShutdownTime.c_str());
  else
    M5.Lcd.println("Last PWR:Unknown");

  if (fob.timestamps.lastShutdownRuntime)
    M5.Lcd.printf("Last Run:%04d:%02d", (fob.timestamps.lastShutdownRuntime/1000)/3600, ((fob.timestamps.lastShutdownRuntime/1000) % 3600)/60);
  else
    M5.Lcd.println("Last Run:Unknown");
}

void lcdPrintSensors(void* arg = NULL)
{
  if(fob.sensors.shtAvailable && fob.sensors.sht.update())
  {
    if(fob.sensors.sht.fTemp > TEMPERATURE_ALERT_THRESH_F)
    {
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextColor(WHITE, RED);
      M5.Lcd.setTextSize(TEXT_SIZE_DEFAULT);
      M5.Lcd.println("__OVER TEMPERATURE__\n");
      M5.Lcd.printf("  Threshold: %dF\n\n", TEMPERATURE_ALERT_THRESH_F);
      M5.Lcd.setTextSize(5);
      M5.Lcd.printf(" %.2fF\n\n", fob.sensors.sht.fTemp);
      M5.Lcd.setTextSize(TEXT_SIZE_DEFAULT);
      M5.Lcd.setTextColor(WHITE, BLACK);
      
      AlertTimestamp_t aTime;
      auto dt = M5.Rtc.getDateTime();
      snprintf(aTime.lastTempAlertTime, sizeof(aTime.lastTempAlertTime), "%02d/%02d/%04d %02d:%02dH",
                dt.date.date, dt.date.month, dt.date.year,
                dt.time.hours, dt.time.minutes);
      aTime.lastTempAlertTemp = fob.sensors.sht.fTemp;
      aTime.lastTempAlertThresh = TEMPERATURE_ALERT_THRESH_F;

      Serial.printf("Over-temperature alert\n\tThreshold: %dF\n\tTemperature: %fF\n\tTimestamp: %s\n",
                    TEMPERATURE_ALERT_THRESH_F, aTime.lastTempAlertTemp, aTime.lastTempAlertTime);

      Preferences alertPrefs;
      if (alertPrefs.begin(TEMPERATURE_ALERT_NAMESPACE, false))
      {
          alertPrefs.clear();
          if(alertPrefs.putBytes(TEMPERATURE_ALERT_NAMESPACE, &aTime, sizeof(aTime)))
            Serial.println("Saved alert timestamp to NVS!");
      }
      alertPrefs.end();
      delay(100);
      return;
    }
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("______SENSORS_______\n");
    M5.Lcd.printf("%%RH :%.2f%% @ %.2fF\n", fob.sensors.sht.humidity, fob.sensors.sht.fTemp);
  }
  else
    M5.Lcd.println("\nSHT sensor unavailable!");

  if(fob.sensors.qmpAvailable && fob.sensors.qmp.update())
  {
    M5.Lcd.printf("Temp:%.4f C\n", fob.sensors.qmp.cTemp);
    M5.Lcd.printf("Pres:%.4f Pa\n", fob.sensors.qmp.pressure);
    M5.Lcd.printf("Alt :%.4f m\n", fob.sensors.qmp.altitude);
  }
  else
    M5.Lcd.println("QMP sensor unavailable!\n");
}

/// @brief Get the status of WAN connections
void printRouterWanStatus(void* arg = NULL)
{
  Serial.println("Getting WAN list - ");
  M5.Lcd.setCursor(0, 0, 1);
  M5.Lcd.println("_____WAN SUMMARY____\n");

  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  M5.Lcd.drawBitmap(200, cursorY, 40, 40, loading, 0);

  if (!fob.routers.router.checkAvailable() || !fob.routers.router.getWanStatus())
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }
  else
  {
    M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);
    M5.Lcd.setCursor(cursorX, cursorY);
    std::vector<PeplinkAPI_WAN *> wanList = fob.routers.router.wanStatus();
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
        if(wan->managementOnly)
        {
          M5.Lcd.setTextColor(TFT_WHITE, TFT_ORANGE);
          M5.Lcd.print("+");
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
          M5.Lcd.println(" " + wan->name);
        }
        else
        {
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
          M5.Lcd.println("  " + wan->name);
        }
        M5.Lcd.println(wan->status);
        // M5.Lcd.print(wan->name +" " + wan->status + "\n");
      }
      else
      {
        // M5.Lcd.print(wan->name +" " + wan->statusLED +" " + wan->status + "\n");
        M5.Lcd.setTextColor(TFT_BLACK, TFT_GREY);
        M5.Lcd.print(" ");
        if(!wan->managementOnly)
        {
          M5.Lcd.setTextColor(TFT_WHITE, TFT_ORANGE);
          M5.Lcd.print("+");
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
          M5.Lcd.println(" " + wan->name);
        }
        else
        {
          M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
          M5.Lcd.println("  " + wan->name);
        }
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
            M5.Lcd.print(((PeplinkAPI_WAN_Cellular *)wan)->carrier + " " );
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
            M5.Lcd.print(((PeplinkAPI_WAN_Cellular *)wan)->networkType);
            M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
            switch (((PeplinkAPI_WAN_Cellular *)wan)->signalLevel)
            {
            case 0: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars0, 0); break;
            case 1: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars1, 0); break;
            case 2: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars2, 0); break;
            case 3: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars3, 0); break;
            default: M5.Lcd.drawBitmap(M5.Lcd.getCursorX() + 10, M5.Lcd.getCursorY() - 5, 20, 21, bars4, 0); break;
            }
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
  if (fob.tasks.dataUpdate)
  {
    vTaskDelete(fob.tasks.dataUpdate);
    fob.tasks.dataUpdate = NULL;
  }
}

/// @brief Check whether ping targets can be reached
void updatePingTargetsStatus(void *arg = NULL)
{
  size_t targetCount = fob.pingTargets.size();
  Serial.printf("Pinging %d targets...\n", targetCount);

  for (size_t i = 0; i < targetCount; ++i)
  {
    if (fob.pingTargets[i].useIP)
      fob.pingTargets[i].pingOK = Ping.ping(fob.pingTargets[i].pingIP, 5);
    else
      fob.pingTargets[i].pingOK = Ping.ping(fob.pingTargets[i].fqn.c_str(), 5);
    fob.pingTargets[i].pinged = true;

    if (fob.pingTargets[i].pingOK)
      fob.menu.pages()[pingTargetsPageId]->items()[i].setAuxTextBackground(GREEN);
    else
      fob.menu.pages()[pingTargetsPageId]->items()[i].setAuxTextBackground(RED);
    Serial.printf("Target %d(%s) -> ping %s\n", i, fob.pingTargets[i].pingIP.toString().c_str(), (fob.pingTargets[i].pingOK) ? "OK" : "FAIL");
    if (fob.tasks.screenUpdate)
      xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
  }
}

/// @brief Starts the task that periodically performs HTTP requests
void startDataUpdate(void *arg = NULL)
{
  UiUpdateType ut;
  if (fob.menu.currentPageId() == timePageId)
    ut = UI_UPDATE_TYPE_TIME;
  else if (fob.menu.currentPageId() == sensorsPageId)
  {
    ut = UI_UPDATE_TYPE_SENSORS;
    
    fob.sensors.shtAvailable = fob.sensors.sht.begin(&Wire, SHT3X_I2C_ADDR, 0, 26, 400000U);
    fob.sensors.qmpAvailable = fob.sensors.qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 0, 26, 400000U);
  }
  else if (fob.menu.currentPageId() == routerWANInfoPageId)
    ut = UI_UPDATE_TYPE_WAN_INFO;
  else if (fob.menu.currentPageId() == fobInfoPageId)
    ut = UI_UPDATE_TYPE_FOB_INFO;
  else if (fob.menu.currentPageId() == pingTargetsPageId)
  {
    ut = UI_UPDATE_TYPE_PING;
    for (auto item : fob.menu.currentPage()->items())
      item.setAuxTextBackground(MINU_BACKGROUND_COLOUR_DEFAULT);
    fob.menu.currentPage()->highlightItem(0);
    if (fob.tasks.screenUpdate)
      xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
  }
  else if(fob.menu.currentPageId() == routerWANSummaryPageId)
    ut = UI_UPDATE_TYPE_WAN_SUMMARY;
  else
    return;

  // If the data update task is already running, delete and create it afresh
  stopDataUpdate();
  xTaskCreatePinnedToCore(dataUpdateTask, "Data Update", 4096, (void *)ut, 2, &fob.tasks.dataUpdate, ARDUINO_RUNNING_CORE);
}

void startWiFiConnectCountdown(void *arg = NULL)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    goToHomePage();
    return;
  }
  fob.menu.pages()[countdownPageId]->setTitle("WAITING FOR WIFI");

  fob.wifi.timedOut = false;
  if (fob.menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (fob.tasks.countdown)
    vTaskDelete(fob.tasks.countdown);
  xTaskCreatePinnedToCore(countdownTask, "WiFi Countdown", 4096, (void *)UI_COUNTDOWN_TYPE_WIFI, 2, &fob.tasks.countdown, ARDUINO_RUNNING_CORE);

  if (WiFi.getMode() != WIFI_MODE_STA)
  {
    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    WiFi.mode(WIFI_MODE_STA);

    if (fob.wifi.usePrimarySsid)
    {
      Serial.printf("Connecting to Wi-Fi: SSID - '%s', Pass - '%s'\n", fob.wifi.ssidStaPrimary.c_str(), fob.wifi.passwordStaPrimary.c_str());
      WiFi.begin(fob.wifi.ssidStaPrimary.c_str(), fob.wifi.passwordStaPrimary.c_str());
    }
    else
    {
      Serial.printf("Connecting to Wi-Fi: SSID - '%s', Pass - '%s'\n", fob.wifi.ssidStaSecondary.c_str(), fob.wifi.passwordStaSecondary.c_str());
      WiFi.begin(fob.wifi.ssidStaSecondary.c_str(), fob.wifi.passwordStaSecondary.c_str());
    }
  }
}

void startRouterConnectCountdown(void *arg = NULL)
{
  fob.menu.pages()[countdownPageId]->setTitle("CONNECTING ROUTER");

  if (fob.menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  vTaskDelay(2000);

  if (fob.tasks.countdown)
    vTaskDelete(fob.tasks.countdown);
  xTaskCreatePinnedToCore(countdownTask, "Router Countdown", 4096, (void *)UI_COUNTDOWN_TYPE_ROUTER, 2, &fob.tasks.countdown, ARDUINO_RUNNING_CORE);

}

/// @brief Start the Wi-Fi access point
void startWiFiAP(void *arg)
{
  if (WiFi.getMode() != WIFI_MODE_AP)
  {
    WiFi.mode(WIFI_MODE_NULL);
    delay(100);
    WiFi.mode(WIFI_MODE_AP);
    Serial.printf("Starting softAP...\tSSID:'%s'\tPassword:'%s'\n", fob.wifi.ssidSoftAp.c_str(), fob.wifi.passwordSoftAp.c_str());
    WiFi.softAP(fob.wifi.ssidSoftAp.c_str(), fob.wifi.passwordSoftAp.c_str());
    fob.wifi.timedOut = true;
  }
  delay(1000);
  if(!fob.servers.started)
  {
    fob.servers.started = true;
    Serial.println("Starting HTTP Server");
    startHttpServer();
  }
  goToWiFiPage();
}

/// @brief Fetch and display list of available sim cards
void showSimList(void *arg)
{
  while (!fob.menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching SIM list");
  
  std::vector<PeplinkAPI_WAN *> wanList = fob.routers.router.wanStatus();
  if (!wanList.size())
  {
    fob.menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
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
      String simName = String();

      for (auto it = 0; it < simList.size(); ++it)
      {
        char simId = it + 'A';
        simName = "SIM " + String(simId);
        simName += (simList[it].detected && simList[it].active) ? String(" (Active)") : String();
        thisItem = fob.menu.pages()[simListPageId]->addItem(goToSimInfoPage, simName.c_str(), " ");
        
        if(!simList[it].detected)
            fob.menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(TFT_GREY);
        else if (simList[it].active)
            fob.menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(GREEN);
        else
          fob.menu.pages()[simListPageId]->items()[thisItem].setAuxTextBackground(RED);
      }    
    }

  }
  fob.menu.pages()[simListPageId]->addItem(goToRouterPage, "<--", NULL);
  fob.menu.pages()[simListPageId]->highlightItem(0);
  if (fob.tasks.screenUpdate)
    xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
}

/// @brief Show information about a particular SIM card
void showSimInfo(void* arg = NULL)
{
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching SIM INFO");
  
  std::vector<PeplinkAPI_WAN *> wanList = fob.routers.router.wanStatus();
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
      M5.Lcd.printf("Name  :SIM %c\n", lastSelectedSim + 'A');
      if(!simList[lastSelectedSim].detected)
      {
        M5.Lcd.setTextColor(RED, BLACK);
        M5.Lcd.println(" (No Sim Detected)");
        M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
        break;
      }
      M5.Lcd.printf("Active:%s\n", simList[lastSelectedSim].active ? "true" : "false");
      M5.Lcd.printf("ICCID :%s\n", simList[lastSelectedSim].iccid.c_str());  
      break;
    }
  }
}

/// @brief Create the list of available WANs
void getWanList(void *arg)
{
  while (!fob.menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();
  Serial.println("Fetching WAN list");

  if (!fob.routers.router.checkAvailable() || !fob.routers.router.getWanStatus())
  {
    Serial.println("Fetching WAN list failed!");
    fob.menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.setTextColor(RED, BLACK);
    M5.Lcd.println("Unavailable!");
    M5.Lcd.setTextColor(MINU_FOREGROUND_COLOUR_DEFAULT, MINU_BACKGROUND_COLOUR_DEFAULT);
    return;
  }

  std::vector<PeplinkAPI_WAN *> wanList = fob.routers.router.wanStatus();
  if (!wanList.size())
  {
    fob.menu.pages()[routerWANListPageId]->addItem(goToRouterPage, NULL, NULL);
    Serial.println("No WAN found!");
    M5.Lcd.setCursor(cursorX, cursorY);
    M5.Lcd.println("No WAN found!");
    return;
  }
  ssize_t thisItem;
  for (PeplinkAPI_WAN *wan : wanList)
  {
    thisItem = fob.menu.pages()[routerWANListPageId]->addItem(goToRouterWANInfoPage, wan->name.c_str(), " ");

    if (!strcmp(wan->statusLED.c_str(), "red"))
      fob.menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(RED);
    else if (!strcmp(wan->statusLED.c_str(), "yellow"))
      fob.menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(YELLOW);
    else if (!strcmp(wan->statusLED.c_str(), "green"))
      fob.menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(GREEN);
    else if (!strcmp(wan->statusLED.c_str(), "flash"))
      fob.menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(WHITE);
    else
      fob.menu.pages()[routerWANListPageId]->items()[thisItem].setAuxTextBackground(TFT_GREY);
  }
  fob.menu.pages()[routerWANListPageId]->addItem(goToWANSummaryPage, "WAN Summary", NULL);
  fob.menu.pages()[routerWANListPageId]->addItem(goToSimListPage, "SIM Cards", NULL);
  fob.menu.pages()[routerWANListPageId]->addItem(goToRouterPage, "<--", NULL);
  fob.menu.pages()[routerWANListPageId]->highlightItem(0);
  if (fob.tasks.screenUpdate)
    xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
}

/// @brief Stop waiting for Wi-Fi to connect and go to homepage
void cancelWiFiSetup(void *arg)
{
  fob.wifi.usePrimarySsid = true;
  WiFi.mode(WIFI_MODE_NULL);
  if(fob.booting || lastVisitedPageId == countdownPageId)
    fob.menu.goToPage(homePageId);
  else
    fob.menu.goToPage(lastVisitedPageId);
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
    fob.wifi.ssidStaPrimary = tempSSID;
    fob.wifi.usePrimarySsid = true;
  }
  else
    fob.wifi.ssidStaSecondary = tempSSID;
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

  fob.menu.goToPage(saveSSIDAsPageId);
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
  if (fob.tasks.screenUpdate)
    xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
  while (!fob.menu.rendered())
    delay(10);
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  fob.wifi.timedOut = true;
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
      fob.menu.pages()[scanResultPageId]->addItem(saveItemSSID, WiFi.SSID(i).c_str(), String(WiFi.RSSI(i)).c_str());

    WiFi.scanDelete();
  }
    
  delay(3000);
  fob.menu.pages()[scanResultPageId]->addItem(goToWiFiPage, "<--", NULL);
  if (fob.tasks.screenUpdate)
    xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
    
  while (!fob.menu.rendered())
    delay(10);
}

void startShutdownCountdown(void *arg = NULL)
{
  fob.menu.pages()[countdownPageId]->setTitle("SHUTDOWN");
  if (fob.menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (fob.tasks.countdown)
    vTaskDelete(fob.tasks.countdown);
  xTaskCreatePinnedToCore(countdownTask, "Shutdown Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_SHUTDOWN, 2, &fob.tasks.countdown, ARDUINO_RUNNING_CORE);
}

void startRebootCountdown(void *arg = NULL)
{
  fob.menu.pages()[countdownPageId]->setTitle("REBOOT");
  if (fob.menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (fob.tasks.countdown)
    vTaskDelete(fob.tasks.countdown);
  xTaskCreatePinnedToCore(countdownTask, "Reboot Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_REBOOT, 2, &fob.tasks.countdown, ARDUINO_RUNNING_CORE);
}

void startFactoryResetCountdown(void *arg = NULL)
{
  fob.menu.pages()[countdownPageId]->setTitle("FACTORY RESET REBOOT");
  if (fob.menu.currentPageId() != countdownPageId)
    goToCountdownPage();

  if (fob.tasks.countdown)
    vTaskDelete(fob.tasks.countdown);
  xTaskCreatePinnedToCore(countdownTask, "Reset Countdown", 2048, (void *)UI_COUNTDOWN_TYPE_FACTORY_RESET, 2, &fob.tasks.countdown, ARDUINO_RUNNING_CORE);
}

void initiateFactoryReset(void *arg = NULL)
{
  resetPreferences();
  savePreferences();
  startFactoryResetCountdown();
}

void uiMenuInit(void)
{
  MinuPage countdownPage(NULL, fob.menu.numPages());
  countdownPage.setOpenedCallback(pageOpenedCallback);
  countdownPage.setClosedCallback(pageClosedCallback);
  countdownPage.setRenderedCallback(pageRenderedCallback);
  countdownPageId = fob.menu.addPage(countdownPage);

  MinuPage homePage("1SIMPLECONNECT", fob.menu.numPages());
  homepageWifiItem = homePage.addItem(goToWiFiPage, "Wi-Fi", " ", updateWiFiItem);
  homePage.addItem(goToPingTargetsPage, "Network Diags", NULL);
  homePage.addItem(goToRouterPage, "Router", NULL);
  homePage.addItem(goToTimePage, "Time", NULL);
  homePage.addItem(goToSensorsPage, "Sensors", NULL);
  homePage.addItem(goToFobInfoPage, "Fob Info", NULL);
  homePage.addItem(startShutdownCountdown, "ShutDown", NULL);
  homePage.addItem(startRebootCountdown, "Reboot", NULL);
  homePage.addItem(goToFactoryResetPage, "Factory Reset", NULL);
  homePage.setOpenedCallback(pageOpenedCallback);
  homePage.setClosedCallback(pageClosedCallback);
  homePage.setRenderedCallback(pageRenderedCallback);
  homePageId = fob.menu.addPage(homePage);

  MinuPage wifiPage("WI-FI", fob.menu.numPages());
  wifiPage.addItem(startWiFiConnectCountdown, "Connect STA", NULL);
  wifiPage.addItem(startWiFiAP, "Start AP", NULL);
  wifiPage.addItem(startWiFiScan, "Scan", NULL);
  wifiStatusItem = wifiPage.addItem(goToHomePage, "<--", NULL);
  wifiPage.setOpenedCallback(pageOpenedCallback);
  wifiPage.setClosedCallback(pageClosedCallback);
  wifiPage.setRenderedCallback(lcdPrintWiFiStatus);
  wifiPageId = fob.menu.addPage(wifiPage);

  MinuPage wifiPromptPage("WI-FI DISCON", fob.menu.numPages());
  wifiPromptPage.addItem(startWiFiConnectCountdown, "Retry STA", NULL);
  wifiPromptPage.addItem(startWiFiAP, "Start AP", NULL);
  wifiPromptPage.addItem(cancelWiFiSetup, "Cancel", NULL);
  wifiPromptPage.setOpenedCallback(pageOpenedCallback);
  wifiPromptPage.setClosedCallback(pageClosedCallback);
  wifiPromptPage.setRenderedCallback(pageRenderedCallback);
  wifiPromptPageId = fob.menu.addPage(wifiPromptPage);

  MinuPage saveSSIDAsPage("SAVE SSID AS", fob.menu.numPages());
  saveSSIDAsPage.addItem(saveSSID, "Primary", NULL);
  saveSSIDAsPage.addItem(saveSSID, "Secondary", NULL);
  saveSSIDAsPage.addItem(goToWiFiPage, "<--", NULL);
  saveSSIDAsPage.setOpenedCallback(pageOpenedCallback);
  saveSSIDAsPage.setClosedCallback(pageClosedCallback);
  saveSSIDAsPage.setRenderedCallback(pageRenderedCallback);
  saveSSIDAsPageId = fob.menu.addPage(saveSSIDAsPage);

  MinuPage scanResultPage("SCAN RESULT", fob.menu.numPages());
  scanResultPage.setOpenedCallback(pageOpenedCallback);
  scanResultPage.setClosedCallback(deleteAllPageItems);
  scanResultPage.setRenderedCallback(pageRenderedCallback);
  scanResultPageId = fob.menu.addPage(scanResultPage);

  MinuPage pingTargetsPage("NETWORK DIAG", fob.menu.numPages());
  for (PingTarget target : fob.pingTargets)
    pingTargetsPage.addItem(NULL, target.displayHostname.c_str(), " ");
  pingTargetsPage.addItem(goToHomePage, "<--", NULL);
  pingTargetsPage.setOpenedCallback(startDataUpdate);
  pingTargetsPage.setClosedCallback(stopDataUpdate);
  pingTargetsPage.setRenderedCallback(pageRenderedCallback);
  pingTargetsPageId = fob.menu.addPage(pingTargetsPage);

  MinuPage routerPage("ROUTER", fob.menu.numPages());
  routerPage.addItem(goToRouterInfoPage, "Info", NULL);
  routerPage.addItem(goToRouterLocationPage, "Location", NULL);
  routerPage.addItem(goToRouterWANListPage, "WAN Status", NULL);
  routerPage.addItem(goToHomePage, "<--", NULL);
  routerPage.setOpenedCallback(pageOpenedCallback);
  routerPage.setClosedCallback(pageClosedCallback);
  routerPage.setRenderedCallback(pageRenderedCallback);
  routerPageId = fob.menu.addPage(routerPage);

  MinuPage routerInfoPage("ROUTER INFO", fob.menu.numPages(), true);
  routerInfoPage.addItem(goToRouterPage, NULL, NULL);
  routerInfoPage.setOpenedCallback(pageOpenedCallback);
  routerInfoPage.setClosedCallback(pageClosedCallback);
  routerInfoPage.setRenderedCallback(lcdPrintRouterInfo);
  routerInfoPageId = fob.menu.addPage(routerInfoPage);

  MinuPage routerLocationPage("ROUTER LOCATION", fob.menu.numPages(), true);
  routerLocationPage.addItem(goToRouterPage, NULL, NULL);
  routerLocationPage.setOpenedCallback(pageOpenedCallback);
  routerLocationPage.setClosedCallback(pageClosedCallback);
  routerLocationPage.setRenderedCallback(lcdPrintRouterLocation);
  routerLocationPageId = fob.menu.addPage(routerLocationPage);

  MinuPage routerUnavailablePage("ROUTER UNAVAILABLE", fob.menu.numPages(), true);
  routerUnavailablePage.addItem(goToHomePage, NULL, NULL);
  routerUnavailablePage.setOpenedCallback(pageOpenedCallback);
  routerUnavailablePage.setClosedCallback(pageClosedCallback);
  routerUnavailablePage.setRenderedCallback(lcdPrintRouterUnavailable);
  routerUnavailablePageId = fob.menu.addPage(routerUnavailablePage);

  MinuPage routerWANListPage("WAN LIST", fob.menu.numPages());
  routerWANListPage.setOpenedCallback(getWanList);
  routerWANListPage.setClosedCallback(wanListPageClosed);
  routerWANListPage.setRenderedCallback(pageRenderedCallback);
  routerWANListPageId = fob.menu.addPage(routerWANListPage);

  MinuPage routerWANInfoPage("WAN STATUS", fob.menu.numPages(), true);
  routerWANInfoPage.addItem(goToRouterWANListPage, NULL, NULL);
  routerWANInfoPage.setOpenedCallback(pageOpenedCallback);
  routerWANInfoPage.setClosedCallback(stopDataUpdate);
  routerWANInfoPage.setRenderedCallback(startDataUpdate);
  routerWANInfoPageId = fob.menu.addPage(routerWANInfoPage);

  MinuPage routerWANSummaryPage("WAN STATUS", fob.menu.numPages(), true);
  routerWANSummaryPage.addItem(goToRouterWANListPage, NULL, NULL);
  routerWANSummaryPage.setOpenedCallback(pageOpenedCallback);
  routerWANSummaryPage.setClosedCallback(stopDataUpdate);
  routerWANSummaryPage.setRenderedCallback(startDataUpdate);
  routerWANSummaryPageId = fob.menu.addPage(routerWANSummaryPage);

  MinuPage simListPage("SIM LIST", fob.menu.numPages());
  simListPage.setOpenedCallback(showSimList);
  simListPage.setClosedCallback(simListPageClosed);
  simListPage.setRenderedCallback(pageRenderedCallback);
  simListPageId = fob.menu.addPage(simListPage);

  MinuPage simInfoPage("SIM STATUS", fob.menu.numPages(), true);
  simInfoPage.addItem(goToSimListPage, NULL, NULL);
  simInfoPage.setOpenedCallback(pageOpenedCallback);
  simInfoPage.setClosedCallback(pageClosedCallback);
  simInfoPage.setRenderedCallback(showSimInfo);
  simInfoPageId = fob.menu.addPage(simInfoPage);

  MinuPage sensorsPage("SENSORS", fob.menu.numPages(), true);
  sensorsPage.addItem(goToHomePage, NULL, NULL);
  sensorsPage.setOpenedCallback(pageOpenedCallback);
  sensorsPage.setClosedCallback(stopDataUpdate);
  sensorsPage.setRenderedCallback(startDataUpdate);
  sensorsPageId = fob.menu.addPage(sensorsPage);

  MinuPage timePage("TIME", fob.menu.numPages(), true);
  timePage.addItem(goToHomePage, NULL, NULL);
  timePage.setOpenedCallback(pageOpenedCallback);
  timePage.setClosedCallback(stopDataUpdate);
  timePage.setRenderedCallback(startDataUpdate);
  timePageId = fob.menu.addPage(timePage);

  MinuPage fobInfoPage("FOB INFO", fob.menu.numPages(), true);
  fobInfoPage.addItem(goToHomePage, NULL, NULL);
  fobInfoPage.setOpenedCallback(pageOpenedCallback);
  fobInfoPage.setClosedCallback(stopDataUpdate);
  fobInfoPage.setRenderedCallback(startDataUpdate);
  fobInfoPageId = fob.menu.addPage(fobInfoPage);

  MinuPage factoryResetPage("FACTORY RESET", fob.menu.numPages());
  factoryResetPage.addItem(initiateFactoryReset, "Factory Reset", NULL);
  factoryResetPage.addItem(goToHomePage, "Cancel", NULL);
  factoryResetPage.setOpenedCallback(pageOpenedCallback);
  factoryResetPage.setClosedCallback(pageClosedCallback);
  factoryResetPage.setRenderedCallback(pageRenderedCallback);
  factoryResetPageId = fob.menu.addPage(factoryResetPage);

  String cookie;
   if ( homePageId < 0 ||
        wifiPageId < 0 ||
        wifiPromptPageId < 0 ||
        scanResultPageId < 0 ||
        saveSSIDAsPageId < 0 ||
        routerPageId < 0 ||
        routerInfoPageId < 0 ||
        routerLocationPageId < 0 ||
        routerUnavailablePageId < 0 ||
        routerWANListPageId < 0 ||
        routerWANInfoPageId < 0 ||
        timePageId < 0 ||
        countdownPageId < 0 ||
        fobInfoPageId < 0)
      goto err;
      
   lastVisitedPageId = wifiPageId;

  xTaskCreatePinnedToCore(screenWatchTask, "Screen Watch Task", 4096, NULL, 1, &fob.tasks.screenWatch, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(screenUpdateTask, "Screen Update Task", 4096, NULL, 1, &fob.tasks.screenUpdate, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(buttonWatchTask, "Button Task", 4096, NULL, 1, &fob.tasks.buttonWatch, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(wifiWatchTask, "WiFi Watch Task", 4096, NULL, 1, &fob.tasks.wifiWatch, ARDUINO_RUNNING_CORE);
  xTaskCreatePinnedToCore(routerConnectTask, "Router connect", 4096, NULL, 1, &fob.tasks.routerConnect, ARDUINO_RUNNING_CORE);
  startWiFiConnectCountdown();

  while(fob.booting)
    delay(10);

  if (WiFi.status() == WL_CONNECTED)
  {
    fob.routers.router.setIP(fob.routers.ip);
    fob.routers.router.setPort(fob.routers.port);
    cookie = fob.routers.router.begin(fob.routers.username, fob.routers.password, fob.routers.clientName, fob.routers.clientScope, true);
    if (!cookie.length())
      Serial.println("!!Router init Fail!!");
    else
    {
      Serial.println("Router connected!");
      if(fob.menu.currentPageId() == pingTargetsPageId)
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
  ssize_t lastSelectedPage = fob.menu.currentPageId();
  ssize_t lastHighlightedItem = fob.menu.currentPage()->highlightedIndex();

  bool updateScreen = false;
  long lastStackCheckTime = 0;

  for (;;)
  {
    // If a short press of A is detected, highlight the next item
    if (fob.buttons.btnPressA == STARLINKFOB_BUTTONPRESS_SHORT)
    {
#ifdef UI_BEEP
      M5.Speaker.tone(8000, 30);
#endif
      if (fob.menu.currentPageId() != countdownPageId)
      {  
        fob.menu.currentPage()->highlightNextItem();
        if(lastHighlightedItem != fob.menu.currentPage()->highlightedIndex())
          updateScreen = true;
      }
      fob.buttons.btnPressA = STARLINKFOB_BUTTONPRESS_NONE;
    }

    // If a long press of A or short press of B is detected, select the currently highlighted item
    if (fob.buttons.btnPressA == STARLINKFOB_BUTTONPRESS_LONG || fob.buttons.btnPressB == STARLINKFOB_BUTTONPRESS_SHORT)
    {
#ifdef UI_BEEP
      M5.Speaker.tone(5000, 30);
#endif
      if (fob.menu.currentPageId() != countdownPageId && fob.menu.currentPage()->highlightedIndex() >= 0)
      {
        MinuPageItem highlightedItem = fob.menu.currentPage()->highlightedItem();
        if (highlightedItem.link())
          highlightedItem.link()(&highlightedItem);
#ifdef UI_DEBUG_LOG
        Serial.printf("Executed selected item link = %p\n", highlightedItem.link());
#endif
      }

      fob.buttons.btnPressA = STARLINKFOB_BUTTONPRESS_NONE;
      fob.buttons.btnPressB = STARLINKFOB_BUTTONPRESS_NONE;
    }

    // If a long press of B is detected, cancel any ongoing shutdown
    if (fob.buttons.btnPressB == STARLINKFOB_BUTTONPRESS_LONG)
    {
      if (fob.tasks.countdown)
        xTaskNotify(fob.tasks.countdown, 1, eSetValueWithOverwrite);

      fob.buttons.btnPressB = STARLINKFOB_BUTTONPRESS_NONE;
    }

    if (fob.buttons.btnPressC == STARLINKFOB_BUTTONPRESS_SHORT)
    {
      if(!fob.booting && lastVisitedPageId != countdownPageId && lastVisitedPageId != scanResultPageId)
      {
        size_t tmp = lastSelectedPage;
        lastSelectedPage = fob.menu.currentPageId();
        fob.menu.goToPage(tmp);
      }

      fob.buttons.btnPressC = STARLINKFOB_BUTTONPRESS_NONE;
    }

    // If the current page has changed, log the change
    if (lastSelectedPage != fob.menu.currentPageId())
    {
#ifdef UI_DEBUG_LOG
      Serial.printf("Changed from page %d to page %d\n", lastSelectedPage, fob.menu.currentPageId());
#endif
      lastVisitedPageId = lastSelectedPage;
      lastSelectedPage = fob.menu.currentPageId();
      lastHighlightedItem = fob.menu.currentPage()->highlightedIndex();
    }

    // If the current page has changed, log the change
    if (lastHighlightedItem != fob.menu.currentPage()->highlightedIndex())
    {
#ifdef UI_DEBUG_LOG
      Serial.printf("Highlighted item changed: Old = %d | New = %d\n", lastHighlightedItem, fob.menu.currentPage()->highlightedIndex());
#endif
      lastHighlightedItem = fob.menu.currentPage()->highlightedIndex();
    }

    if (updateScreen || !fob.menu.rendered())
    {
      updateScreen = false;
      if (fob.tasks.countdown && fob.menu.currentPageId() != countdownPageId)
      {
        vTaskDelete(fob.tasks.countdown);
        fob.tasks.countdown = NULL;
      }

      if (fob.tasks.screenUpdate)
        xTaskNotify(fob.tasks.screenUpdate, 1, eSetValueWithOverwrite);
      while(!fob.menu.rendered())
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
  long currentTime = 0;
  bool beepA = true;
  bool beepB = true;

  for (;;)
  {
    M5.update();
    currentTime = millis();
    if (M5.BtnA.wasPressed())
    {
      fob.buttons.lastPressTimeA = currentTime;
      fob.buttons.isPressedA = true;
    }
    
    if(fob.buttons.isPressedA && beepA && (currentTime - fob.buttons.lastPressTimeA) >= LONG_PRESS_THRESHOLD_MS)
    {
      beepA = false;
      M5.Speaker.tone(9000, 50);
    }

    if (M5.BtnA.wasReleased())
    {
      fob.buttons.isPressedA = false;
      fob.buttons.pressDurationA = currentTime - fob.buttons.lastPressTimeA;
      if (fob.buttons.pressDurationA > LONG_PRESS_THRESHOLD_MS)
      {
        fob.buttons.btnPressA = STARLINKFOB_BUTTONPRESS_LONG;
#ifdef UI_DEBUG_LOG
        Serial.print("Long");
#endif
      }
      else
      {
        fob.buttons.btnPressA = STARLINKFOB_BUTTONPRESS_SHORT;
#ifdef UI_DEBUG_LOG
        Serial.print("Short");
#endif
      }
#ifdef UI_DEBUG_LOG
      Serial.printf(" press A: %lums\n", fob.buttons.pressDurationA);
#endif
      beepA = true;
    }

    if (M5.BtnB.wasPressed())
    {
      fob.buttons.lastPressTimeB = currentTime;
      fob.buttons.isPressedB = true;
    }
    
    if(fob.buttons.isPressedB && beepB && (currentTime - fob.buttons.lastPressTimeB) >= LONG_PRESS_THRESHOLD_MS)
    {
      beepB = false;
      M5.Speaker.tone(7000, 50);
    }

    if (M5.BtnB.wasReleased())
    {
      fob.buttons.isPressedB = false;
      fob.buttons.pressDurationB = currentTime - fob.buttons.lastPressTimeB;
      if (fob.buttons.pressDurationB > LONG_PRESS_THRESHOLD_MS)
      {
        fob.buttons.btnPressB = STARLINKFOB_BUTTONPRESS_LONG;
#ifdef UI_DEBUG_LOG
        Serial.print("Long");
#endif
      }
      else
      {
        fob.buttons.btnPressB = STARLINKFOB_BUTTONPRESS_SHORT;
#ifdef UI_DEBUG_LOG
        Serial.print("Short");
#endif
      }
#ifdef UI_DEBUG_LOG
      Serial.printf(" press B: %lums\n", fob.buttons.pressDurationB);
#endif
      beepB = true;
    }

    if (M5.BtnPWR.wasPressed())
      fob.buttons.lastPressTimeC = currentTime;

    if (M5.BtnPWR.wasReleased())
    {
      fob.buttons.pressDurationC = currentTime - fob.buttons.lastPressTimeC;
      if (fob.buttons.pressDurationC > LONG_PRESS_THRESHOLD_MS)
      {
        fob.buttons.btnPressC = STARLINKFOB_BUTTONPRESS_LONG;
#ifdef UI_DEBUG_LOG
        Serial.print("Long");
#endif
      }
      else
      {
        fob.buttons.btnPressC = STARLINKFOB_BUTTONPRESS_SHORT;
#ifdef UI_DEBUG_LOG
        Serial.print("Short");
#endif
      }
#ifdef UI_DEBUG_LOG
      Serial.printf(" press C: %lums\n", fob.buttons.pressDurationC);
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
    min = fob.wifi.timeoutMs ? (fob.wifi.timeoutMs / 1000) / 60 : WIFI_COUNTDOWN_SECONDS / 60;
    sec = fob.wifi.timeoutMs ? (fob.wifi.timeoutMs / 1000) % 60 : WIFI_COUNTDOWN_SECONDS % 60;
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
  else if( countdownType == UI_COUNTDOWN_TYPE_ROUTER)
  {
    min = WIFI_COUNTDOWN_SECONDS / 60;
    sec = WIFI_COUNTDOWN_SECONDS % 60;
  }
  else
  {
#ifdef UI_DEBUG_LOG
    Serial.println("Unspecified countdown type!");
#endif
    vTaskDelete(NULL);
  }

  while (!fob.menu.rendered())
    delay(10);

  if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
    M5.Lcd.printf("SSID: %s\n\n", (fob.wifi.usePrimarySsid) ? fob.wifi.ssidStaPrimary.c_str() : fob.wifi.ssidStaSecondary.c_str());

  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();


  M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);

  while (min >= 0 || sec >= 0)
  {
    if (countdownType == UI_COUNTDOWN_TYPE_WIFI && WiFi.status() == WL_CONNECTED)
      break;
    else if(countdownType == UI_COUNTDOWN_TYPE_ROUTER && fob.routers.router.available())
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
      if (fob.wifi.usePrimarySsid)
      {
        fob.wifi.usePrimarySsid = false;
        // WiFi.mode(WIFI_MODE_NULL);
        // delay(100);
        fob.menu.goToPage(lastVisitedPageId);
        xTaskNotify(fob.tasks.wifiWatch, 1, eSetValueWithOverwrite);
      }
      else
      {
        fob.wifi.timedOut = true;
        goToWiFiPromptPage();
      }

    }
    else if (countdownType == UI_COUNTDOWN_TYPE_SHUTDOWN)
    {
      ShutdownTimestamp_t sTime;
      auto dt = M5.Rtc.getDateTime();
      snprintf(sTime.lastShutdownTime, sizeof(sTime.lastShutdownTime), "%02d/%02d/%04d %02d:%02dH",
                dt.date.date, dt.date.month, dt.date.year,
                dt.time.hours, dt.time.minutes);
      snprintf(sTime.lastShutdownTimezone, sizeof(sTime.lastShutdownTimezone), "%s", TIMEZONE);
      sTime.lastShutdownRuntime = millis();

      Serial.println("Saving shutdown details:");
      Serial.printf("\tRuntime: %"PRId64"\n", sTime.lastShutdownRuntime);
      Serial.printf("\tTime: %s\n", sTime.lastShutdownTime);
      Serial.printf("\tTimezone: %s\n",  sTime.lastShutdownTimezone);

      Preferences timePrefs;
      if (timePrefs.begin(TIMESTAMP_NAMESPACE, false))
      {
          timePrefs.clear();
          if(timePrefs.putBytes(TIMESTAMP_NAMESPACE, &sTime, sizeof(sTime)))
            Serial.println("Saved timestamps to storage...shutting down now!");
      }
      timePrefs.end();
      delay(100);
      M5.Power.powerOff();
    }
    else if (countdownType == UI_COUNTDOWN_TYPE_REBOOT)
      ESP.restart();
    else if (countdownType == UI_COUNTDOWN_TYPE_ROUTER)
      goToRouterUnavailablePage();
  }
  else
  {
    if (countdownType == UI_COUNTDOWN_TYPE_WIFI)
    {
      if (stopCountdown)
      {
        fob.wifi.timedOut = true;
        goToWiFiPromptPage();
      }
      else if (fob.booting)
      {  
        M5.Lcd.clear();
        M5.Lcd.setCursor(0, cursorY);
        M5.Lcd.printf(" Connected to WiFi:\n %s\n", (fob.wifi.usePrimarySsid) ? fob.wifi.ssidStaPrimary.c_str() : fob.wifi.ssidStaSecondary.c_str());
        vTaskDelay(2000);
        if(!fob.servers.started)
        {
          fob.servers.started = true;
          Serial.println("Starting HTTP Server");
          startHttpServer();
        }
        goToPingTargetsPage();
        xTaskNotify(fob.tasks.wifiWatch, 1, eSetValueWithOverwrite);
        xTaskNotify(fob.tasks.routerConnect, 1, eSetValueWithOverwrite);
      }
      else
      {  
        M5.Lcd.clear();
        M5.Lcd.setCursor(0, cursorY);
        M5.Lcd.printf(" Connected to WiFi:\n %s\n", (fob.wifi.usePrimarySsid) ? fob.wifi.ssidStaPrimary.c_str() : fob.wifi.ssidStaSecondary.c_str());
        vTaskDelay(2000);
        fob.menu.goToPage(lastVisitedPageId);
      }
    }
    else if (countdownType == UI_COUNTDOWN_TYPE_SHUTDOWN || countdownType == UI_COUNTDOWN_TYPE_REBOOT)
      goToHomePage();
    else if (countdownType == UI_COUNTDOWN_TYPE_ROUTER)
    {
      if (stopCountdown)
        goToRouterUnavailablePage();
      else if(lastVisitedPageId == pingTargetsPageId)
        goToRouterWANListPage();
      else
        fob.menu.goToPage(lastVisitedPageId);
    }
  }

#ifdef UI_BEEP
  M5.Speaker.tone(9000, 300);
#endif

  fob.tasks.countdown = NULL;
  vTaskDelete(NULL);
}

void dataUpdateTask(void *arg)
{
  while (!fob.menu.rendered())
    delay(10);
  
  const int cursorX = M5.Lcd.getCursorX();
  const int cursorY = M5.Lcd.getCursorY();

  int updateType = (int)arg;
#ifdef UI_DEBUG_LOG
  Serial.printf("Started data update task: type %d\n", updateType);
#endif
  M5.Lcd.fillRect(0, cursorY, M5.Lcd.width(), M5.Lcd.height() - cursorY, MINU_BACKGROUND_COLOUR_DEFAULT);
  if(updateType == UI_UPDATE_TYPE_WAN_INFO)
    fob.routers.router.getWanStatus();
  while (1)
  {
    M5.Lcd.setCursor(cursorX, cursorY);
    Serial.printf("Update type %d started\n", updateType);

    if (updateType == UI_UPDATE_TYPE_TIME)
      lcdPrintTime();
    else if(updateType == UI_UPDATE_TYPE_SENSORS)
      lcdPrintSensors();
    else if (updateType == UI_UPDATE_TYPE_WAN_INFO)
      lcdPrintRouterWANInfo();
    else if (updateType == UI_UPDATE_TYPE_FOB_INFO)
      lcdPrintFobInfo();
    else if (updateType == UI_UPDATE_TYPE_PING)
      updatePingTargetsStatus();
    else if (updateType == UI_UPDATE_TYPE_WAN_SUMMARY)
      printRouterWanStatus();

    Serial.printf("Update type %d done\n", updateType);
    vTaskDelay(pdMS_TO_TICKS(1000));

    if (updateType == UI_UPDATE_TYPE_PING && fob.booting && fob.pingTargets[0].pingOK)
    {
      fob.booting = false;
      break;
    }
  }

  fob.tasks.dataUpdate = NULL;
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
      fob.menu.render(MINU_ITEM_MAX_COUNT);
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
    if (WiFi.getMode() == WIFI_MODE_STA && WiFi.status() != WL_CONNECTED && !fob.tasks.countdown && !fob.wifi.timedOut)
    {
      lastVisitedPageId = fob.menu.currentPageId();
      startWiFiConnectCountdown(NULL);
    }
    if(!fob.booting && !fob.tasks.countdown && WiFi.getMode() == WIFI_MODE_STA && WiFi.status() == WL_CONNECTED && !fob.routers.router.available())
    {
      lastVisitedPageId = fob.menu.currentPageId();
      startRouterConnectCountdown(NULL);
    }
  }
}

void routerConnectTask(void* arg)
{
  ulTaskNotifyTake(false, portMAX_DELAY);
  for(;;)
  {
    if(!fob.booting && fob.tasks.countdown && WiFi.getMode() == WIFI_MODE_STA && WiFi.status() == WL_CONNECTED && !fob.routers.router.available())
      fob.routers.router.checkAvailable();
  }
}