#include <stdint.h>

#include <SPIFFS.h>
#include <WebServer.h>
#include <Preferences.h>

#include "PeplinkAPI.h"
#include "utils.h"
#include "config.h"

extern WebServer httpServer;
File fsUploadFile;

/// @brief HTML content sent when the root URI of the webserver is requested
static const char indexPage[] =
    R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
  <title>ESP32 Peplink Fob Configuration</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <script type="text/javascript">

    var clientScope = "read-write";
    function initiateReboot() {
      var xh = new XMLHttpRequest();
      xh.onreadystatechange = function () {
        if (xh.readyState == 4) {
          if (xh.status == 200) {
            document.open();
            document.write(xh.responseText);
            document.close();
          }
        }
      };
      xh.open("POST", "/reboot", true);
      xh.send(null);
    }

    function clearCookies() {
      var xh = new XMLHttpRequest();
      xh.onreadystatechange = function () {
        if (xh.readyState == 4) {
          if (xh.status == 200) {
            document.open();
            document.write(xh.responseText);
            document.close();
          }
        }
      };
      xh.open("POST", "/cookies", true);
      xh.send(null);
    }

    function refreshLogin() {
      var xh = new XMLHttpRequest();
      xh.onreadystatechange = function () {
        if (xh.readyState == 4) {
          if (xh.status == 200) {
            document.open();
            document.write(xh.responseText);
            document.close();
          }
        }
      };
      xh.open("POST", "/login", true);
      xh.send(null);
    }

    function loadValues() {
      var xh = new XMLHttpRequest();
      xh.onreadystatechange = function () {
        if (xh.readyState == 4) {
          if (xh.status == 200) {
            var res = JSON.parse(xh.responseText);
            document.getElementById("wifi1ssid").value = res.wifi1ssid;
            document.getElementById("wifi1password").value = res.wifi1password;
            document.getElementById("wifi2ssid").value = res.wifi2ssid;
            document.getElementById("wifi2password").value = res.wifi2password;
            document.getElementById("wifissidsoftap").value = res.wifissidsoftap;
            document.getElementById("wifipasswordsoftap").value = res.wifipasswordsoftap;
            document.getElementById("ip").value = res.ip;
            document.getElementById("port").value = res.port;
            document.getElementById("username").value = res.username;
            document.getElementById("password").value = res.password;
            document.getElementById("clientname").value = res.clientname;
            document.getElementById("wifitimeout").value = res.wifitimeout;
            if (res.clientscope === "read-write") {
              document.getElementById("read-write").checked = true;
              document.getElementById("read-only").checked = false;
            }
            else {
              document.getElementById("read-write").checked = false;
              document.getElementById("read-only").checked = true;
            }
          }
        }
      };
      xh.open("GET", "/settings", true);
      xh.send(null);
    };

    function onBodyLoad() {
      loadValues();

      const form = document.forms[0];
      form.onsubmit = e => {
        e.preventDefault();
        const fd = new FormData();
        const props = {};
        for (let element of form.elements) {
          if (element.type !== "submit" && element.type !== "radio") {
            props[element.name] = element.value;
            fd.append(element.name, element.value);
          }
        }

        const clientScopeButtons = document.getElementsByName("clientscope");
        for (const clientScopeButton of clientScopeButtons) {
          if (clientScopeButton.checked) {
            props[clientScopeButton.name] = clientScopeButton.id;
            break;
          }
        }

        const json = JSON.stringify(props);
        console.log(json);

        var xh = new XMLHttpRequest();
        xh.onreadystatechange = function () {
          if (xh.readyState == 4) {
            if (xh.status == 200) {
              document.open();
              document.write(xh.responseText);
              document.close();
            }
          }
        };
        xh.open("POST", "/settings", true);
        xh.send(json);

      }

    }

  </script>
</head>

