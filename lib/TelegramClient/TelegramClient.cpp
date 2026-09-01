#include "TelegramClient.h"

TelegramClient::TelegramClient() {
    _botToken = "";
    _chatId = "";
    _lastUpdateId = 0;
}

void TelegramClient::begin(String botToken, String chatId) {
    _botToken = botToken;
    _chatId = chatId;
    _lastUpdateId = 0;
    _botToken.trim();
    _chatId.trim();
}

String TelegramClient::escapeHtml(String str) {
    str.replace("&", "&amp;");
    str.replace("<", "&lt;");
    str.replace(">", "&gt;");
    str.replace("\"", "&quot;");
    return str;
}

String TelegramClient::escapeJson(String str) {
    str.replace("\\", "\\\\");
    str.replace("\"", "\\\"");
    str.replace("\n", "\\n");
    str.replace("\r", "\\r");
    str.replace("\t", "\\t");
    return str;
}

bool TelegramClient::sendSMS(SMSMessage sms) {
    if (!isConfigured()) {
        Serial.println("[Telegram Warning] Bot Token atau Chat ID belum diset.");
        return false;
    }

    String phone = escapeHtml(sms.phone);
    String body = escapeHtml(sms.message);
    String dt = sms.datetime.length() > 0 ? sms.datetime : "Baru saja";

    String text = "📩 <b>SMS BARU DITERIMA</b>\n";
    text += "━━━━━━━━━━━━━━━━━━━━\n";
    text += "📱 <b>Pengirim :</b> <code>" + phone + "</code>\n";
    text += "⏰ <b>Waktu    :</b> " + dt + "\n";
    text += "━━━━━━━━━━━━━━━━━━━━\n";
    text += "💬 <b>Isi Pesan:</b>\n";
    text += body;

    return sendMessage(text);
}

bool TelegramClient::sendMessageTo(String targetChatId, String htmlMessage) {
    if (WiFi.status() != WL_CONNECTED || _botToken.length() == 0 || targetChatId.length() == 0) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Mengizinkan HTTPS tanpa fingerprint CA statis

    HTTPClient https;
    https.setTimeout(5000);
    String url = "https://api.telegram.org/bot" + _botToken + "/sendMessage";

    if (!https.begin(client, url)) {
        Serial.println("[Telegram Error] Gagal menginisialisasi koneksi HTTPS ke Telegram API.");
        return false;
    }

    https.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"chat_id\":\"" + targetChatId + "\",";
    jsonPayload += "\"text\":\"" + escapeJson(htmlMessage) + "\",";
    jsonPayload += "\"parse_mode\":\"HTML\"";
    jsonPayload += "}";

    int httpCode = https.POST(jsonPayload);
    bool success = (httpCode == HTTP_CODE_OK);

    if (!success) {
        if (httpCode > 0) {
            Serial.printf("[Telegram Error] HTTP %d: %s\n", httpCode, https.getString().c_str());
        } else {
            Serial.printf("[Telegram Error] Koneksi gagal: %s\n", https.errorToString(httpCode).c_str());
        }
    }

    https.end();
    return success;
}

bool TelegramClient::sendMessage(String htmlMessage) {
    return sendMessageTo(_chatId, htmlMessage);
}

std::vector<TelegramIncomingMessage> TelegramClient::getNewMessages() {
    std::vector<TelegramIncomingMessage> messages;

    if (WiFi.status() != WL_CONNECTED || !isConfigured()) {
        return messages;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(4000); // 4 detik timeout non-blocking

    String url = "https://api.telegram.org/bot" + _botToken + "/getUpdates?limit=5";
    if (_lastUpdateId > 0) {
        url += "&offset=" + String(_lastUpdateId);
    }

    if (!https.begin(client, url)) {
        return messages;
    }

    int httpCode = https.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        
        // Parsing JSON respons Telegram secara efisien dan hemat memori
        int pos = 0;
        while ((pos = payload.indexOf("\"update_id\":", pos)) != -1) {
            int uIdStart = pos + 12;
            int uIdEnd = payload.indexOf(",", uIdStart);
            if (uIdEnd == -1) uIdEnd = payload.indexOf("}", uIdStart);
            long uId = payload.substring(uIdStart, uIdEnd).toInt();
            
            if (uId >= _lastUpdateId) {
                _lastUpdateId = uId + 1; // Tandai update_id sudah terbaca
            }

            TelegramIncomingMessage msg;
            msg.updateId = uId;

            // Cari Chat ID
            int chatPos = payload.indexOf("\"chat\":", pos);
            if (chatPos != -1) {
                int idPos = payload.indexOf("\"id\":", chatPos);
                if (idPos != -1) {
                    int idStart = idPos + 5;
                    int idEnd = payload.indexOf(",", idStart);
                    if (idEnd == -1) idEnd = payload.indexOf("}", idStart);
                    msg.chatId = payload.substring(idStart, idEnd);
                    msg.chatId.trim();
                }
            }

            // Cari First Name Pengirim
            int fromPos = payload.indexOf("\"first_name\":\"", pos);
            if (fromPos != -1) {
                int fnStart = fromPos + 14;
                int fnEnd = payload.indexOf("\"", fnStart);
                if (fnEnd != -1) {
                    msg.senderName = payload.substring(fnStart, fnEnd);
                }
            }

            // Cari Teks Pesan
            int textPos = payload.indexOf("\"text\":\"", pos);
            if (textPos != -1) {
                int txtStart = textPos + 8;
                int txtEnd = payload.indexOf("\"", txtStart);
                if (txtEnd != -1) {
                    String rawText = payload.substring(txtStart, txtEnd);
                    rawText.replace("\\n", "\n");
                    rawText.replace("\\/", "/");
                    rawText.replace("\\\"", "\"");
                    msg.text = rawText;
                }
            }

            if (msg.chatId.length() > 0 && msg.text.length() > 0) {
                messages.push_back(msg);
            }

            // Lanjut ke update_id berikutnya
            pos = payload.indexOf("\"update_id\":", uIdStart);
            if (pos == -1) break;
        }
    }

    https.end();
    return messages;
}

bool TelegramClient::testConnection() {
    String testMsg = "🔔 <b>[ESP32 Standalone SMS Gateway]</b>\n\nTes koneksi langsung dari chip ESP32 ke Telegram berhasil!";
    return sendMessage(testMsg);
}
