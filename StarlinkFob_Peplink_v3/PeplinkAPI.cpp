#include "stdint.h"
#include "Arduino.h"
#include <ArduinoJson.h>
#include <Preferences.h>

#include "PeplinkAPI.h"
#include "config.h"

#include "utils.h"

String PeplinkRouter::login(const char *username, const char *password)
{
  String uri = "/api/login";
  String response = String();

  JsonDocument sendDoc;
  JsonDocument recvDoc;

  // Add a filter that will grab the cookie from the router HTTP response header on successful login
  const char *headerKeys[] = {"Set-Cookie"};
  Serial.println("Logging into Router with admin account");
  // Construct a JSON document containing credentials to be sent to the server
  sendDoc["username"] = username;
  sendDoc["password"] = password;
  char json_string[256];
  serializeJson(sendDoc, json_string);
#ifdef PEPLINK_DEBUG_LOG
  Serial.println("JSON Body:");
  Serial.println(json_string);
#endif
  int httpResponseCode;
  HTTPClient https;
#ifdef PEPLINK_USE_HTTPS
  https.begin(_ip, _port, uri, rootCACertificate);
#else
  https.begin(_ip, _port, uri);
#endif
  // Set the HTTP client to grab the cookie header from the router response
  https.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));

  // The content being sent to the router is JSON data
  https.addHeader("Content-Type", "application/json");

  // Perform a post request and retrieve the HTTP response code
  httpResponseCode = https.POST(json_string);

  Serial.print("HTTP RESPONSE: ");
  Serial.println(httpResponseCode);
  if (httpResponseCode > 0)
  {
    // On success, retrieve the HTTP response body
    response = https.getString();
#ifdef PEPLINK_DEBUG_LOG
    Serial.println("PAYLOAD:");
    Serial.println(response);
#endif
  }
  else
  {
    // Print the error code string on fail
    Serial.println("Error " + String(httpResponseCode) + String(" : ") + https.errorToString(httpResponseCode));
    https.end();
    return String();
  }

  https.end();
  
  // Attempt to parse the router response as a JSON document
  DeserializationError error = deserializeJson(recvDoc, response);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return String();
  }

  String opStatus = recvDoc["stat"];
  if (opStatus != "ok")
  {
    String message = recvDoc["message"].as<String>();
    Serial.print("Login error ");
    int err = recvDoc["code"].as<int>();
    Serial.print(err);
    Serial.print(": ");
    Serial.println(message);

    // On a login attempt, if a 301 : Unauthorised response means wrong login credentials are used
    if(err == 301 && message == "Unauthorized")  
      Serial.println("Login failed: Invalid credentials");

    return String();
  }

  // Verify the access rights provided by a successful login 
  if (recvDoc["response"]["permission"])
  {
    int getAllowed = recvDoc["response"]["permission"]["GET"].as<int>();
    int postAllowed = recvDoc["response"]["permission"]["POST"].as<int>();
    Serial.print("Login permissions: GET - ");
    Serial.print((getAllowed ? "allowed" : "denied"));
    Serial.print(" POST - ");
    Serial.println((postAllowed ? "allowed" : "denied"));

    if (getAllowed != 1 || postAllowed != 1)
      return String();
  }

  Serial.println("Login success");

#ifdef PEPLINK_DEBUG_LOG

  int i = https.headers();
  Serial.println("Got " + String(i) + " HTTP headers :");

  while (i--)
    Serial.println(headerKeys[i] + String(" : ") + https.header(headerKeys[i]));