<body onload="onBodyLoad()" style="margin-bottom:50px;">
  <h1>Admin</h1>
  <button onclick="clearCookies()">
    Delete auth key
  </button>
  <button onclick="initiateReboot()">
    Reboot
  </button>
  <button onclick="refreshLogin()">Refresh login</button>
  <h1>Firmware Update</h1>
  <button onclick="window.location.href='/update';">
    Go to Update Page
  </button>
  <h1>Edit Settings</h1>
  <form action="/settings" method="POST">
    <p>Router1 SSID : <input type="text" name="wifi1ssid" id="wifi1ssid" placeholder="Router SSID" required></p>
    <p>Router1 Password : <input type="text" name="wifi1password" id="wifi1password" placeholder="Router Password"
        required></p>
    <p>Router2 SSID : <input type="text" name="wifi2ssid" id="wifi2ssid" placeholder="Router SSID" required></p>
    <p>Router2 Password : <input type="text" name="wifi2password" id="wifi2password" placeholder="Router Password"
        required></p>
    <p>SoftAP SSID : <input type="text" name="wifissidsoftap" id="wifissidsoftap" placeholder="SoftAP SSID" required></p>
    <p>SoftAP Password : <input type="text" name="wifipasswordsoftap" id="wifipasswordsoftap" placeholder="SoftAP Password"
        required></p>
    <br>
    <p>Router IP : <input type="text" name="ip" id="ip" placeholder="Router IP" required></p>
    <p>Router Port : <input type="number" min="0" max="65535" name="port" id="port" placeholder="Router Port" required>
    </p>
    <p>Router API Username : <input type="text" name="username" id="username" placeholder="Router API Username"
        required></p>
    <p>Router API Password : <input type="text" name="password" id="password" placeholder="Router API Password"
        required></p>
    <br>
    <p>Router Client Name : <input type="text" name="clientname" id="clientname" placeholder="Router Client Name"
        required></p>
    <p>Router Client Scope : <br>
      <input type="radio" name="clientscope" id="read-write"><label>Read-write</label><br>
      <input type="radio" name="clientscope" id="read-only"><label>Read-only</label><br>
    </p>
    <p>Wi-Fi connect timeout : <input type="number" min="5000" name="wifitimeout" id="wifitimeout"
        placeholder="Wi-Fi connect timeout" required></p>
    <input type="submit" value="Submit">
  </form>
</body>

</html>
)rawliteral";

/// @brief Returns a MIME type based on filename
String getContentType(String filename)
{
  if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";

  return "text/plain";
}

/// @brief Checks whether the file at the specified path exists
static bool exists(String path)
{
  bool yes = false;
  File file = SPIFFS.open(path, "r");

  if (!file.isDirectory())
    yes = true;

  file.close();
  return yes;
}

/// @brief Handles file download requests
static bool handleFileRead(String path)
{
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";

  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";

  if(path == "/index.html")
  {
    httpServer.send(200, "text/html", indexPage);
    return true;
  }
  else if (exists(pathWithGz) || exists(path))
  {
    if (exists(pathWithGz))
      path += ".gz";

    File file = SPIFFS.open(path, "r");
    httpServer.streamFile(file, contentType);
    file.close();
    return true;
  }

  return false;
}

/// @brief Applies settings received as JSON from the web interface
static void unpackSettings(const char* json)
{
  Serial.println("Parsing settings json: ");
  Serial.println(json);
  JsonDocument settingsDoc;
  deserializeJson(settingsDoc, json);

  wifi1Ssid = settingsDoc["wifi1ssid"].as<String>();
  wifi1Password = settingsDoc["wifi1password"].as<String>();
  wifi2Ssid = settingsDoc["wifi2ssid"].as<String>();
  wifi2Password = settingsDoc["wifi2password"].as<String>();
  wifiSsidSoftAp = settingsDoc["wifissidsoftap"].as<String>();
  wifiPasswordSoftAp = settingsDoc["wifipasswordsoftap"].as<String>();
  routerIP = settingsDoc["ip"].as<String>();
  routerPort = settingsDoc["port"].as<int>();
  routerUsername = settingsDoc["username"].as<String>();
  routerPassword = settingsDoc["password"].as<String>();
  routerClientName = settingsDoc["clientname"].as<String>();
  routerClientScope = ((settingsDoc["clientscope"].as<String>() == "read-write") ? CLIENT_SCOPE_READ_WRITE : CLIENT_SCOPE_READ_ONLY);
  wifiTimeoutMs = settingsDoc["wifitimeout"].as<int>();

}

