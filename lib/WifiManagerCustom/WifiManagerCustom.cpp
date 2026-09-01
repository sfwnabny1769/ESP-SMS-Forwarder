#include "WifiManagerCustom.h"
#include "../TelegramClient/TelegramClient.h"
#include <esp_wifi.h>

WifiManagerCustom::WifiManagerCustom() 
    : _server(80), _inPortalMode(false), _buttonPressStart(0), _lastReconnectAttempt(0), _isConfigured(false) {
    _ssid = "";
    _password = "";
    _telegramBotToken = "";
    _telegramChatId = "";
    _apiUrl = "";
    _deviceToken = "";
    _serverSyncEnabled = true;
}

void WifiManagerCustom::loadFromPreferences() {
    _prefs.begin("sms_gw", true); // Open in read-only mode
    _ssid = _prefs.getString("ssid", DEFAULT_WIFI_SSID);
    _password = _prefs.getString("pass", DEFAULT_WIFI_PASSWORD);
    _telegramBotToken = _prefs.getString("tg_token", DEFAULT_TELEGRAM_BOT_TOKEN);
    _telegramChatId = _prefs.getString("tg_chat", DEFAULT_TELEGRAM_CHAT_ID);
    _apiUrl = _prefs.getString("api_url", DEFAULT_API_URL);
    _deviceToken = _prefs.getString("token", DEFAULT_DEVICE_TOKEN);
    _serverSyncEnabled = _prefs.getBool("sync_srv", DEFAULT_SERVER_SYNC_ENABLED);
    _isConfigured = _prefs.getBool("configured", false);
    _prefs.end();

    if (_apiUrl.endsWith("/")) {
        _apiUrl = _apiUrl.substring(0, _apiUrl.length() - 1);
    }
}

void WifiManagerCustom::saveToPreferences(String ssid, String pass, String tgToken, String tgChat, String apiUrl, String token, bool syncServer) {
    if (apiUrl.endsWith("/")) {
        apiUrl = apiUrl.substring(0, apiUrl.length() - 1);
    }

    _prefs.begin("sms_gw", false); // Open in read-write mode
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass);
    _prefs.putString("tg_token", tgToken);
    _prefs.putString("tg_chat", tgChat);
    _prefs.putString("api_url", apiUrl);
    _prefs.putString("token", token);
    _prefs.putBool("sync_srv", syncServer);
    _prefs.putBool("configured", true);
    _prefs.end();

    _ssid = ssid;
    _password = pass;
    _telegramBotToken = tgToken;
    _telegramChatId = tgChat;
    _apiUrl = apiUrl;
    _deviceToken = token;
    _serverSyncEnabled = syncServer;
    _isConfigured = true;
}

void WifiManagerCustom::resetConfig() {
    _prefs.begin("sms_gw", false);
    _prefs.clear();
    _prefs.end();
    Serial.println("[NVS] Konfigurasi berhasil dihapus (Factory Reset).");
}

void WifiManagerCustom::begin() {
    pinMode(SETUP_TRIGGER_PIN, INPUT_PULLUP);

    loadFromPreferences();

    Serial.println("\n[WiFi Manager] Membaca konfigurasi dari memori Flash (NVS)...");
    Serial.printf(" - SSID Target    : %s\n", _ssid.c_str());
    Serial.printf(" - Telegram Config: %s\n", _telegramBotToken.length() > 0 ? "Terkonfigurasi" : "Belum diisi");
    Serial.printf(" - Server API URL : %s (Sync: %s)\n", _apiUrl.c_str(), _serverSyncEnabled ? "Aktif" : "Non-aktif");
    Serial.printf(" - Device Token   : %s\n", _deviceToken.c_str());

    // Jika SSID kosong, langsung jalankan mode portal konfigurasi
    if (_ssid.length() == 0) {
        Serial.println("[WiFi Manager] Belum ada konfigurasi WiFi. Membuka Portal Konfigurasi...");
        startConfigPortal(true);
        return;
    }

    // Coba koneksi ke WiFi dalam mode STA
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    Serial.print("[WiFi Manager] Menghubungkan ke ");
    Serial.print(_ssid);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < WIFI_CONNECT_TIMEOUT)) {
        delay(500);
        Serial.print(".");
        
        // Deteksi jika tombol BOOT ditekan saat proses koneksi
        if (digitalRead(SETUP_TRIGGER_PIN) == LOW) {
            Serial.println("\n[WiFi Manager] Tombol BOOT ditekan. Beralih ke Web Portal...");
            startConfigPortal(false);
            return;
        }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi Manager] ✅ Terhubung ke WiFi!");
        Serial.print(" - IP Address : ");
        Serial.println(WiFi.localIP());
        Serial.print(" - Gateway IP  : ");
        Serial.println(WiFi.gatewayIP());
        Serial.print(" - RSSI Sinyal : ");
        Serial.printf("%d dBm\n", WiFi.RSSI());
    } else {
        Serial.println("\n[WiFi Manager] ⚠️ Gagal terhubung ke WiFi dalam 15 detik.");
        Serial.println("[WiFi Manager] Membuka Web Portal secara otomatis agar dapat dikonfigurasi ulang...");
        startConfigPortal(true);
    }
}