#endif

  // Check for cookie field and extract if exists
  if (https.hasHeader("Set-cookie"))
  {
    String cookieField = https.header("Set-cookie");
#ifdef PEPLINK_DEBUG_LOG
    Serial.println("Cooie header = 'Set-cookie:" + cookieField + "'");
#endif
    int cookieStart = cookieField.indexOf("auth=");
    if (cookieStart == -1)
      return String();

    int cookieEnd = cookieField.indexOf(';', cookieStart);
    if (cookieEnd == -1)
      return String();

    // Cookie will either have a key of "pauth" for "http" or bauth for https
    // so extract one more character before "auth"
    String cookie = cookieField.substring(--cookieStart, cookieEnd);
    Serial.println("Got cookie : " + cookie);
    _cookie = cookie;


    // Store the cookie in our non-volatile cookie jar
    Preferences prefs;
    prefs.begin(COOKIES_NAMESPACE, false);
    
    size_t cookieJarSize = prefs.getBytesLength(COOKIES_NAMESPACE);

    // If the stored data is of the right size, extract, modify then put it back
    if (cookieJarSize == sizeof(PeplinkAPI_CookieJar_t))
    {
      uint8_t cookieBuffer[sizeof(PeplinkAPI_CookieJar_t)];
      prefs.getBytes(COOKIES_NAMESPACE, cookieBuffer, sizeof(cookieBuffer));
      PeplinkAPI_CookieJar_t *jar = (PeplinkAPI_CookieJar_t *)cookieBuffer;
      strncpy(jar->cookie, cookie.c_str(), sizeof(jar->cookie));
      prefs.putBytes(COOKIES_NAMESPACE, jar, sizeof(PeplinkAPI_CookieJar_t));
      prefs.end();
    }
    else  // Otherwise, initialize the cookie and store the new cookie into it
    {
      prefs.clear();
      PeplinkAPI_CookieJar_t jar = { .magic = 0xDEADBEEF };
      strncpy(jar.cookie, cookie.c_str(), sizeof(jar.cookie));

      prefs.putBytes(COOKIES_NAMESPACE, &jar, sizeof(jar));
      prefs.end();
    }

    return cookie;
  }

  return String();
}

PeplinkAPI_ClientInfo PeplinkRouter::_createClient(String name, PeplinkAPI_ClientScope_t scope)
{
  PeplinkAPI_ClientInfo clientInfo;
  String uri = "/api/auth.client";

  JsonDocument sendDoc;
  JsonDocument recvDoc;

  // Construct the JSON data required to create a new client
  sendDoc["action"] = "add";
  sendDoc["name"] = name;
  sendDoc["scope"] = (scope == CLIENT_SCOPE_READ_ONLY) ? "api.read-only" : "api";
  char json_string[256];
  serializeJson(sendDoc, json_string);

#ifdef PEPLINK_DEBUG_LOG
  Serial.println("JSON Body:");
  Serial.println(json_string);
#endif

  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_POST, uri, json_string);
  if (!response.length())
    return clientInfo;

  deserializeJson(recvDoc, response);

  // Extract the new client information received from the router
  clientInfo.name = recvDoc["response"]["name"].as<String>();
  clientInfo.id = recvDoc["response"]["clientId"].as<String>();
  clientInfo.secret = recvDoc["response"]["clientSecret"].as<String>();
  String clientScope = recvDoc["response"]["scope"].as<String>();

  if (clientScope == "api.read-only")
    clientInfo.scope = CLIENT_SCOPE_READ_ONLY;
  else
    clientInfo.scope = CLIENT_SCOPE_READ_WRITE;

  if (clientInfo.id.length())
  {
    // If a client was successfullt created, add it to the local list of clients
    _grantClientToken(clientInfo);
    _clients.push_back(clientInfo);
  }

  return clientInfo;
}

bool PeplinkRouter::_deleteClient(PeplinkAPI_ClientInfo &clientToDelete)
{
  String uri = "/api/auth.client";

  JsonDocument sendDoc;
  JsonDocument recvDoc;

  Serial.println("Deleting client with id: " + clientToDelete.id);
  sendDoc["action"] = "remove";
  sendDoc["clientId"] = clientToDelete.id.c_str();
  char json_string[256];
  serializeJson(sendDoc, json_string);

#ifdef PEPLINK_DEBUG_LOG
  Serial.println("JSON Body:");
  Serial.println(json_string);
#endif

  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_POST, uri, json_string);
  if (!response.length())
    return false;

  // If the request to delete a client from the router was successful, remove it from the local list of clients
  for (auto it = _clients.begin(); it != _clients.end(); ++it)
  {
    if (it->id == clientToDelete.id)
    {
      _clients.erase(it);
      break;
    }
  }

  return true;
}

