#ifndef _PEPLINKAPI_H_
#define _PEPLINKAPI_H_

#include <vector>
#include <stdint.h>

#include <WiFi.h>
#include <Arduino.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "secrets.h"
#include "config.h"

/// @brief Stores router-assigned credentials
typedef struct
{
    uint32_t magic;         // Header used to verify the validity of a cookie jar retrieved from storage
    char cookie[128];       // Issued at login, and used to subsequentlt provide admin access without explicit login
    char token[128];        // Issued when a client token is granted and used for generic API access
} PeplinkAPI_CookieJar_t;

/// @brief HTTP request types supported by the router
typedef enum
{
    PEPLINKAPI_HTTP_REQUEST_GET, 
    PEPLINKAPI_HTTP_REQUEST_POST,
} PeplinkAPI_HTTPRequest_t;

/// @brief Access rights used when creating a client
typedef enum
{
    CLIENT_SCOPE_READ_ONLY,
    CLIENT_SCOPE_READ_WRITE
} PeplinkAPI_ClientScope_t;

/// @brief Client information
class PeplinkAPI_ClientInfo
{
public:
    String name;
    String id;
    String secret;
    String token;
    int tokenExpiry;    // Time in seconds before the token granted to the client expires
    PeplinkAPI_ClientScope_t scope;
};

/// @brief WAN types
typedef enum
{
    PEPLINKAPI_WAN_TYPE_ETHERNET,
    PEPLINKAPI_WAN_TYPE_CELLULAR,
    PEPLINKAPI_WAN_TYPE_WIFI,
} PeplinkAPI_WANType_t;

/// @brief Generic WAN class
class PeplinkAPI_WAN
{
public:
    String name;
    String ip;
    String status;
    String statusLED;
    long upload;        // Upload bandwidth
    long download;      // Download bandwidth
    String unit;        // Unit used for upload/download bandwidth
    int priority;
    PeplinkAPI_WANType_t type;
};

/// @brief SIM slot information for cellular wan
class PeplinkAPI_WAN_Cellular_SIM
{
public:
    bool detected;
    bool active;
    String iccid;
};

/// @brief Cellular WAN class
class PeplinkAPI_WAN_Cellular : public PeplinkAPI_WAN
{
public:
    String carrier;
    int rssi;
    int signalLevel;
    String networkType;

    std::vector<PeplinkAPI_WAN_Cellular_SIM> simCards;
};

/// @brief Ethernet WAN class
class PeplinkAPI_WAN_Ethernet : public PeplinkAPI_WAN{};

/// @brief Wi-Fi WAN class
class PeplinkAPI_WAN_WiFi : public PeplinkAPI_WAN
{
public:
    int strength;
    String ssid;
    String bssid;
};

/// @brief WAN bandwidth information
struct PeplinkAPI_WAN_Traffic
{
    String name;
    long download;
    long upload;
    String unit;
};

/// @brief Router device information
struct PeplinkRouterInfo
{
    String name;
    long uptime;
    String serial;
    String fwVersion;
    String productCode;
    String hardwareRev;
};

/// @brief Router location information
struct PeplinkRouterLocation
{
    String latitude;
    String longitude;
    String altitude;
};

/// @brief 
class PeplinkRouter
{

public:
    PeplinkRouter(){};
    PeplinkRouter(String ip, uint16_t port)
    {
        _ip = ip;
        _port = port;
    }

    /// @brief Ping the router IP address
    bool begin(){ return isAvailable(); }

    
    /// @brief Initialize the router object by testing the validity of cookes and accesss token and refreshing them as necessary
    /// @note The cookie and token values should be initialized from NVS (if available) prior to calling this function, avoding unnecessary login
    /// @param username Administrator username used when loging in
    /// @param password Administrator password used when loging in
    /// @param clientName Name of the client to create if there are no valid clients
    /// @param clientScope Access rights used to create a new client
    /// @param deleteExistingClients Whether or not to delete all existing clients registered on the router then create a new client
    /// @return Cookie, on success.
    /// @return Empty string on fail
    String begin(String username, String password, String clientName, PeplinkAPI_ClientScope_t clientScope, bool deleteExistingClients);
    
    /// @brief Check whether the IP address of the router is reachable via a ping request
    /// @note   Default ping timeout is 10s
    /// @return true on ping success
    /// @return false on ping fail
    bool isAvailable();


