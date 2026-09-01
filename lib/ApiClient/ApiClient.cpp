#include "ApiClient.h"
#include <WiFiClientSecure.h>

ApiClient::ApiClient() {
    _baseUrl = "";
    _token = "";
}

void ApiClient::begin(String baseUrl, String token) {
    _baseUrl = baseUrl;
    if (_baseUrl.endsWith("/")) {
        _baseUrl = _baseUrl.substring(0, _baseUrl.length() - 1);
    }
    _token = token;
}

bool ApiClient::sendSMS(SMSMessage sms) {
    if (WiFi.status() != WL_CONNECTED || _baseUrl.length() == 0) {
        Serial.println("[API]\nSend SMS failed: WiFi Disconnected / URL Belum Diatur");
        return false;
    }

    HTTPClient http;
    String endpoint = _baseUrl + "/api/sms";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setInsecure(); // Izinkan koneksi ke domain ngrok / custom web server
        if (!http.begin(secureClient, endpoint)) {
            Serial.println("[API] Gagal menginisialisasi HTTPS client.");
            return false;
        }
    } else {
        if (!http.begin(plainClient, endpoint)) {
            Serial.println("[API] Gagal menginisialisasi HTTP client.");
            return false;
        }
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _token);

    String escapedMsg = sms.message;
    escapedMsg.replace("\\", "\\\\");
    escapedMsg.replace("\"", "\\\"");
    escapedMsg.replace("\n", "\\n");
    escapedMsg.replace("\r", "\\r");
    escapedMsg.replace("\t", "\\t");

    String payload = "{";
    payload += "\"token\":\"" + _token + "\",";
    payload += "\"phone\":\"" + sms.phone + "\",";
    payload += "\"message\":\"" + escapedMsg + "\"";
    if (sms.datetime.length() > 0) {
        payload += ",\"received_at\":\"" + sms.datetime + "\"";
    }
    payload += "}";

    int httpCode = http.POST(payload);
    bool success = false;

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        Serial.printf("[API]\nHTTP %d (SMS Berhasil Diteruskan ke Laravel)\n", httpCode);
        success = true;
    } else {
        if (httpCode > 0) {
            Serial.printf("[API]\nHTTP Error %d: %s\n", httpCode, http.getString().c_str());
        } else {
            Serial.printf("[API]\nHTTP POST Connection Failed: %s\n", http.errorToString(httpCode).c_str());
        }
        success = false;
    }

    http.end();
    return success;
}

bool ApiClient::heartbeat(int signal, String operatorName, String simStatus, String regStatus, String &outPendingCommand) {
    outPendingCommand = "";

    if (WiFi.status() != WL_CONNECTED || _baseUrl.length() == 0) {
        return false;
    }

    HTTPClient http;
    String endpoint = _baseUrl + "/api/device/heartbeat";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setInsecure(); // Izinkan koneksi ke domain ngrok / custom web server
        if (!http.begin(secureClient, endpoint)) {
            return false;
        }
    } else {
        if (!http.begin(plainClient, endpoint)) {
            return false;
        }
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _token);

    String escapedOp = operatorName;
    escapedOp.replace("\"", "\\\"");

    String payload = "{";
    payload += "\"token\":\"" + _token + "\",";
    payload += "\"signal\":" + String(signal) + ",";
    payload += "\"operator\":\"" + escapedOp + "\",";
    payload += "\"sim_status\":\"" + simStatus + "\",";
    payload += "\"reg_status\":\"" + regStatus + "\"";
    payload += "}";

    int httpCode = http.POST(payload);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (success) {
        String respBody = http.getString();
        // Check if backend queued a pending AT command
        int cmdKey = respBody.indexOf("\"pending_command\":\"");
        if (cmdKey != -1) {
            int startIdx = cmdKey + 19;
            int endIdx = respBody.indexOf("\"", startIdx);
            if (endIdx != -1) {
                outPendingCommand = respBody.substring(startIdx, endIdx);
                // Unescape backslashes if any
                outPendingCommand.replace("\\\"", "\"");
                outPendingCommand.replace("\\\\", "\\");
            }
        }
        Serial.printf("[API] Heartbeat OK (HTTP %d)\n", httpCode);
    } else {
        if (httpCode > 0) {
            Serial.printf("[API] Heartbeat Error HTTP %d: %s\n", httpCode, http.getString().c_str());
        } else {
            Serial.printf("[API] Heartbeat Gagal: %s\n", http.errorToString(httpCode).c_str());
        }
    }

    http.end();
    return success;
}

bool ApiClient::sendATResponse(String command, String response) {
    if (WiFi.status() != WL_CONNECTED || _baseUrl.length() == 0) {
        return false;
    }

    HTTPClient http;
    String endpoint = _baseUrl + "/api/device/command-response";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setInsecure();
        if (!http.begin(secureClient, endpoint)) {
            return false;
        }
    } else {
        if (!http.begin(plainClient, endpoint)) {
            return false;
        }
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Device-Token", _token);

    // Escape special characters in command & response
    String escCmd = command;
    escCmd.replace("\\", "\\\\");
    escCmd.replace("\"", "\\\"");

    String escResp = response;
    escResp.replace("\\", "\\\\");
    escResp.replace("\"", "\\\"");
    escResp.replace("\n", "\\n");
    escResp.replace("\r", "\\r");
    escResp.replace("\t", "\\t");

    String payload = "{";
    payload += "\"token\":\"" + _token + "\",";
    payload += "\"command\":\"" + escCmd + "\",";
    payload += "\"response\":\"" + escResp + "\"";
    payload += "}";

    Serial.println("[API] Sending AT Command Execution Output to Laravel...");
    int httpCode = http.POST(payload);
    bool success = (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED);

    if (success) {
        Serial.printf("[API] AT Response Sent OK (HTTP %d)\n", httpCode);
    } else {
        Serial.printf("[API] Failed to send AT Response (HTTP %d)\n", httpCode);
    }

    http.end();
    return success;
}