/// @brief Set URI handlers and start the HTTP server
void startHttpServer()
{
  httpServer.on("/settings", HTTP_POST, []()
  {
    if(httpServer.args() == 0)
      return;

#ifdef WEBSERVER_DEBUG_LOG
    int args = httpServer.args();
    Serial.printf("Server has %d args:\n", args);
    for(int i  = 0; i < args; i++)
      Serial.printf("Arg %s: %s\n", httpServer.argName(i).c_str(), httpServer.arg(i).c_str());
#endif

    unpackSettings(httpServer.arg("plain").c_str());

#ifdef WEBSERVER_DEBUG_LOG
    Serial.println("Got new preferences from web interface: ");
    dumpPreferences();
#endif
    // Response returned to confirm that the preferences have been successfully received
    String responsePage = 
    String(
      R"rawliteral(
      <!DOCTYPE html>
      <html>
        <head>
          <meta http-equiv="refresh" content="10; url=index.html" />
        </head>
        <script>
        function countDown()
        {
          var count=10;
          var counter=setInterval(timer, 1000);
          function timer()
          {
            count=count-1;
            if (count <= 0)
            {
              clearInterval(counter);
              return;
            }
          document.getElementById("counter").innerHTML=count;
          }
        }
        </script>

        <body onload="countDown()">
          <p>Parameters set successfully. Restart device for the new preferences to take effect.</p>
          <p>This page will automatically redirect to the home page in <span id ="counter"></span> seconds...</p>
          <h1>New Settings</h1>
          <p>)rawliteral") +

    String("Router1 SSID : " + wifi1Ssid + String("<br>")) +
    String("Router1 Password :" + wifi1Password + String("<br>")) +
    String("Router2 SSID : " + wifi2Ssid + String("<br>")) +
    String("Router2 Password :" + wifi2Password + String("<br>")) +
    String("SoftAP SSID : " + wifiSsidSoftAp + String("<br>")) +
    String("SoftAP Password :" + wifiPasswordSoftAp + String("<br>")) +
    String("Router IP : " + routerIP + String("<br>")) +
    String("Router Port : " + String(routerPort) + String("<br>")) +
    String("Router API Username : " + routerUsername + String("<br>")) +
    String("Router API Password : " + routerPassword + String("<br>")) +
    String("Router Client Name : " + routerClientName + String("<br>")) +
    String("Router Client Scope : " + String((routerClientScope == CLIENT_SCOPE_READ_WRITE)?"read-write":"read-only") + String("<br>")) +
    String("Wi-Fi connect timeout : " + String(wifiTimeoutMs) + String("<br>")) +

    String(R"rawliteral(
          </p>
        </body>
      </html>
    )rawliteral");

    // Save new preferences to NVS
    if (savePreferences())
    {
      httpServer.send(200, "text/html", responsePage.c_str());
      ESP.restart();
    }
    else
      httpServer.send(500, "text/plain", "Couldn't set Parameters");

  });

  // Called when the current user-define preferences are requested
  httpServer.on("/settings", HTTP_GET, []()
  {
    String json = "{";
    json += "\"wifi1ssid\":\"" + wifi1Ssid;
    json += "\",\"wifi1password\":\"" + wifi1Password;
    json += "\",\"wifi2ssid\":\"" + wifi2Ssid;
    json += "\",\"wifi2password\":\"" + wifi2Password;
    json += "\",\"wifissidsoftap\":\"" + wifiSsidSoftAp;
    json += "\",\"wifipasswordsoftap\":\"" + wifiPasswordSoftAp;
    json += "\",\"ip\":\"" + routerIP;
    json += "\",\"port\":\"" + String(routerPort);
    json += "\",\"username\":\"" + routerUsername;
    json += "\",\"password\":\"" + routerPassword;
    json += "\",\"clientname\":\"" + routerClientName;
    json += "\",\"clientscope\":\"" + String((routerClientScope == CLIENT_SCOPE_READ_WRITE) ? "read-write" : "read-only");
    json += "\",\"wifitimeout\":\"" + String(wifiTimeoutMs);
    json += "\"}";
    httpServer.send(200, "application/json", json);
    json = String();
  });

  // Called when a reboot is requested
  httpServer.on("/reboot", HTTP_POST, []()
  {
    String responsePage = String(
    R"rawliteral(
      <!DOCTYPE html>
      <html>
        <head>
          <meta http-equiv="refresh" content="10; url=index.html" />
        </head>
        <script>
        function countDown()
        {
          var count=10;
          var counter=setInterval(timer, 1000);
          function timer()
          {
            count=count-1;
            if (count <= 0)
            {
              clearInterval(counter);
              return;
            }
          document.getElementById("counter").innerHTML=count;
          }
        }
        </script>

        <body onload="countDown()">
          <p>Restarting device....</p>
          <p>This page will automatically redirect to the home page in <span id ="counter"></span> seconds...</p>
          
        </body>
      </html>
    )rawliteral");

      httpServer.send(200, "text/html", responsePage.c_str());
      ESP.restart();
  });
  
  // Called when cookies are requested to be cleared
  httpServer.on("/cookies", HTTP_POST, []()
  {
    String responsePage = String(
    R"rawliteral(
      <!DOCTYPE html>
      <html>
        <head>
          <meta http-equiv="refresh" content="10; url=index.html" />
        </head>
        <script>
        function countDown()
        {
          var count=10;
          var counter=setInterval(timer, 1000);
          function timer()
          {
            count=count-1;
            if (count <= 0)
            {
              clearInterval(counter);
              return;
            }
          document.getElementById("counter").innerHTML=count;
          }
        }
        </script>

        <body onload="countDown()">
          <p>Authenticaion keys deleted from storage. Restarting device....</p>
          <p>This page will automatically redirect to the home page in <span id ="counter"></span> seconds...</p>
          
        </body>
      </html>
    )rawliteral");

    // Clear server authentication credentials from NVS
    Preferences prefs;

    if (prefs.begin(COOKIES_NAMESPACE, false))
    {
      prefs.clear(); // Clear any currently stored cookies
      prefs.end();
      httpServer.send(200, "text/html", responsePage.c_str());
    }
    else
      httpServer.send(500, "text/plain", "Couldn't set Parameters");

  });

  // Called when a new router login is requested
  httpServer.on("/login", HTTP_POST, []()
                {
    String responsePage = String(
    R"rawliteral(
      <!DOCTYPE html>
      <html>
        <head>
          <meta http-equiv="refresh" content="5; url=index.html" />
        </head>
        <script>
        function countDown()
        {
          var count=5;
          var counter=setInterval(timer, 1000);
          function timer()
          {
            count=count-1;
            if (count <= 0)
            {
              clearInterval(counter);
              return;
            }
          document.getElementById("counter").innerHTML=count;
          }
        }
        </script>

        <body onload="countDown()">
          <p>Log in refreshed....</p>
          <p>This page will automatically redirect to the home page in <span id ="counter"></span> seconds...</p>
          
        </body>
      </html>
    )rawliteral");

    router.setIP(routerIP);
    router.setPort(routerPort);
    String cookie = router.begin(routerUsername, routerPassword, routerClientName, routerClientScope, false);
    if (!cookie.length())
    {
      Serial.println("!!Router init Fail!!");
      httpServer.send(500, "text/plain", "Couldn't log in");  
    }
    else
      httpServer.send(200, "text/html", responsePage.c_str());
    
  });

  // Called when the requested path is not available.
  httpServer.onNotFound([]()
                        {
    if (!handleFileRead(httpServer.uri())) {
      // This will not be executed since handleFileRead returns the index page for any unknown URI
      httpServer.send(404, "text/plain", "FileNotFound");
    } });

  // Attach the HTTP update service to the HTTP server
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  Serial.println("HTTP server started");
}