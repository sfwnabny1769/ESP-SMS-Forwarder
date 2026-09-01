#ifndef TELEGRAM_CLIENT_H
#define TELEGRAM_CLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../GSMManager/GSMManager.h"
#include <vector>

//struktur data untuk menampung pesan dari telegram
struct TelegramIncomingMessage {
    String chatId;
    String senderName;
    String text;
    long updateId;
};

class TelegramClient {
private:
    String _botToken;
    String _chatId;
    long _lastUpdateId; // melacak offset update_id terakhir

    String escapeHtml(String str);
    String escapeJson(String str);

public:
    TelegramClient();

    void begin(String botToken, String chatId);
    bool sendSMS(SMSMessage sms);
    bool sendMessage(String htmlMessage);
    bool sendMessageTo(String targetChatId, String htmlMessage);
    bool testConnection();

    //fungsi polling untuk menerima pesan dari telegram
    std::vector<TelegramIncomingMessage> getNewMessages();
    
    bool isConfigured() const { return _botToken.length() > 0 && _chatId.length() > 0; }
    bool isAuthorized(String incomingChatId) const { return incomingChatId == _chatId; }
    String getBotToken() const { return _botToken; }
    String getChatId() const { return _chatId; }
};

#endif // TELEGRAM_CLIENT_H