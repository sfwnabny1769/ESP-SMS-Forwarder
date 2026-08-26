#ifndef TELEGRAM_CLIENT_H
#define TELEGRAM_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../GSMManager/GSMManager.h"

class TelegramClient {
private:
    String _botToken;
    String _chatId;

    String escapeHtml(String str);
    String escapeJson(String str);

public:
    TelegramClient();

    void begin(String botToken, String chatId);
    bool sendSMS(SMSMessage sms);
    bool sendMessage(String htmlMessage);
    bool testConnection();
    
    bool isConfigured() const { return _botToken.length() > 0 && _chatId.length() > 0; }
    String getBotToken() const { return _botToken; }
    String getChatId() const { return _chatId; }
};

#endif // TELEGRAM_CLIENT_H