void WifiManagerCustom::update() {
    // 1. Jika dalam portal mode, layani request DNS dan Web
    if (_inPortalMode) {
        _dnsServer.processNextRequest();
        _server.handleClient();
        //return; (Jangan gunakan return; agar loop() di main.cpp tetap bisa lanjut memproses GSM) 
    }

    // 2. Deteksi tombol BOOT (Setup Trigger) ditekan selama 3 detik
    if (digitalRead(SETUP_TRIGGER_PIN) == LOW) {
        if (_buttonPressStart == 0) {
            _buttonPressStart = millis();
        } else if (millis() - _buttonPressStart >= 3000) {
            Serial.println("\n[WiFi Manager] 🔘 Tombol BOOT ditahan 3 detik! Membuka Portal Konfigurasi...");
            _buttonPressStart = 0;
            startConfigPortal(false);
            return;
        }
    } else {
        _buttonPressStart = 0;
    }

    // 3. Auto-reconnect non-blocking jika koneksi WiFi terputus
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long currentMillis = millis();
        if (currentMillis - _lastReconnectAttempt >= RECONNECT_INTERVAL) {
            _lastReconnectAttempt = currentMillis;
            Serial.println("[WiFi Manager] Koneksi WiFi terputus. Mencoba menghubungkan kembali...");
            WiFi.disconnect();
            WiFi.begin(_ssid.c_str(), _password.c_str());
        }
    }
}

bool WifiManagerCustom::isConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

void WifiManagerCustom::startConfigPortal(bool autoTriggered) {
    _inPortalMode = true;

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA); // Mode AP_STA agar tetap bisa memindai jaringan WiFi sekitar

    Serial.println("[Portal] Memindai jaringan WiFi sekitar...");
    int networkCount = WiFi.scanNetworks();
    Serial.printf("[Portal] Ditemukan %d jaringan WiFi.\n", networkCount);

    // Generate Random 4-Digit Suffix untuk SSID Sekali Pakai (No-Cache Conflict pada HP)
    uint32_t randSsidSuffix = (esp_random() % 9000) + 1000;
    _portalSSID = "ESP-SMS-" + String(randSsidSuffix);

    // Generate Dynamic 8-Digit Random PIN Password via hardware esp_random
    char passBuf[9];
    uint32_t randNum = (esp_random() % 90000000) + 10000000;
    snprintf(passBuf, sizeof(passBuf), "%08u", randNum);
    _portalPassword = String(passBuf);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress netMsk(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, netMsk);
    WiFi.softAP(_portalSSID.c_str(), _portalPassword.c_str());

    Serial.println("\n=======================================================");
    Serial.println("         🔥 WEB CONFIGURATION PORTAL AKTIF 🔥         ");
    Serial.println("=======================================================");
    Serial.printf(" 1. Hubungkan HP / Laptop ke WiFi : %s\n", _portalSSID.c_str());
    Serial.printf(" 2. Masukkan Password WiFi       : %s\n", _portalPassword.c_str());
    Serial.println(" 3. Buka browser dan ketik alamat : http://192.168.4.1");
    Serial.println(" 4. Atur WiFi, Telegram Bot, dan Server Laravel");
    Serial.println("=======================================================\n");

    _dnsServer.start(53, "*", apIP);
    setupWebRoutes(networkCount);
    _server.begin();

    Serial.println("[Portal] Web Server & DNS berhasil dijalankan (Non-blocking)."); 
}