void PeplinkRouter::_deleteExistingClients()
{
  Serial.println("Deleting all " + String(_clients.size()) + " clients");
  while(_clients.size())
  {
    _deleteClient(_clients[0]);
    delay(1000);
    getClientList();
    delay(1000);
  }
}

String PeplinkRouter::begin(String username, String password, String clientName, PeplinkAPI_ClientScope_t clientScope, bool deleteExistingClients)
{
    // Check that the router is accessible (ping)
    if (!isAvailable())
    {
      Serial.println("Router ping failed!! IP : " + _ip + String(" Port: ") + String(_port));
      return String();
    }

    String cookie = String();

    // If no cookie has been retrieved from storage, log in to receive one from the server
    if (!_cookie.length())
    {
      cookie = login(username.c_str(), password.c_str());
      if (!cookie.length())
        return String();
    }

    // If no access token has been retrieved from storage, get the list of existing clients and attempt to grant a new token
    // On failure, delete all clients and create one afresh. This prevents stale (outdated/unuseable) clients from accumulating.
    if(!_token.length() && getClientList())
    {
      if (deleteExistingClients)
      {
        printRouterClients(*this);

        _deleteExistingClients();
        PeplinkAPI_ClientInfo client = _createClient(clientName, clientScope);
        if (!client.id.length())
            return String();
      }
      else
      {
        if (_clients.size())
          if (!_grantClientToken(_clients[0]))
            _deleteExistingClients();
      }    
    }
    
    return _cookie;
}

bool PeplinkRouter::isAvailable()
{
    IPAddress ip;
    ip.fromString(_ip);
    _available = Ping.ping(ip);
    return _available;
}

bool PeplinkRouter::getClientList()
{
  String uri = "/api/auth.client?accessToken=" + _token;

  JsonDocument recvDoc;
  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_GET, uri, NULL);
  if (!response.length())
    return false;

  Serial.println("getClientList() success");

  deserializeJson(recvDoc, response);

  _clients.clear();

  int clientCount = recvDoc["response"].as<JsonArray>().size();
  Serial.printf("Got %d router clients\n", clientCount);

  for (JsonObject routerClient : recvDoc["response"].as<JsonArray>())
  {
    PeplinkAPI_ClientInfo clientInfo;
    clientInfo.name = routerClient["name"].as<String>();
    clientInfo.id = routerClient["clientId"].as<String>();
    clientInfo.secret = routerClient["clientSecret"].as<String>();
    clientInfo.scope = (routerClient["scope"].as<String>() == "api") ? CLIENT_SCOPE_READ_WRITE : CLIENT_SCOPE_READ_ONLY;
    _clients.push_back(clientInfo);
  }

  return true;
}

bool PeplinkRouter::_refreshToken()
{
    _token = String();
    if(getClientList())
    {
      printRouterClients(*this);
      _deleteExistingClients();
      PeplinkAPI_ClientInfo client = _createClient(CLIENT_NAME_DEFAULT, CLIENT_SCOPE_DEFAULT);
      if (!client.id.length())
        return false;
    }
    else
      return false;
    return true;
}

