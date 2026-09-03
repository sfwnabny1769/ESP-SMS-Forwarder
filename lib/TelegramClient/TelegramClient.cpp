#include "TelegramClient.h"

// Go Daddy Root & Intermediate CA Certificates untuk api.telegram.org (CWE-295 Remediation)
const char TELEGRAM_ROOT_CA[] PROGMEM = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDxTCCAq2gAwIBAgIBADANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
"EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
"ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTA5MDkwMTAwMDAwMFoXDTM3MTIzMTIz\n"
"NTk1OVowgYMxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
"EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjExMC8GA1UE\n"
"AxMoR28gRGFkZHkgUm9vdCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHkgLSBHMjCCASIw\n"
"DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAL9xYgjx+lk09xvJGKP3gElY6SKD\n"
"E6bFIEMBO4Tx5oVJnyfq9oQbTqC023CYxzIBsQU+B07u9PpPL1kwIuerGVZr4oAH\n"
"/PMWdYA5UXvl+TW2dE6pjYIT5LY/qQOD+qK+ihVqf94Lw7YZFAXK6sOoBJQ7Rnwy\n"
"DfMAZiLIjWltNowRGLfTshxgtDj6AozO091GB94KPutdfMh8+7ArU6SSYmlRJQVh\n"
"GkSBjCypQ5Yj36w6gZoOKcUcqeldHraenjAKOc7xiID7S13MMuyFYkMlNAJWJwGR\n"
"tDtwKj9useiciAF9n9T521NtYJ2/LOdYq7hfRvzOxBsDPAnrSTFcaUaz4EcCAwEA\n"
"AaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYwHQYDVR0OBBYE\n"
"FDqahQcQZyi27/a9BUFuIMGU2g/eMA0GCSqGSIb3DQEBCwUAA4IBAQCZ21151fmX\n"
"WWcDYfF+OwYxdS2hII5PZYe096acvNjpL9DbWu7PdIxztDhC2gV7+AJ1uP2lsdeu\n"
"9tfeE8tTEH6KRtGX+rcuKxGrkLAngPnon1rpN5+r5N9ss4UXnT3ZJE95kTXWXwTr\n"
"gIOrmgIttRD02JDHBHNA7XIloKmf7J6raBKZV8aPEjoJpL1E/QYVN8Gb5DKj7Tjo\n"
"2GTzLH4U/ALqn83/B2gX2yKQOC16jdFU8WnjXzPKej17CuPKf1855eJ1usV2GDPO\n"
"LPAvTK33sefOT6jEm0pUBsV/fdUID+Ic/n4XuKxe9tQWskMJDE32p2u0mYRlynqI\n"
"4uJEvlz36hz1\n"
"-----END CERTIFICATE-----\n"
"-----BEGIN CERTIFICATE-----\n"
"MIIE0DCCA7igAwIBAgIBBzANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
"EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
"EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
"ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTExMDUwMzA3MDAwMFoXDTMxMDUwMzA3\n"
"MDAwMFowgbQxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
"EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjEtMCsGA1UE\n"
"CxMkaHR0cDovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkvMTMwMQYDVQQD\n"
"EypHbyBEYWRkeSBTZWN1cmUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n"
"MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC54MsQ1K92vdSTYuswZLiBCGzD\n"
"BNliF44v/z5lz4/OYuY8UhzaFkVLVat4a2ODYpDOD2lsmcgaFItMzEUz6ojcnqOv\n"
"K/6AYZ15V8TPLvQ/MDxdR/yaFrzDN5ZBUY4RS1T4KL7QjL7wMDge87Am+GZHY23e\n"
"cSZHjzhHU9FGHbTj3ADqRay9vHHZqm8A29vNMDp5T19MR/gd71vCxJ1gO7GyQ5HY\n"
"pDNO6rPWJ0+tJYqlxvTV0KaudAVkV4i1RFXULSo6Pvi4vekyCgKUZMQWOlDxSq7n\n"
"eTOvDCAHf+jfBDnCaQJsY1L6d8EbyHSHyLmTGFBUNUtpTrw700kuH9zB0lL7AgMB\n"
"AAGjggEaMIIBFjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNV\n"
"HQ4EFgQUQMK9J47MNIMwojPX+2yz8LQsgM4wHwYDVR0jBBgwFoAUOpqFBxBnKLbv\n"
"9r0FQW4gwZTaD94wNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8v\n"
"b2NzcC5nb2RhZGR5LmNvbS8wNQYDVR0fBC4wLDAqoCigJoYkaHR0cDovL2NybC5n\n"
"b2RhZGR5LmNvbS9nZHJvb3QtZzIuY3JsMEYGA1UdIAQ/MD0wOwYEVR0gADAzMDEG\n"
"CCsGAQUFBwIBFiVodHRwczovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkv\n"
"MA0GCSqGSIb3DQEBCwUAA4IBAQAIfmyTEMg4uJapkEv/oV9PBO9sPpyIBslQj6Zz\n"
"91cxG7685C/b+LrTW+C05+Z5Yg4MotdqY3MxtfWoSKQ7CC2iXZDXtHwlTxFWMMS2\n"
"RJ17LJ3lXubvDGGqv+QqG+6EnriDfcFDzkSnE3ANkR/0yBOtg2DZ2HKocyQetawi\n"
"DsoXiWJYRBuriSUBAA/NxBti21G00w9RKpv0vHP8ds42pM3Z2Czqrpv1KrKQ0U11\n"
"GIo/ikGQI31bS/6kA1ibRrLDYGCD+H1QQc7CoZDDu+8CL9IVVO5EFdkKrqeKM+2x\n"
"LXY2JtwE65/3YR8V3Idv7kaWKK2hJn0KCacuBKONvPi8BDAB\n"
"-----END CERTIFICATE-----\n";

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
    client.setCACert(TELEGRAM_ROOT_CA); // Verifikasi sertifikat TLS Root & Intermediate CA resmi (CWE-295 Remediation)

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
    client.setCACert(TELEGRAM_ROOT_CA); // Verifikasi sertifikat TLS Root & Intermediate CA resmi (CWE-295 Remediation)

    HTTPClient https;
    https.setTimeout(25000); // 25 detik socket timeout untuk long-polling pasif

    String url = "https://api.telegram.org/bot" + _botToken + "/getUpdates?limit=5&timeout=20";
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