void WifiManagerCustom::setupWebRoutes(int networkCount) {
    // Route Halaman Utama Portal
    _server.on("/", HTTP_GET, [this, networkCount]() {
        _server.send(200, "text/html", generatePortalHtml(networkCount));
    });

    // Handler Simpan Konfigurasi (POST /save)
    _server.on("/save", HTTP_POST, [this]() {
        String newSsid    = _server.arg("ssid");
        String newPass    = _server.arg("password");
        String newTgToken = _server.arg("tg_token");
        String newTgChat  = _server.arg("tg_chat");
        String newUrl     = _server.arg("api_url");
        String newToken   = _server.arg("token");
        bool   syncServer = _server.hasArg("sync_srv");

        newSsid.trim();
        newPass.trim();
        newTgToken.trim();
        newTgChat.trim();
        newUrl.trim();
        newToken.trim();

        if (newSsid.length() == 0) {
            String errHtml = generateSuccessHtml("Gagal! SSID WiFi tidak boleh kosong.", false);
            _server.send(400, "text/html", errHtml);
            return;
        }

        saveToPreferences(newSsid, newPass, newTgToken, newTgChat, newUrl, newToken, syncServer);

        Serial.println("\n[Portal] Konfigurasi Baru Berhasil Disimpan:");
        Serial.printf(" - SSID        : %s\n", newSsid.c_str());
        Serial.printf(" - Telegram    : Token (%s) | Chat (%s)\n", newTgToken.length() > 0 ? "Set" : "Empty", newTgChat.c_str());
        Serial.printf(" - Server Sync : %s (%s)\n", syncServer ? "Aktif" : "Non-aktif", newUrl.c_str());
        Serial.println("[Portal] Memulai restart ESP32 dalam 2 detik...");

        String succHtml = generateSuccessHtml("Konfigurasi Berhasil Disimpan! ESP32 akan restart dan terhubung...", true);
        _server.send(200, "text/html", succHtml);

        delay(1800);
        ESP.restart();
    });

    // Handler Test Kirim Telegram Langsung (AJAX POST /test-telegram)
    _server.on("/test-telegram", HTTP_POST, [this]() {
        String token = _server.arg("token");
        String chat  = _server.arg("chat");
        token.trim();
        chat.trim();

        if (token.length() == 0 || chat.length() == 0) {
            _server.send(400, "application/json", "{\"success\":false,\"message\":\"Token dan Chat ID wajib diisi.\"}");
            return;
        }

        TelegramClient testClient;
        testClient.begin(token, chat);
        bool sent = testClient.sendMessage("🔔 <b>[ESP32 SMS Gateway Portal]</b>\n\nTes notifikasi langsung dari Web Portal ESP32 berhasil!");

        if (sent) {
            _server.send(200, "application/json", "{\"success\":true,\"message\":\"Pesan tes berhasil terkirim ke Telegram Anda!\"}");
        } else {
            _server.send(500, "application/json", "{\"success\":false,\"message\":\"Gagal mengirim pesan. Pastikan bot sudah di-Start dan internet terhubung.\"}");
        }
    });

    // Handler Reset ke Pengaturan Awal (POST /reset)
    _server.on("/reset", HTTP_POST, [this]() {
        resetConfig();
        String resp = generateSuccessHtml("Perangkat berhasil di-reset ke pengaturan default. Restarting...", true);
        _server.send(200, "text/html", resp);
        delay(1800);
        ESP.restart();
    });

    // Captive Portal Redirection Routes
    _server.on("/generate_204", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });
    _server.on("/gen_204", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });
    _server.on("/hotspot-detect.html", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });
    _server.on("/canonical.html", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });
    _server.on("/ncsi.txt", HTTP_GET, [this]() { _server.send(200, "text/plain", "Microsoft NCSI"); });
    _server.on("/connecttest.txt", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });
    _server.on("/redirect", HTTP_GET, [this, networkCount]() { _server.send(200, "text/html", generatePortalHtml(networkCount)); });

    _server.onNotFound([this]() {
        _server.sendHeader("Location", "http://192.168.4.1/", true);
        _server.send(302, "text/plain", "");
    });
}