bool PeplinkRouter::_grantClientToken(PeplinkAPI_ClientInfo &client)
{
  String uri = "/api/auth.token.grant";

  JsonDocument sendDoc;
  JsonDocument recvDoc;

  sendDoc["clientId"] = client.id;
  sendDoc["clientSecret"] = client.secret;
  char json_string[256];
  serializeJson(sendDoc, json_string);

#ifdef PEPLINK_DEBUG_LOG
  Serial.println("JSON Body:");
  Serial.println(json_string);
#endif

  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_POST, uri, json_string);
  if (!response.length())
    return false;

  Serial.println("_grantClientToken() success");

  deserializeJson(recvDoc, response);

  client.token = recvDoc["response"]["accessToken"].as<String>();
  client.tokenExpiry = recvDoc["response"]["expiresIn"].as<int>();

  Serial.println("Got access token " + client.token + "for client " + client.id + " expiring in " + client.tokenExpiry + " seconds");
  _token = client.token;

  // Store the token in our non-volatile cookie jar
  Preferences prefs;
  prefs.begin(COOKIES_NAMESPACE, false);
  
  size_t jarSize = prefs.getBytesLength(COOKIES_NAMESPACE);

   // If the stored data is of the right size, read, modify write
  if (jarSize == sizeof(PeplinkAPI_CookieJar_t))
  {
    uint8_t jarBuffer[sizeof(PeplinkAPI_CookieJar_t)];
    prefs.getBytes(COOKIES_NAMESPACE, jarBuffer, sizeof(jarBuffer));
    PeplinkAPI_CookieJar_t *jar = (PeplinkAPI_CookieJar_t *)jarBuffer;
    strncpy(jar->token, _token.c_str(), sizeof(jar->token));
    prefs.putBytes(COOKIES_NAMESPACE, jar, sizeof(PeplinkAPI_CookieJar_t));
    prefs.end();
  }
  else
  {
    prefs.clear();
    PeplinkAPI_CookieJar_t jar = { .magic = 0xDEADBEEF };
    strncpy(jar.token, _token.c_str(), sizeof(jar.token));
    
    prefs.putBytes(COOKIES_NAMESPACE, &jar, sizeof(jar));
    prefs.end();
  }
  return true;
}

bool PeplinkRouter::getWanTraffic(uint8_t id)
{
  
  String uri = "/api/status.traffic?accessToken=" + _token;

  JsonDocument recvDoc;
  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_GET, uri, NULL);
  if (!response.length())
    return false;

  Serial.println("getWanTraffic() success");

  deserializeJson(recvDoc, response);

  PeplinkAPI_WAN_Traffic speed;
  for (int key : recvDoc["response"]["bandwidth"]["order"].as<JsonArray>())
  {
    speed.name = recvDoc["response"]["bandwidth"][String(key).c_str()]["name"].as<String>();
    speed.download = recvDoc["response"]["bandwidth"][String(key).c_str()]["overall"]["download"].as<int>();
    speed.upload = recvDoc["response"]["bandwidth"][String(key).c_str()]["overall"]["upload"].as<int>();
    speed.unit = recvDoc["response"]["bandwidth"]["unit"].as<String>();
    speed.id = key;
    _wanTraffic.push_back(speed);
  }

  if(id == 0)
  {
    for(size_t i = 0; i < _wanTraffic.size(), i < _wan.size(); ++i)
    {
      _wan[i]->download = _wanTraffic[i].download;
      _wan[i]->upload = _wanTraffic[i].upload;
      _wan[i]->unit = _wanTraffic[i].unit;
    }
  }
  else
  {  
    for(size_t i = 0; i < _wanTraffic.size(); ++i)
    {
      if(id == _wanTraffic[i].id)
      {
        _wan[0]->download = _wanTraffic[i].download;
        _wan[0]->upload = _wanTraffic[i].upload;
        _wan[0]->unit = _wanTraffic[i].unit;
        break;
      }
    }
  }

  return true;
} 

