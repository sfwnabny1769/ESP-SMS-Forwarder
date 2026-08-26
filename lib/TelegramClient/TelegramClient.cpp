#include "TelegramClient.h"

TelegramClient::TelegramClient() {
    _botToken = "";
    _chatId = "";
}

void TelegramClient::begin(String botToken, String chatId) {
    _botToken = botToken;
    _chatId = chatId;
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

bool TelegramClient::sendMessage(String htmlMessage) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Telegram Error] Gagal kirim: WiFi tidak terhubung.");
        return false;
    }

    if (!isConfigured()) {
        Serial.println("[Telegram Error] Bot Token atau Chat ID kosong.");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure(); // Mengizinkan HTTPS tanpa memverifikasi fingerprint root CA

    HTTPClient https;
    String url = "https://api.telegram.org/bot" + _botToken + "/sendMessage";

    if (!https.begin(client, url)) {
        Serial.println("[Telegram Error] Gagal menginisialisasi koneksi HTTPS ke Telegram API.");
        return false;
    }

    https.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"chat_id\":\"" + _chatId + "\",";
    jsonPayload += "\"text\":\"" + escapeJson(htmlMessage) + "\",";
    jsonPayload += "\"parse_mode\":\"HTML\"";
    jsonPayload += "}";

    Serial.println("[Telegram] Mengirim notifikasi ke Telegram Cloud API...");
    int httpCode = https.POST(jsonPayload);
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("[Telegram] ✅ Pesan BERHASIL dikirim ke Telegram!");
        success = true;
    } else {
        if (httpCode > 0) {
            Serial.printf("[Telegram Error] HTTP %d: %s\n", httpCode, https.getString().c_str());
        } else {
            Serial.printf("[Telegram Error] Koneksi gagal: %s\n", https.errorToString(httpCode).c_str());
        }
        success = false;
    }

    https.end();
    return success;
}

bool TelegramClient::testConnection() {
    String testMsg = "🔔 <b>[ESP32 Standalone SMS Gateway]</b>\n\nTes koneksi langsung dari chip ESP32 ke Telegram berhasil!";
    return sendMessage(testMsg);
}