String WifiManagerCustom::generatePortalHtml(int networkCount) {
    String mac = WiFi.macAddress();
    
    String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>";
    html += "<title>SMS Gateway Setup</title>";
    html += "<style>";
    html += "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;}";
    html += "body{background:#0b1120;color:#f1f5f9;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:16px;}";
    html += ".card{background:#1e293b;border:1px solid #334155;border-radius:16px;max-width:460px;width:100%;padding:24px;box-shadow:0 20px 25px -5px rgba(0,0,0,0.5);}";
    html += ".header{text-align:center;margin-bottom:18px;}";
    html += ".header h1{font-size:20px;font-weight:700;color:#38bdf8;margin-bottom:4px;}";
    html += ".header p{font-size:13px;color:#94a3b8;}";
    html += ".badge-box{display:flex;justify-content:center;gap:8px;margin-top:10px;flex-wrap:wrap;}";
    html += ".badge{background:#0f172a;border:1px solid #334155;color:#38bdf8;font-size:11px;font-weight:600;padding:4px 10px;border-radius:999px;}";
    html += ".section-title{font-size:13px;font-weight:700;color:#38bdf8;margin:18px 0 10px 0;display:flex;align-items:center;gap:6px;}";
    html += ".form-group{margin-bottom:14px;}";
    html += "label{display:block;font-size:12px;font-weight:600;color:#cbd5e1;margin-bottom:6px;text-transform:uppercase;letter-spacing:0.5px;}";
    html += "input,select{width:100%;padding:10px 14px;background:#0f172a;border:1px solid #334155;border-radius:8px;color:#fff;font-size:14px;outline:none;transition:border-color .2s;}";
    html += "input:focus,select:focus{border-color:#38bdf8;}";
    html += ".input-desc{font-size:11px;color:#64748b;margin-top:4px;line-height:1.4;}";
    html += ".pw-wrapper{position:relative;}";
    html += ".pw-toggle{position:absolute;right:10px;top:50%;transform:translateY(-50%);background:none;border:none;color:#94a3b8;cursor:pointer;font-size:12px;padding:4px;}";
    html += ".btn{width:100%;padding:12px;border:none;border-radius:8px;font-size:14px;font-weight:600;cursor:pointer;transition:background .2s;display:flex;justify-content:center;align-items:center;gap:8px;}";
    html += ".btn-primary{background:#0284c7;color:#fff;margin-top:12px;}";
    html += ".btn-primary:hover{background:#0369a1;}";
    html += ".btn-outline{background:transparent;border:1px solid #0284c7;color:#38bdf8;padding:8px 12px;font-size:12px;margin-top:6px;}";
    html += ".btn-outline:hover{background:rgba(2,132,199,0.15);}";
    html += ".btn-danger{background:transparent;border:1px solid #ef4444;color:#ef4444;margin-top:12px;font-size:12px;padding:8px;}";
    html += ".btn-danger:hover{background:rgba(239,68,68,0.1);}";
    html += ".divider{height:1px;background:#334155;margin:16px 0;}";
    html += ".checkbox-group{display:flex;align-items:center;gap:8px;margin-bottom:12px;}";
    html += ".checkbox-group input{width:auto;cursor:pointer;}";
    html += ".checkbox-group label{margin-bottom:0;text-transform:none;cursor:pointer;}";
    html += "#testStatus{font-size:12px;padding:6px 10px;border-radius:6px;margin-top:6px;display:none;}";
    html += "</style></head><body>";

    html += "<div class='card'>";
    html += "<div class='header'>";
    html += "<h1>📡 SMS Gateway Setup</h1>";
    html += "<p>Mode 1 (Standalone), Mode 2 (Local), Mode 3 (Sync)</p>";
    html += "<div class='badge-box'>";
    html += "<span class='badge'>ESP32-S3</span>";
    html += "<span class='badge'>MAC: " + mac + "</span>";
    html += "</div>";
    html += "</div>";

    html += "<form method='POST' action='/save'>";
    
    // SECTION 1: WiFi
    html += "<div class='section-title'>1️⃣ PENGATURAN WIFI MODEM</div>";
    html += "<div class='form-group'>";
    html += "<label for='wifi_select'>Pilih Jaringan WiFi</label>";
    html += "<select id='wifi_select' onchange='selectSSID(this.value)'>";
    html += "<option value=''>-- Pilih SSID dari daftar pemindaian --</option>";
    for (int i = 0; i < networkCount; ++i) {
        String ssidItem = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        int quality = (rssi <= -100) ? 0 : (rssi >= -50 ? 100 : 2 * (rssi + 100));
        String lock = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓" : "🔒";
        String selected = (ssidItem == _ssid) ? " selected" : "";
        html += "<option value='" + ssidItem + "'" + selected + ">" + lock + " " + ssidItem + " (" + String(quality) + "%)</option>";
    }
    html += "<option value='__MANUAL__'>[+ Masukkan SSID Manual / Tersembunyi]</option>";
    html += "</select>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label for='ssid'>Nama SSID WiFi</label>";
    html += "<input type='text' id='ssid' name='ssid' value='" + _ssid + "' placeholder='Contoh: MyModem_WiFi' required>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label for='password'>Password WiFi</label>";
    html += "<div class='pw-wrapper'>";
    html += "<input type='password' id='password' name='password' value='" + _password + "' placeholder='Kosongkan jika Open WiFi'>";
    html += "<button type='button' class='pw-toggle' onclick='togglePw()'>👁️</button>";
    html += "</div>";
    html += "</div>";

    html += "<div class='divider'></div>";

    // SECTION 2: Mode 1 Telegram Standalone
    html += "<div class='section-title'>2️⃣ MODE 1: TELEGRAM BOT (STANDALONE)</div>";
    html += "<div class='form-group'>";
    html += "<label for='tg_token'>Telegram Bot Token</label>";
    html += "<input type='text' id='tg_token' name='tg_token' value='" + _telegramBotToken + "' placeholder='Contoh: 7123456789:AAHfk...'>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label for='tg_chat'>Telegram Chat ID</label>";
    html += "<input type='text' id='tg_chat' name='tg_chat' value='" + _telegramChatId + "' placeholder='Contoh: 123456789 atau -100...'>";
    html += "<button type='button' class='btn btn-outline' onclick='testTelegram()'>🔔 Test Kirim Notifikasi Telegram</button>";
    html += "<div id='testStatus'></div>";
    html += "</div>";

    html += "<div class='divider'></div>";

    // SECTION 3: Mode 3 Server Sync
    html += "<div class='section-title'>3️⃣ MODE 3: SINKRONISASI SERVER (OPSIONAL)</div>";
    html += "<div class='checkbox-group'>";
    html += "<input type='checkbox' id='sync_srv' name='sync_srv' value='1'" + String(_serverSyncEnabled ? " checked" : "") + ">";
    html += "<label for='sync_srv'>Aktifkan sinkronisasi SMS ke Laravel Server</label>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label for='api_url'>URL Server API Laravel</label>";
    html += "<input type='text' id='api_url' name='api_url' value='" + _apiUrl + "' placeholder='http://192.168.1.50:8000 atau Cloudflare URL'>";
    html += "</div>";

    html += "<div class='form-group'>";
    html += "<label for='token'>Device Token Laravel</label>";
    html += "<input type='text' id='token' name='token' value='" + _deviceToken + "' placeholder='ESP32_SECRET_TOKEN...'>";
    html += "</div>";

    html += "<button type='submit' class='btn btn-primary' id='btnSubmit'>💾 Simpan Pengaturan & Restart</button>";
    html += "</form>";

    html += "<form method='POST' action='/reset' onsubmit='return confirm(\"Reset semua data konfigurasi di ESP32?\");'>";
    html += "<button type='submit' class='btn btn-danger'>⚠️ Reset ke Pengaturan Awal</button>";
    html += "</form>";

    html += "</div>";

    // Frontend JS
    html += "<script>";
    html += "function selectSSID(val){";
    html += "  var input = document.getElementById('ssid');";
    html += "  if(val && val !== '__MANUAL__'){ input.value = val; document.getElementById('password').focus(); }";
    html += "  else if(val === '__MANUAL__'){ input.value = ''; input.focus(); }";
    html += "}";
    html += "function togglePw(){";
    html += "  var pw = document.getElementById('password');";
    html += "  pw.type = (pw.type === 'password') ? 'text' : 'password';";
    html += "}";
    html += "function testTelegram(){";
    html += "  var token = document.getElementById('tg_token').value.trim();";
    html += "  var chat = document.getElementById('tg_chat').value.trim();";
    html += "  var statusDiv = document.getElementById('testStatus');";
    html += "  if(!token || !chat){ alert('Isi Token dan Chat ID Telegram terlebih dahulu!'); return; }";
    html += "  statusDiv.style.display = 'block'; statusDiv.style.background = '#0284c7'; statusDiv.style.color = '#fff'; statusDiv.innerText = 'Mengirim pesan tes...';";
    html += "  var fd = new FormData(); fd.append('token', token); fd.append('chat', chat);";
    html += "  fetch('/test-telegram', { method: 'POST', body: fd })";
    html += "    .then(r => r.json())";
    html += "    .then(d => {";
    html += "       if(d.success){ statusDiv.style.background = '#059669'; statusDiv.innerText = '✅ ' + d.message; }";
    html += "       else { statusDiv.style.background = '#dc2626'; statusDiv.innerText = '❌ ' + d.message; }";
    html += "    })";
    html += "    .catch(e => { statusDiv.style.background = '#dc2626'; statusDiv.innerText = '❌ Gagal: ' + e; });";
    html += "}";
    html += "document.querySelector('form').addEventListener('submit', function(){";
    html += "  var btn = document.getElementById('btnSubmit');";
    html += "  btn.innerHTML = '⏳ Menyimpan...'; btn.style.opacity = '0.7';";
    html += "});";
    html += "</script>";

    html += "</body></html>";
    return html;
}