bool PeplinkRouter::getWanStatus(uint8_t id)
{
  for(PeplinkAPI_WAN* wan: _wan)
    delete wan;
  _wan.clear();

  String uri = "/api/status.wan.connection?accessToken=" + _token;
  if(id != 0 && id <= 3)
    uri += "&id=" + String(id);

  JsonDocument recvDoc;
  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_GET, uri, NULL);
  if (!response.length())
    return false;

  Serial.println("getWanStatus() success");

  deserializeJson(recvDoc, response);

  // Extract the ordered list of available WANs
  for (int key : recvDoc["response"]["order"].as<JsonArray>())
  {
    String wanInfo = recvDoc["response"][String(key).c_str()].as<String>();
    String wanType = recvDoc["response"][String(key).c_str()]["type"];

    if (wanType == "ethernet")
    {
      PeplinkAPI_WAN_Ethernet wan = _parseEthernetWAN(wanInfo);
      wan.id = key;
      if (wan.name.length())
        _wan.push_back((PeplinkAPI_WAN *)new PeplinkAPI_WAN_Ethernet(wan));
    }
    else if (wanType == "cellular")
    {
      PeplinkAPI_WAN_Cellular wan = _parseCellularWAN(wanInfo);
      wan.id = key;
      if (wan.name.length())
        _wan.push_back((PeplinkAPI_WAN *)new PeplinkAPI_WAN_Cellular(wan));
    }
    else if (wanType == "wifi")
    {
      PeplinkAPI_WAN_WiFi wan = _parseWiFiWAN(wanInfo);
      wan.id = key;
      if (wan.name.length())
        _wan.push_back((PeplinkAPI_WAN *)new PeplinkAPI_WAN_WiFi(wan));
    }
    else
      Serial.println("Unsupported WAN type: " + wanType);
  }

  // Rearrange the WAN list based on priority
  if(this->_wan.size() > 1)
  {
    size_t wanCount = _wan.size();
    size_t currentWanPriority = 1;

    for(size_t i = 0; i < wanCount; ++i)
    {
      for(size_t j = 0; j < wanCount; ++j)
      {
        if(_wan[i]->priority == currentWanPriority)
        {
          _wan.push_back(_wan[i]);
          break;
        }
      }
      currentWanPriority++;
    }

    // Append the rest of the WANs with no priority assigned
    for(size_t i = 0; i < wanCount; ++i)
      if(_wan[i]->priority == 0)
        _wan.push_back(_wan[i]);

    // Erase the old list
    for(size_t i = 0; i < wanCount; i++)
      _wan.erase(_wan.begin());

  }
  getWanTraffic(id);
  return true;
}

String PeplinkRouter::_sendJsonRequest(PeplinkAPI_HTTPRequest_t type, String &endpoint, char *body)
{
  JsonDocument recvDoc;
  String response = String();
  int httpResponseCode;
  HTTPClient https;
  
#ifdef PEPLINK_USE_HTTPS
  https.begin(_ip, _port, endpoint, rootCACertificate);
#else
  https.begin(_ip, _port, endpoint);
#endif

  https.addHeader("Cookie", _cookie.c_str());

  switch (type)
  {
  case PEPLINKAPI_HTTP_REQUEST_POST:
  {
    https.addHeader("Content-Type", "application/json");
    httpResponseCode = https.POST(body);
  }
    break;
  case PEPLINKAPI_HTTP_REQUEST_GET:
    httpResponseCode = https.GET();
    break;
  }

  Serial.print("HTTP RESPONSE: ");
  Serial.println(httpResponseCode);
  if (httpResponseCode > 0)
  {
    response = https.getString();
#ifdef PEPLINK_DEBUG_LOG
    Serial.println("PAYLOAD:");
    Serial.println(response);
#endif
  }
  else
  {
    Serial.println("Error " + String(httpResponseCode) + String(" : ") + https.errorToString(httpResponseCode));
    https.end();
    return String();
  }
  https.end();

  DeserializationError error = deserializeJson(recvDoc, response);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return String();
  }

  String opStatus = recvDoc["stat"];
  if (opStatus != "ok")
  {
    String message = recvDoc["message"].as<String>();
    Serial.print("Operation error ");
    int err = recvDoc["code"].as<int>();
    Serial.print(err);
    Serial.print(": ");
    Serial.println(message);

    if(err == 401 && message == "Unauthorized") // If cookie expired, log in again
      login(fob.routers.username.c_str(), fob.routers.password.c_str());
    else if(err == 401 && message == "Invalid access token") //If access token expired, refresh it
      _refreshToken();

    return String();
  }

  return response;
}

PeplinkAPI_WAN_Ethernet PeplinkRouter::_parseEthernetWAN(String& wanInfo)
{
  JsonDocument wanDoc;
  deserializeJson(wanDoc, wanInfo);
  PeplinkAPI_WAN_Ethernet wan;

  wan.name = wanDoc["name"].as<String>();
  wan.type = PEPLINKAPI_WAN_TYPE_ETHERNET;
  wan.status = wanDoc["message"].as<String>();
  wan.statusLED = wanDoc["statusLed"].as<String>();
  wan.priority = wanDoc["priority"].as<int>();
  wan.managementOnly = wanDoc["managementOnly"].as<bool>();

  if (wan.status == "Disabled")
    return wan;

  wan.ip = wanDoc["ip"].as<String>();
  return wan;
}

