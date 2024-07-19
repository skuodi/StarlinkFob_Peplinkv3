# StarlinkFob_Peplink_v3

## Contents
- [Misc](#misc)
  - [x] [1. Comments](#1-comments)
  - [x] [2. Debug logs](#2-debug-logs)
  - [3. Logic flow](#3-logic-flow)
  - [x] [4. Guide to adding new menus](#4-guide-to-adding-new-menus)
  - [5. Primary and secondary Wi-Fi](#5-primary-and-secondary-wi-fi)
- [Core features](#core-features)
  - [x] [1. Use asynchronous delays](#1-use-asynchronous-delays)
  - [x] [2. Dual-core configuration](#2-dual-core-configuration)
  - [3. Beep on long press only](#3-beep-on-long-press-only)
  - [4. Add button C functionality](#4-add-button-c-functionality)
  - [5. Interrupts for button press detection](#5-interrupts-for-button-press-detection)
  - [x] [6. Home page rendering issue](#6-home-page-rendering-issue)
  - [x] [7. HTTP server early startup](#7-http-server-early-startup)
  - [8. Startup sequence](#8-startup-sequence)
  - [9. New splash screen sequence](#9-new-splash-screen-sequence)
  - [x] [10. Update time screen](#10-update-time-screen)
  - [x] [11. Add sensors screen](#11-add-sensors-screen)
  - [12. Changes to Wi-Fi post-connection sequence](#12-changes-to-wi-fi-post-connection-sequence)
  - [13. USB power loss detection](#13-usb-power-loss-detection)
  - [x] [14. Temperature alarm \& logging](#14-temperature-alarm--logging)
  - [x] [15. Ping screen loop over hosts](#15-ping-screen-loop-over-hosts)
- [Network](#network)
  - [1. HTTPS ignore certificate CN verification](#1-https-ignore-certificate-cn-verification)
- [Peplink](#peplink)
  - [1. Loading indication](#1-loading-indication)
  - [2. WAN list sort by priority](#2-wan-list-sort-by-priority)
  - [3. SIM List page](#3-sim-list-page)
  - [4. WAN status PAGE](#4-wan-status-page)
  - [5. WAN summary page display](#5-wan-summary-page-display)
  - [6. Cellular signal indicator](#6-cellular-signal-indicator)
  - [7. Add router reboot option](#7-add-router-reboot-option)

## Misc

### 1. Comments
- Comments have been added

### 2. Debug logs

```
Please add lots of comments in the code, I would like to learn from this project.
Implement debug mode that outputs a lot more output to serial, and also suppresses a lot when it's off.
```

- The following flags can be defined at the top of `config.h` and control the printing of debug logs for different translation units:
  - `ENABLE_VERBOSE_DEBUG_LOG` - enables all debug logs
  - `PEPLINK_DEBUG_LOG` - enables debug printing for Peplink API functions
  - `UI_DEBUG_LOG` - enables debug logs for UI functions
  - `WEBSERVER_DEBUG_LOG` - enables debug logs for webserver functions

### 3. Logic flow  

```
Separately add a readme that has a simple sequence of startup/shutdown/error condition flows.
```

### 4. Guide to adding new menus
```
Also a readme document on how to add new menus.
```

- Available in the [*Usage* section of the Minu library README]().

### 5. Primary and secondary Wi-Fi
```
Explain how the two wifi's work, I'm not sure I understand/it doesn't seem to work for me.
```
## Core features

### 1. Use asynchronous delays

```
Change all delay() to millis(), code should still be executing for getting connected to wifi , pinging hosts, logging into router ,fetching json, parsing json, regardless of how long we want to displaying a screen.
```

- `delay()`s are only used in the splash screen, when waiting for Wi-Fi to switch modes and when waiting for the ui to be updated.
During the splash screen, it is necessary to wait because the next screen is a countdown screen and both cannot be displayed at the same time. Other than during the splash screen, there are upto 5 separate threads running concurrently, meaning that if a delay is requested in one thread all the rest will continue performing without interruption.

### 2. Dual-core configuration

```
How can we use the second core for fetching json, or utilize pinned tasks?
```

- In ESP32 terminology, the two CPU cores are called App(User application) CPU and Pro(Background process) CPU. I'm pinning all tasks to run on the App CPU because the Pro CPU is designated for critical processes such as Wi-Fi and networking and it is therefore recommended to avoid using it for user applications.

Also, using the Pro CPU. to fetch JSON does not offer any performance improvements since we'd still have to idle-wait for the JSON to be fetched and parsed before we can proceed to display on  the App CPU.

But to answer the question, utilizing the second core is just a matter of changing which core a task is pinned to upon creation. The Update task does all the json fetching and parsing so this would be the task to pin.

### 3. Beep on long press only

```
Play beep when detecting long press A(M5), should be heard before having to release the button.
Play beep when detecting long press B (different tone), should be heard before having to release the button.
This will help with the user knowing how long to hold button.
No tone for short press.
```

### 4. Add button C functionality

```
Add detection of button c , short press via interrupt.
https://community.m5stack.com/topic/3722/m5stickc-plus-detect-power-button-momentary-press
If you can get detection of button C to work, make sure press go back a menu, regardless of the currently selected item.
```

### 5. Interrupts for button press detection

```
Should all buttons be detected via interrupts?
```

- The API does not expose any function to find out how long a button has been pressed until it is released. So to achieve the requested beep-on-long-press functionality, interrupts are necessary to provide flexibility in handling and it should be possible to use them on all buttons

### 6. Home page rendering issue

```
On home page the selected item does not invert white background , black text like it does on other screens.
```

### 7. HTTP server early startup
```
HTTP server doesn't seem to be running when on ping screen, seems like it starts when logging into router, can you check?
```
The HTTP server is started as soon as Wi-Fi ism connected in either AP or STA mode

### 8. Startup sequence
```
Startup sequence (define/show me how to add change the sequence of screens)
```

There is no straightforward way to implement the changing of screens, since specific conditions must be met before the page changes.

```c
/// @todo List examples of these conditions
```

### 9. New splash screen sequence
```
#1
GoBOX screen

#2
Splash screen (company name/number) (Display for 5 secs)
Implement company logo to replace text, comment out text if we change back.
https://www.hackster.io/tommyho/m5gfx-media-player-jpg-png-bmp-gif-ab76a9
Let us know how to convert/upload logo.
```

### 10. Update time screen
```
#3
Time Screen (Display for 2 secs)
-Show current time with GMT HH:MM:SS(24hr) GMT-6
-Last PWR off: DD/MM/YY HH:MM(24hr)
-Last runtime  HHHH:MM (Length of time unit was powered on)

-Add to time menu display from home page.
-Show current time with GMT-/+ number for the current TZ
-Last PWR:     DD/MM/YY HH:MM(24hr)
-Last runtime  HHHH:MM (Length of time unit was powered on)
```

### 11. Add sensors screen
```
#4
Temperature And Humidity screen (Display for 2 secs)
-Show current Temperature and Humidity screen
-Show dd/mm/yy HH:MM of last triggered alarm.
-Catch exception if Temperature sensor not connected. (display no Temperature sensor, if not connected.)

-Add Temperature And Humidity menu entry from homepage, to screen same as boot menu when triggered with exit option


```

### 12. Changes to Wi-Fi post-connection sequence
```
#5
Wifi Countdown screen

#6
After successfully connected to Wifi
Display Connected to SSID name for 2 secs

#7 Network Check screen

#9 Wan summary screen
```

### 13. USB power loss detection
```
As needed
On shutdown or detect loss of usb power
-Display Shutting down screen with 30secs time (title no pwr shutdown)
-If power is restored, abort shutdown.
-write current date/time to eeprom when power loss was detected.
-write runtime hh:mm to eeprom when power loss was detected.
-Ensure that device powers back on when usb power is restored, after a complete shutdown.
```

### 14. Temperature alarm & logging
```
Beep every 1sec when temperature exceeds 120F (Make value configurable temperature and C/F)
-Display screen temperature threshold exceeded , current temperature. (solid Red background, white text ALERT Title: Temperature threshold, current temp in large font like countdown timer.)
-Record date/time/temperature to eeprom(update date/time/temperature to eeprom as it increases)
-If a new date/time exceeds 120F, then it should overwrite the last date/time/max temperature.
Log to serial for testing as well.
```

### 15. Ping screen loop over hosts
```
When ping screen is manually select after the power on sequence.
Ping Screen (Loop over hosts, and continue to ping while on this screen)
So if a host is red, it has a chance to change to green while watching for example.
```

- Already accomplished

## Network

### 1. HTTPS ignore certificate CN verification
```
Please enable https mode ignoring hostname.
https://www.esp32.com/viewtopic.php?t=19200
```

## Peplink

### 1. Loading indication
```
For screens that Auto refresh, display o/O, blinking . ,refresh icon or something to indicate it's loading in upper right corner of Title when it's getting json data instead of fetching or Getting WAN list.
```

### 2. WAN list sort by priority
```
Look at the json returned from the api/status.wan.connection display the wan list according to the "priority": 2 values.
If a wan has the same priority, then display them in the order based first on priority, and second on the array list.
This should re-sort the order when on the WAN summary screen.
```

### 3. SIM List page
```
SIM LIST Page
 For SIM0 Display as SIM A, for SIM1 Display as SIM B.
 Display (Active) after the active one in the list.
 Example
 SIM A (Active)
 SIM B
 When going into the details of the sim if no sim detected display
 (No Sim Detected)
```

### 4. WAN status PAGE

```
Wan status PAGE
 For type Cellular
 Display networktype in white text with Blue background.
 Example LTE , LTE-A , 5G should be displayed with a blue background.
 Single m5 press refreshes just this page.
 Add id to the api call to limit the retrieved data.
 status.wan.connection?id=2
 Long press exits the menu and returns to the wan list.
 
 On PeplinkAPI_WAN_Traffic speed change ["response"]["traffic"] to ["response"]["bandwidth"]
```

### 5. WAN summary page display
```
WAN summary page display
 Display networktype in white text with Blue background.
 Example LTE , LTE-A , 5G should be displayed with a blue background.
 After the type Cellular name is displayed display the active sim card.
 For SIM0 Display as SIM A, for SIM1 Display as SIM B.
 Example [color icon] Cellular A (Display A or B as white text on a Green background)
 After the first colored status icon, IF "managementOnly": true display orange square with white + in center.
 IF "managementOnly": False keep black space icon. before displaying the name of the wan.

```

### 6. Cellular signal indicator
```
Implement cell signalLevel as icon or line draw displaying cell signal level like you see on a phone. (should occupy at most two squares)
You can use TFT_GREY as the disabled signal line, and blue as the enabled line, or see what you come up with.
```

### 7. Add router reboot option
```
Add reboot router under router menu
implement /api/cmd.system.reboot command
Set a task to watch for router reconnect.
Add a router connection countdown page, if you have wifi, but no router connection availability, timeout 5 mins.
You can test with this router remotely.
```