    bool update() { return (getClientList() && getWanStatus() && getInfo() && getLocation()); }
    
    /// @brief Retrieve the router's list of registered clients and update the local copy.
    /// @note  Any existing clients in the local list is cleared before the new list is retrieved
    bool getClientList();
    
    /// @brief Get the WAN connection status
    bool getWanStatus();
    
    /// @brief Get the bandwith of all WANs
    bool getWanTraffic();
    
    /// @brief Get device information from router
    bool getInfo();

    /// @brief Return the router device information
    PeplinkRouterInfo info() const { return _info; };

    /// @brief Get location information from router
    bool getLocation();

    /// @brief Return router location
    PeplinkRouterLocation location() const { return _location; };
    
    /// @brief Set the router IP address
    void setIP(String ip) { _ip = ip; };

    /// @brief Get the router IP address
    String ip() const { return _ip; };

    /// @brief Set the router port
    void setPort(uint16_t port) { _port = port; };

    /// @brief Get the router port
    uint16_t port() const { return _port; };

    /// @brief Set the router login cookie
    /// @note Call this function before begin() to use a cookie retrieved from NVS for admin access rather than logging in afresh
    void setCookie(String cookie) { _cookie = cookie; }

    /// @brief Get the router cookie
    String cookie() const { return _cookie; }
    
    /// @brief Set the router API access token
    /// @note Call this function before begin() to use an access token retrieved from NVS for API access
    void setToken(String token) { _token = token; }
    
    /// @brief Get the router API access token
    String token() const { return _token; }


    /// @brief Log in to router with a the administrator \a username & \a password
    /// @return Server-generated cookie if login is successful and access rights are confirmed
    /// @return Empty String on fail
    String login(const char *username, const char *password);

    std::vector<PeplinkAPI_ClientInfo> clients() const { return _clients; };
    size_t numClients() const { return _clients.size(); };
    std::vector<PeplinkAPI_WAN *> wanStatus() const { return _wan; };

private:
    
    /// @brief Send an HTTP request and handle any authentication errors before returning the response
    /// @param type HTTP request type. Either GET or POST
    /// @param endpoint URL of the API endpoint to send the request to
    /// @param body If sending a POST request, this parameter becomes the body content of the request
    /// @return Valid string on success
    /// @return Empty string on fail
    String _sendJsonRequest(PeplinkAPI_HTTPRequest_t type, String &endpoint, char *body);

    /// @brief Extract the information for ethernet-type WAN
    PeplinkAPI_WAN_Ethernet _parseEthernetWAN(String &wanInfo);

    /// @brief Extract the information for cellular-type WAN
    PeplinkAPI_WAN_Cellular _parseCellularWAN(String &wanInfo);

    /// @brief Extract the information for WiFi-type WAN
    PeplinkAPI_WAN_WiFi _parseWiFiWAN(String &wanInfo);

    /// @brief Create a clent with a given \a name and permissions.
    /// @note   Multiple clients can be created with the same name since the router will assing each one a unique ID 
    /// @return On success, id and secret populated
    /// @return On fail, empty PeplinkAPI_ClientInfo
    PeplinkAPI_ClientInfo _createClient(String name, PeplinkAPI_ClientScope_t scope);
    
    /// @brief Delete a specified client from the local list as well as from the router
    bool _deleteClient(PeplinkAPI_ClientInfo &client);
    
    /// @brief Delete all clients in the local list as well as from the router
    void _deleteExistingClients();
    
    /// @brief Request an access token for an existing client
    bool _grantClientToken(PeplinkAPI_ClientInfo &client);
    
    /// @brief  Refresh the access token used for non-admin API access.
    /// @note   By default, the access token used for API acccess is one granted in the most recent call to _grantClientToken()
    bool _refreshToken();

private:
    PeplinkRouterInfo _info;
    PeplinkRouterLocation _location;
    bool _available;
    String _ip;
    uint16_t _port;
    String _cookie;
    String _token;
    std::vector<PeplinkAPI_WAN *> _wan;
    std::vector<PeplinkAPI_WAN_Traffic> _wanTraffic;
    std::vector<PeplinkAPI_ClientInfo> _clients;
};

#endif