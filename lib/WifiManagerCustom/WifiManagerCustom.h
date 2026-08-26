#ifndef WIFI_MANAGER_CUSTOM_H
#define WIFI_MANAGER_CUSTOM_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"

class WifiManagerCustom {
private:
    String _ssid;
    String _password;
    String _telegramBotToken;
    String _telegramChatId;
    String _apiUrl;
    String _deviceToken;
    bool   _serverSyncEnabled;
    bool   _isConfigured;

    unsigned long _lastReconnectAttempt;
    unsigned long _buttonPressStart;
    bool          _inPortalMode;

    WebServer   _server;
    DNSServer   _dnsServer;
    Preferences _prefs;

    const unsigned long RECONNECT_INTERVAL = 10000; // Coba reconnect setiap 10 detik

    void loadFromPreferences();
    void saveToPreferences(String ssid, String pass, String tgToken, String tgChat, String apiUrl, String token, bool syncServer);
    void setupWebRoutes(int networkCount);
    String generatePortalHtml(int networkCount);
    String generateSuccessHtml(String message, bool restart);
    bool isIp(String str);
    String toStringIP(IPAddress ip);

public:
    WifiManagerCustom();

    void begin();
    void update();
    void startConfigPortal(bool autoTriggered = false);
    void resetConfig();

    bool isConnected();
    bool isPortalRunning() const { return _inPortalMode; }

    // Getters untuk Mode 1, Mode 2, dan Mode 3                                                                                     
    String getSSID() const { return _ssid; }                                                                                        
    String getTelegramToken() const { return _telegramBotToken; }                                                                   
    String getTelegramChatId() const { return _telegramChatId; }                                                                    
    String getApiUrl() const { return _apiUrl; }                                                                                    
    String getDeviceToken() const { return _deviceToken; }                                                                          
    bool isServerSyncEnabled() const { return _serverSyncEnabled; }                                                                 
                                                                                                                                    
    String getIP() const { return WiFi.localIP().toString(); }                                                                      
    String getAPIP() const { return WiFi.softAPIP().toString(); }                                                                   
    String getMacAddress() const { return WiFi.macAddress(); }  
};

#endif // WIFI_MANAGER_CUSTOM_H