String WifiManagerCustom::generateSuccessHtml(String message, bool restart) {
    String html = "<!DOCTYPE html><html lang='id'><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Status Pengaturan</title>";
    html += "<style>";
    html += "*{box-sizing:border-box;margin:0;padding:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;}";
    html += "body{background:#0b1120;color:#f1f5f9;display:flex;justify-content:center;align-items:center;min-height:100vh;padding:16px;}";
    html += ".card{background:#1e293b;border:1px solid #334155;border-radius:16px;max-width:400px;width:100%;padding:32px;text-align:center;box-shadow:0 20px 25px -5px rgba(0,0,0,0.5);}";
    html += ".icon{font-size:48px;margin-bottom:16px;}";
    html += "h1{font-size:18px;font-weight:700;color:" + String(restart ? "#38bdf8" : "#ef4444") + ";margin-bottom:8px;}";
    html += "p{font-size:14px;color:#94a3b8;line-height:1.5;margin-bottom:20px;}";
    html += ".spinner{width:36px;height:36px;border:3px solid #334155;border-top-color:#38bdf8;border-radius:50%;animation:spin 1s linear infinite;margin:0 auto 16px;}";
    html += "@keyframes spin{to{transform:rotate(360deg);}}";
    html += "</style></head><body>";
    html += "<div class='card'>";
    html += "<div class='icon'>" + String(restart ? "✅" : "⚠️") + "</div>";
    html += "<h1>" + String(restart ? "Berhasil Disimpan!" : "Peringatan") + "</h1>";
    html += "<p>" + message + "</p>";
    if (restart) {
        html += "<div class='spinner'></div>";
        html += "<p style='font-size:12px;color:#64748b;'>ESP32 sedang me-restart dan menghubungkan ke jaringan...</p>";
    } else {
        html += "<a href='/' style='color:#38bdf8;text-decoration:none;font-size:14px;font-weight:600;'>← Kembali ke Form Setup</a>";
    }
    html += "</div></body></html>";
    return html;
}
