#include "ApiClient.h"
#include <WiFiClientSecure.h>

// Definisi protokol ALPN untuk kompatibilitas Cloud Edge Proxy (Ngrok, Cloudflare, AWS)
static const char *ALPN_HTTP11[] = {"http/1.1", NULL};

// Root CA Sertifikat untuk Server Backend HTTPS (Let's Encrypt ISRG Root X1 / Cloudflare / Ngrok - CWE-295 Remediation)
const char BACKEND_ROOT_CA[] PROGMEM = 
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJkgZu454U/0SiIGB\n"
"hnpPKuUrHYNWJxRtb6K9rtwSrIk8EvmUQDxW00ZybOPTOVVcCqzWCLcmjPymooxS\n"
"LoP/HYqETTRSZX9q2DZttlfdUvc5guHdKWapMTIxmWYZOiPdUrAxCLRNnDKolHum\n"
"DLWCkh3qhBoQ1zrvmmQUilMc199Ja90KWmhZNw4oWQG26Nvd52nNm++3uH455wqB\n"
"6uDqnJ1G0EBmNLFV9SLuw05mrDAgwglBmZvRpy9qY8LJOFxz80gdrSDBZ456350e\n"
"p50ei54owUrKGhKmndHs6/fZjb9qhrEHc+MaPJhaCEFzbU23642M75xruragrIhS\n"
"Go12okN8QL6XYKAAnEp099rn94OMEB63da74kVHyGe7GpyRxg8Gtymv06irxEvZo\n"
"9f14okF8Rtwved6h05nxSuusPv37bUhrDTGD34FGViBexYT2b59ucUC618/U5GSf\n"
"I2eyU38Y8+Mb87etDYdWAmQUnHPR7aVCrefhqPUEGpesW996ElFefZZRHzzkkoU1\n"
"epBLWN7Y+/dEo5qZN7tY5jU0566DLyctrwoko4mFbHP/kgUCreAg5N01lRi90bW4\n"
"7py3514oTVnRwfQQVNd9G6VLh282N5GXSK+vk8W4JSkZNKe3KUUCikrrv0Q3Xsk6\n"
"4SGUPqk73bRovVVZaq44EB4eFYq7YLncnxIwXO4QVCBr5IYELHr3KhwpFu8DLkhN\n"
"tot73b5H7JDsrLwyKs9eFEqMqmEWL6EBPnKuHYCPEd369LnITcCIwpLmnGh50S2k\n"
"mUhOvYYaefBIQUqc9BDxoqkhptwcGcnokDAT6IJYPeOGnKGup5aaNKAvm83auBoQ\n"
"1xJU7UTWh/2lg0PPU/Ub9y8VDZNvxC9qwyTxfi42whNWmWmt9S2962unRUh2HKnl\n"
"GKUIoYGRaOxVDZgneKGPNqYi2bMQn56/+BR6wQNAGqvAivrkECngxCoVUzkCVjul\n"
"jrPcWnzzsur+pDnMtW5NKA==\n"
"-----END CERTIFICATE-----\n";

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
    http.setTimeout(8000);
    String endpoint = _baseUrl + "/api/sms";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setCACert(BACKEND_ROOT_CA); // Verifikasi sertifikat TLS server backend (CWE-295 Remediation)
        secureClient.setAlpnProtocols(ALPN_HTTP11); // Negosiasi protokol HTTP/1.1 untuk Cloud Edge Proxy
        secureClient.setHandshakeTimeout(10);
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
    http.addHeader("ngrok-skip-browser-warning", "true");
    http.addHeader("User-Agent", "ESP32-SMS-Gateway");
    http.addHeader("Connection", "close");

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
    http.setTimeout(8000);
    String endpoint = _baseUrl + "/api/device/heartbeat";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setCACert(BACKEND_ROOT_CA); // Verifikasi sertifikat TLS server backend (CWE-295 Remediation)
        secureClient.setAlpnProtocols(ALPN_HTTP11); // Negosiasi protokol HTTP/1.1 untuk Cloud Edge Proxy
        secureClient.setHandshakeTimeout(10);
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
    http.addHeader("ngrok-skip-browser-warning", "true");
    http.addHeader("User-Agent", "ESP32-SMS-Gateway");
    http.addHeader("Connection", "close");

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
    http.setTimeout(8000);
    String endpoint = _baseUrl + "/api/device/command-response";

    bool isHttps = endpoint.startsWith("https://");
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    if (isHttps) {
        secureClient.setCACert(BACKEND_ROOT_CA); // Verifikasi sertifikat TLS server backend (CWE-295 Remediation)
        secureClient.setAlpnProtocols(ALPN_HTTP11);
        secureClient.setHandshakeTimeout(10);
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
    http.addHeader("ngrok-skip-browser-warning", "true");
    http.addHeader("User-Agent", "ESP32-SMS-Gateway");
    http.addHeader("Connection", "close");

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