PeplinkAPI_WAN_Cellular PeplinkRouter::_parseCellularWAN(String& wanInfo)
{
  JsonDocument wanDoc;
  deserializeJson(wanDoc, wanInfo);

  PeplinkAPI_WAN_Cellular wan;

  wan.name = wanDoc["name"].as<String>();
  wan.type = PEPLINKAPI_WAN_TYPE_CELLULAR;
  wan.status = wanDoc["message"].as<String>();
  wan.statusLED = wanDoc["statusLed"].as<String>();
  wan.priority = wanDoc["priority"].as<int>();
  wan.signalLevel = wanDoc["cellular"]["signalLevel"].as<int>();
  wan.networkType = wanDoc["cellular"]["network"].as<String>();

  if (wan.status == "Disabled")
    return wan;

  wan.ip = wanDoc["ip"].as<String>();
  wan.carrier = wanDoc["cellular"]["carrier"]["name"].as<String>();

  for (int key : wanDoc["cellular"]["sim"]["order"].as<JsonArray>())
  {
    String simKey = String(key);
    JsonObject sim = wanDoc["cellular"]["sim"][simKey.c_str()];

    PeplinkAPI_WAN_Cellular_SIM simCard;
    simCard.detected = sim["simCardDetected"].as<bool>();
    if (simCard.detected)
    {
      simCard.active = sim["active"].as<bool>();
      simCard.iccid = sim["iccid"].as<String>();
    }
    wan.simCards.push_back(simCard);
  }
  return wan;
}

PeplinkAPI_WAN_WiFi PeplinkRouter::_parseWiFiWAN(String& wanInfo)
{
  JsonDocument wanDoc;
  deserializeJson(wanDoc, wanInfo);

  PeplinkAPI_WAN_WiFi wan;

  wan.name = wanDoc["name"].as<String>();
  wan.type = PEPLINKAPI_WAN_TYPE_WIFI;
  wan.status = wanDoc["message"].as<String>();
  wan.statusLED = wanDoc["statusLed"].as<String>();
  wan.priority = wanDoc["priority"].as<int>();

  if (wan.status == "Disabled")
    return wan;

  wan.ip = wanDoc["ip"].as<String>();
  wan.strength = wanDoc["signal"]["strength"].as<int>();
  wan.ssid = wanDoc["ssid"].as<String>();
  wan.bssid = wanDoc["bssid"].as<String>();
  return wan;
}

bool PeplinkRouter::getInfo()
{
  String uri = "/api/status.system.info?accessToken=" + _token;

  JsonDocument recvDoc;

  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_GET, uri, NULL);
  if (!response.length())
    return false;

  Serial.println("getInfo() success");

  deserializeJson(recvDoc, response);

  // Extract the system info
  _info.name = recvDoc["response"]["device"]["name"].as<String>();
  _info.uptime = recvDoc["response"]["uptime"]["second"].as<long>();
  _info.serial = recvDoc["response"]["device"]["serialNumber"].as<String>();
  _info.fwVersion = recvDoc["response"]["device"]["firmwareVersion"].as<String>();
  _info.productCode = recvDoc["response"]["device"]["productCode"].as<String>();
  _info.hardwareRev = recvDoc["response"]["device"]["hardwareRevision"].as<String>();

  return true;
}

bool PeplinkRouter::getLocation()
{
  String uri = "/api/info.location?accessToken=" + _token;

  JsonDocument recvDoc;

  String response = _sendJsonRequest(PEPLINKAPI_HTTP_REQUEST_GET, uri, NULL);
  if (!response.length())
    return false;

  Serial.println("getLocation() success");

  deserializeJson(recvDoc, response);

  // Extract the location
  _location.latitude = recvDoc["response"]["location"]["latitude"].as<String>();
  _location.longitude = recvDoc["response"]["location"]["longitude"].as<String>();
  _location.altitude = recvDoc["response"]["location"]["altitude"].as<String>();
  return true;
}