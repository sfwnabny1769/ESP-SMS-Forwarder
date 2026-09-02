#include <Arduino.h>
#include "config.h"
#include "WifiManagerCustom.h"
#include "GSMManager.h"
#include "ApiClient.h"
#include "TelegramClient.h"
#include "DisplayManager.h"
#include <vector>

// ============================================================================
// Global Instances untuk Tri-Mode Gateway & Display
// ============================================================================
WifiManagerCustom wifi;      // [Mode 2] Web Server on Chip & NVS Config Manager
GSMManager        gsm;       // [Hardware] SIM800L Engine & AT State Machine
ApiClient         api;       // [Mode 3] Laravel REST API Client
TelegramClient    telegram;  // [Mode 1] Direct-to-Telegram HTTPS Client
DisplayManager    display;   // [Hardware UI] 0.96" SSD1306 OLED Display

// Mutex untuk sinkronisasi thread-safe akses GSM & Shared Globals (Core 0 vs Core 1)
SemaphoreHandle_t gsmMutex = NULL;

// Struktur data untuk melacak jumlah percobaan retry
struct QueuedSMS {
    SMSMessage sms;
    int retryCount;
};

// Antrean penampung SMS yang gagal terkirim (Offline Retry Queue)
std::vector<QueuedSMS> failedQueue;

int gsmConsecutiveFailures = 0;
const int MAX_GSM_FAILURES_BEFORE_HARDWARE_RESET = 3; // Batas kegagalan berturut-turut sebelum reset modem

unsigned long lastHeartbeatTime = 0;
unsigned long lastRetryAttempt = 0;
unsigned long lastDisplayRefresh = 0;
unsigned long lastVoltageCheck = 0;

const unsigned long RETRY_INTERVAL = 15000;         // Coba kirim ulang setiap 15 detik
const int MAX_RETRY_LIMIT = 5;                      // Maksimal 5x percobaan sebelum ditandai gagal permanen

int totalSmsReceived = 0;
float gsmVoltage = 0.0f;
bool isLaravelConnected = false;
unsigned long bootButtonPressTime = 0;
bool bootButtonHandled = false;
bool portalDisplayShown = false;
String lastPortalSSID = "";

// ============================================================================
// Handler Perintah Kontrol Jarak Jauh (Telegram Bot Remote Control - Core 0)
// ============================================================================
void handleTelegramBotCommands() {
    if (!telegram.isConfigured() || !wifi.isConnected()) return;

    // Polling getUpdates dilakukan tanpa mutex agar tidak memblokir Core 1
    std::vector<TelegramIncomingMessage> messages = telegram.getNewMessages();
    for (const auto &msg : messages) {
        // 1. Otorisasi Pengirim (Hanya Chat ID resmi yang diizinkan)
        if (!telegram.isAuthorized(msg.chatId)) {
            Serial.printf("[Telegram Security] ⛔ Akses Ditolak dari Chat ID: %s (Nama: %s) | Teks: %s\n",
                          msg.chatId.c_str(), msg.senderName.c_str(), msg.text.c_str());
            telegram.sendMessageTo(msg.chatId, "⛔ <b>Akses Ditolak</b>\nAnda tidak memiliki izin untuk mengontrol gateway ini.");
            continue;
        }

        Serial.printf("\n[Telegram Command] Menerima Perintah dari %s (Chat ID: %s): %s\n",
                      msg.senderName.c_str(), msg.chatId.c_str(), msg.text.c_str());

        String cmd = msg.text;
        cmd.trim();

        // Bersihkan mention bot jika ada, contoh: /status@MyBot -> /status
        int atIdx = cmd.indexOf('@');
        if (atIdx != -1) {
            cmd = cmd.substring(0, atIdx);
        }

        // ====================================================================
        // Command Dispatcher (Sinkronisasi Mutex untuk Thread Safety)
        // ====================================================================
        if (cmd.equalsIgnoreCase("/start") || cmd.equalsIgnoreCase("/help")) {
            String helpText = "🤖 <b>ESP32 SMS GATEWAY COMMAND CENTER</b>\n";
            helpText += "━━━━━━━━━━━━━━━━━━━━\n";
            helpText += "📌 <b>Perintah Monitoring:</b>\n";
            helpText += "/status - Status lengkap sistem & jaringan\n";
            helpText += "/signal - Cek kuat sinyal (CSQ & dBm) + Operator\n";
            helpText += "/voltage - Cek tegangan suplai daya SIM800L\n\n";
            helpText += "⚡ <b>Perintah Kontrol:</b>\n";
            helpText += "/sync_sms - Tarik seluruh SMS dari memori SIM\n";
            helpText += "/restart_gsm - Re-inisialisasi modul SIM800L\n";
            helpText += "/reboot - Restart chip ESP32 Gateway\n";
            helpText += "/help - Tampilkan menu bantuan ini";
            telegram.sendMessage(helpText);

        } else if (cmd.equalsIgnoreCase("/status")) {
            int signal = 0;
            String op = "", simStatus = "", regStatus = "", signalQuality = "";
            int dbm = -113;
            float currentVoltage = 0.0f;
            int totalSms = 0, queueSize = 0;

            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                signal = gsm.getSignal();
                op = gsm.getOperator();
                simStatus = gsm.getSIMStatus();
                regStatus = gsm.getRegistrationStatus();
                signalQuality = gsm.getSignalQualityText();
                dbm = (signal >= 2 && signal <= 30) ? (-113 + (signal * 2)) : -113;
                currentVoltage = gsmVoltage;
                totalSms = totalSmsReceived;
                queueSize = failedQueue.size();
                xSemaphoreGive(gsmMutex);
            }

            unsigned long totalSec = millis() / 1000;
            unsigned long days = totalSec / 86400;
            unsigned long hours = (totalSec % 86400) / 3600;
            unsigned long minutes = (totalSec % 3600) / 60;
            unsigned long seconds = totalSec % 60;
            char uptimeBuf[32];
            snprintf(uptimeBuf, sizeof(uptimeBuf), "%02lud %02luh %02lum %02lus", days, hours, minutes, seconds);

            String statusText = "📊 <b>STATUS SISTEM GATEWAY</b>\n";
            statusText += "━━━━━━━━━━━━━━━━━━━━\n";
            statusText += "📶 <b>Provider    :</b> " + op + "\n";
            statusText += "📶 <b>Sinyal CSQ  :</b> " + String(signal) + "/31 (" + String(dbm) + " dBm) [" + signalQuality + "]\n";
            statusText += "📱 <b>SIM Card    :</b> " + simStatus + " (" + regStatus + ")\n";
            statusText += "⚡ <b>Tegangan GSM:</b> " + (currentVoltage > 0 ? String(currentVoltage, 2) + " V" : "Membaca...") + "\n";
            statusText += "━━━━━━━━━━━━━━━━━━━━\n";
            statusText += "🌐 <b>WiFi IP     :</b> " + wifi.getIP() + "\n";
            statusText += "☁️ <b>Laravel API :</b> ";
            statusText += (wifi.isServerSyncEnabled() ? (isLaravelConnected ? "Connected (OK)" : "Error / Unreachable") : "Non-aktif");
            statusText += "\n";
            statusText += "━━━━━━━━━━━━━━━━━━━━\n";
            statusText += "📩 <b>Total SMS   :</b> " + String(totalSms) + " Diterima\n";
            statusText += "⏳ <b>Retry Queue :</b> " + String(queueSize) + " Tertunda\n";
            statusText += "⏱️ <b>Uptime      :</b> " + String(uptimeBuf) + "\n";
            statusText += "⚙️ <b>Kondisi     :</b> Standby OK";
            telegram.sendMessage(statusText);

        } else if (cmd.equalsIgnoreCase("/signal") || cmd.equalsIgnoreCase("/csq")) {
            int signal = 0;
            String op = "", regStatus = "", signalQuality = "";
            int dbm = -113;

            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                signal = gsm.getSignal();
                op = gsm.getOperator();
                regStatus = gsm.getRegistrationStatus();
                signalQuality = gsm.getSignalQualityText();
                dbm = (signal >= 2 && signal <= 30) ? (-113 + (signal * 2)) : -113;
                xSemaphoreGive(gsmMutex);
            }

            String sigText = "📶 <b>INFORMASI SINYAL GSM</b>\n";
            sigText += "━━━━━━━━━━━━━━━━━━━━\n";
            sigText += "• <b>Operator :</b> " + op + "\n";
            sigText += "• <b>Kuat CSQ :</b> " + String(signal) + " / 31\n";
            sigText += "• <b>Desibel  :</b> " + String(dbm) + " dBm (" + signalQuality + ")\n";
            sigText += "• <b>Jaringan :</b> " + regStatus;
            telegram.sendMessage(sigText);

        } else if (cmd.equalsIgnoreCase("/voltage") || cmd.equalsIgnoreCase("/baterai") || cmd.equalsIgnoreCase("/battery")) {
            float v = 0.0f;
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                v = gsm.getBatteryVoltage();
                if (v > 0) gsmVoltage = v;
                else v = gsmVoltage;
                xSemaphoreGive(gsmMutex);
            }

            String voltText = "⚡ <b>STATUS TEGANGAN DAYA</b>\n";
            voltText += "━━━━━━━━━━━━━━━━━━━━\n";
            voltText += "• <b>Tegangan SIM800L:</b> " + (v > 0 ? String(v, 2) + " V" : "Gagal membaca") + "\n";
            voltText += "• <b>Rekomendasi   :</b> 3.80 V - 4.20 V\n";
            voltText += "• <b>Status        :</b> ";
            voltText += (v >= 3.7f && v <= 4.4f ? "✅ Normal & Stabil" : "⚠️ Periksa Sumber Daya!");
            telegram.sendMessage(voltText);

        } else if (cmd.equalsIgnoreCase("/sync_sms") || cmd.equalsIgnoreCase("/pull_sms")) {
            telegram.sendMessage("⏳ <i>Memulai penarikan seluruh SMS dari memori SIM card...</i>");
            int pulledCount = 0;
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(15000)) == pdTRUE) {
                pulledCount = gsm.syncStoredSMS(false);
                xSemaphoreGive(gsmMutex);
            }
            telegram.sendMessage("📥 <b>Penarikan SMS Selesai</b>\nBerhasil menarik dan memproses <code>" + String(pulledCount) + "</code> pesan dari kartu SIM.");

        } else if (cmd.equalsIgnoreCase("/restart_gsm") || cmd.equalsIgnoreCase("/reset_gsm")) {
            telegram.sendMessage("🔄 <i>Menginisialisasi ulang modul GSM SIM800L...</i>");
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(10000)) == pdTRUE) {
                gsm.begin(&Serial2, SIM800_RX, SIM800_TX, SIM800_BAUD);
                xSemaphoreGive(gsmMutex);
            }
            telegram.sendMessage("✅ <b>Modul SIM800L Berhasil Diinisialisasi Ulang.</b>");

        } else if (cmd.equalsIgnoreCase("/reboot") || cmd.equalsIgnoreCase("/restart")) {
            telegram.sendMessage("⚠️ <b>Rebooting Gateway...</b>\nChip ESP32 sedang melakukan restart dalam 1 detik.");
            delay(1000);
            ESP.restart();

        } else {
            String unkText = "❓ <b>Perintah Tidak Dikenal:</b> <code>" + cmd + "</code>\n\n";
            unkText += "Ketik /help untuk melihat daftar perintah yang tersedia.";
            telegram.sendMessage(unkText);
        }
    }
}

TaskHandle_t telegramTaskHandle = NULL;

// Deklarasikan Handle dan Fungsi Task FreeRTOS (Core 0)
void telegramBotTask(void *pvParameters) {
    for (;;) {
        if (wifi.isConnected() && telegram.isConfigured()) {
            handleTelegramBotCommands();
        }
        vTaskDelay(pdMS_TO_TICKS(2500)); // Delay 2.5 detik untuk polling perintah bot
    }
}

void setup() {
    //0. Turunkan frekuensi CPU ke 80 MHz (power safe dan dingin)
    #ifdef GATEWAY_CPU_FREQ_MHZ
    setCpuFrequencyMhz(GATEWAY_CPU_FREQ_MHZ);
    #endif
    
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");                                                         
    Serial.println("  ESP32-S3 SIM800L SMS Gateway (Eco-Mode Active)  ");                                                           
    Serial.println("==================================================");                                                           
    Serial.printf("[Power] CPU Clock Berjalan pada: %d MHz\n", getCpuFrequencyMhz()); 

    // 0. Inisialisasi FreeRTOS Mutex untuk Sinkronisasi Thread-Safe
    gsmMutex = xSemaphoreCreateMutex();

    // 1. Inisialisasi Layar OLED (Safe Probe)
    display.begin(OLED_SDA, OLED_SCL, OLED_I2C_ADDRESS, OLED_TIMEOUT);

    // 2. [MODE 2] Inisialisasi WiFi & Web Server on Chip (Membaca NVS Flash)
    wifi.begin();

    // Aktifkan WiFi Modem-Sleep Protocol (Hemat Arus WiFi dari 120mA -> 15mA)                                                      
    WiFi.setSleep(WIFI_PS_MIN_MODEM);                                                                                               
    Serial.println("[Power] WiFi Modem-Sleep Protocol (DTIM) Diaktifkan."); 

    // 3. [HARDWARE] Inisialisasi Modul GSM SIM800L
    gsm.begin(&Serial2, SIM800_RX, SIM800_TX, SIM800_BAUD, SIM800_RST_PIN, SIM800_DTR_PIN, SIM800_RI_PIN);

    // 3b. Pasang Hardware Interrupt untuk Pin RING/RI (Event-Driven SMS)                                                           
    if (SIM800_RI_PIN >= 0) {                                                                                                       
        pinMode(SIM800_RI_PIN, INPUT_PULLUP);                                                                                       
        attachInterrupt(digitalPinToInterrupt(SIM800_RI_PIN), []() {                                                                
            gsm.notifyRingInterrupt();                                                                                              
        }, FALLING);                                                                                                                
        Serial.println("[Hardware] Interrupt Pin RI (Ring Indicator) Diaktifkan.");                                                 
    }  

    // 4. [MODE 1] Inisialisasi Direct Telegram Sender dari Flash NVS
    telegram.begin(wifi.getTelegramToken(), wifi.getTelegramChatId());

    // 5. [MODE 3] Inisialisasi Laravel API Client jika diaktifkan di Web Portal
    if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0) {
        api.begin(wifi.getApiUrl(), wifi.getDeviceToken());
    }

    Serial.println("\n[Setup] Status Konfigurasi Aktif:");
    Serial.printf(" - WiFi SSID       : %s\n", wifi.getSSID().c_str());
    Serial.printf(" - Mode 1 Telegram : %s (Chat ID: %s)\n",
                    telegram.isConfigured() ? "Aktif" : "Belum Diatur",
                    wifi.getTelegramChatId().c_str());
    Serial.printf(" - Mode 3 Server   : %s (%s)\n",
                    wifi.isServerSyncEnabled() ? "Aktif" : "Non-aktif",
                    wifi.getApiUrl().c_str());
    Serial.printf(" - OLED Display    : %s\n", display.isAvailable() ? "Aktif" : "Tidak Terdeteksi");
    Serial.println("[Setup] Info: Tekan BOOT 1x untuk lihat status di OLED, tahan 3s untuk Web Portal.\n");

    // 6. [FreeRTOS] Jalankan Telegram Bot Poller di Core 0 sebagai Background Task
    xTaskCreatePinnedToCore(
        telegramBotTask,      // Fungsi task
        "TelegramBotTask",    // Nama task
        8192,                 // Ukuran stack (8 KB untuk HTTPS SSL Handshake)
        NULL,                 // Parameter task (tidak ada)
        1,                    // Prioritas task
        &telegramTaskHandle,  // Handle task
        0                     // Jalankan di Core 0
    );
}

void loop() {
    // 1. Maintain WiFi Connection & Background Server (Core 1)
    wifi.update();
    display.update(); // Maintain OLED Smart Sleep timer

    unsigned long currentMillis = millis();

    // 2. Mode Display Handler (Portal Mode vs Live Status Mode)
    if (wifi.isPortalRunning()) {
        if (!portalDisplayShown || lastPortalSSID != wifi.getPortalSSID()) {
            portalDisplayShown = true;
            lastPortalSSID = wifi.getPortalSSID();
            Serial.printf("[OLED Portal] Menampilkan SSID: %s, PASS: %s, IP: %s\n",
                          wifi.getPortalSSID().c_str(),
                          wifi.getPortalPassword().c_str(),
                          wifi.getAPIP().c_str());
            display.showPortalMode(wifi.getPortalSSID(), wifi.getPortalPassword(), wifi.getAPIP());
        }
    } else {
        if (portalDisplayShown) {
            // Baru saja keluar dari mode portal (Batal / Reconnect): Bangunkan layar dan tampilkan dashboard status!
            portalDisplayShown = false;
            lastPortalSSID = "";
            display.wakeUp();
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                display.showStatus(
                    wifi.isConnected(),
                    wifi.isConnected() ? wifi.getIP() : "No WiFi",
                    gsm.getSignal(),
                    gsm.getOperator(),
                    gsm.getSIMStatus(),
                    gsm.getRegistrationStatus(),
                    totalSmsReceived,
                    telegram.isConfigured(),
                    wifi.isServerSyncEnabled(),
                    isLaravelConnected,
                    gsmVoltage,
                    gsm.getSIMStatus() == "READY"
                );
                xSemaphoreGive(gsmMutex);
            }
        }

        // Deteksi Tombol BOOT (Single Click = Wake Up OLED)
        if (digitalRead(SETUP_TRIGGER_PIN) == LOW) {
            if (bootButtonPressTime == 0) {
                bootButtonPressTime = currentMillis;
                bootButtonHandled = false;
            }
        } else {
            if (bootButtonPressTime > 0) {
                unsigned long pressDuration = currentMillis - bootButtonPressTime;
                // Jika ditekan singkat (antara 50ms hingga 2500ms), bangunkan OLED
                if (pressDuration >= 50 && pressDuration < 2500) {
                    Serial.println("[Button] Klik tombol BOOT terdeteksi: Menampilkan Status di Layar OLED.");
                    display.wakeUp();
                    if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        display.showStatus(
                            wifi.isConnected(),
                            wifi.isConnected() ? wifi.getIP() : "No WiFi",
                            gsm.getSignal(),
                            gsm.getOperator(),
                            gsm.getSIMStatus(),
                            gsm.getRegistrationStatus(),
                            totalSmsReceived,
                            telegram.isConfigured(),
                            wifi.isServerSyncEnabled(),
                            isLaravelConnected,
                            gsmVoltage,
                            gsm.getSIMStatus() == "READY"
                        );
                        xSemaphoreGive(gsmMutex);
                    }
                }
                bootButtonPressTime = 0;
            }
        }

        // Refresh data OLED berkala (tiap 1.5 detik) HANYA jika layar sedang menyala
        if (display.isDisplayOn() && (currentMillis - lastDisplayRefresh >= 1500)) {
            lastDisplayRefresh = currentMillis;
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                display.showStatus(
                    wifi.isConnected(),
                    wifi.isConnected() ? wifi.getIP() : "No WiFi",
                    gsm.getSignal(),
                    gsm.getOperator(),
                    gsm.getSIMStatus(),
                    gsm.getRegistrationStatus(),
                    totalSmsReceived,
                    telegram.isConfigured(),
                    wifi.isServerSyncEnabled(),
                    isLaravelConnected,
                    gsmVoltage,
                    gsm.getSIMStatus() == "READY"
                );
                xSemaphoreGive(gsmMutex);
            }
        }

    }

    // 3. Operasi GSM & Forwarding Engine (Sinkronisasi Thread-Safe via gsmMutex)
    if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        gsm.update();

    // 1. Query Tegangan Baterai & Health Check SIM800L setiap 30 detik                                                         
    if (currentMillis - lastVoltageCheck >= 30000 || gsmVoltage == 0.0f) {                                                      
        lastVoltageCheck = currentMillis;                                                                                       
        float v = gsm.getBatteryVoltage();                                                                                      
                                                                                                                                
        if (v > 0.0f) {                                                                                                         
            gsmVoltage = v;                                                                                                     
            gsmConsecutiveFailures = 0; // Komunikasi normal, reset counter                                                     
        } else {                                                                                                                
            gsmConsecutiveFailures++;                                                                                           
            Serial.printf("[GSM Health Warning] Kegagalan komunikasi GSM #%d/%d.\n",                                            
                            gsmConsecutiveFailures, MAX_GSM_FAILURES_BEFORE_HARDWARE_RESET);                                      
                                                                                                                                
            if (gsmConsecutiveFailures >= MAX_GSM_FAILURES_BEFORE_HARDWARE_RESET) {                                             
                Serial.println("[GSM Recovery] ⚠️ Modem tidak merespon berturut-turut! Memicu HARDWARE RESET...");              
                gsm.hardwareReset();                                                                                            
                gsmConsecutiveFailures = 0;                                                                                     
            }                                                                                                                   
        }                                                                                                                       
    }                                                                                                                           
                
        // [MODE 3] Heartbeat & Remote AT Console Loop
        if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {
            if (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
                lastHeartbeatTime = currentMillis;

                int signal = gsm.getSignal();
                String opName = gsm.getOperator();
                String simStatus = gsm.getSIMStatus();
                String regStatus = gsm.getRegistrationStatus();
                String pendingCmd = "";

                bool hbSuccess = api.heartbeat(signal, opName, simStatus, regStatus, pendingCmd);
                isLaravelConnected = hbSuccess;

                if (hbSuccess && pendingCmd.length() > 0) {
                    Serial.println("\n[Remote Console] Menerima Perintah dari Web Laravel:");
                    Serial.println("> " + pendingCmd);

                    if (pendingCmd == "SYNC_SIM_SMS" || pendingCmd == "PULL_SMS") {
                        Serial.println("[Remote Console] Menjalankan Penarikan Seluruh SMS dari Memori SIM...");
                        int pulledCount = gsm.syncStoredSMS(false);
                        String syncResult = "[SYNC_SIM_SMS OK] Berhasil menarik " + String(pulledCount) + " pesan dari kartu SIM.";
                        Serial.println("[Remote Console] " + syncResult);
                        api.sendATResponse(pendingCmd, syncResult);
                    } else {
                        String result = gsm.executeCustomAT(pendingCmd, 4000);
                        Serial.println("[Remote Console] Hasil Eksekusi SIM800L:\n" + result);
                        api.sendATResponse(pendingCmd, result);
                    }
                }
            }
        }

        // [MODE 1 & MODE 3] Alur Forwarding SMS Masuk (Real-time Inbound SMS)
        if (gsm.hasNewSMS()) {
            SMSMessage sms = gsm.readSMS();
            totalSmsReceived++;

            Serial.println("\n==================================================");
            Serial.printf("[SMS Inbound] Dari: %s | Waktu: %s\n", sms.phone.c_str(), sms.datetime.c_str());
            Serial.printf("[SMS Inbound] Pesan: %s\n", sms.message.c_str());
            Serial.println("==================================================");

            // Visual Notification di Layar OLED
            display.showNewSMS(sms.phone, sms.message);

            // Lepaskan mutex sementara saat melakukan blocking network call ke Telegram/Laravel
            xSemaphoreGive(gsmMutex);

            bool tgSuccess = false;
            bool apiSuccess = false;

            if (telegram.isConfigured() && wifi.isConnected()) {
                Serial.println("[Forwarder] [Mode 1] Mengirim langsung ke Telegram Cloud API...");
                tgSuccess = telegram.sendSMS(sms);
            }

            if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {
                Serial.println("[Forwarder] [Mode 3] Meneruskan ke Laravel REST API...");
                apiSuccess = api.sendSMS(sms);
            }

            // Ambil kembali mutex untuk menghapus SMS dari kartu SIM
            if (xSemaphoreTake(gsmMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                if (tgSuccess) {
                    if (sms.index > 0) {
                        gsm.deleteSMS(sms.index);
                        Serial.printf("[Forwarder] SMS #%d berhasil dikirim & dihapus dari kartu SIM.\n", sms.index);
                    }
                } else {
                    Serial.println("[Forwarder Warning] Gagal kirim ke Telegram. Menyimpan SMS ke Antrean Retry...");
                    failedQueue.push_back({sms, 0});
                }
            }
        }

        // [RETRY WORKER] Memproses Antrean SMS yang Tertunda Saat WiFi Kembali Online
        if (wifi.isConnected() && (currentMillis - lastRetryAttempt >= RETRY_INTERVAL)) {
            lastRetryAttempt = currentMillis;

            if (!failedQueue.empty()) {
                Serial.printf("\n[Retry Worker] Memproses %d pesan dalam antrean retry...\n", failedQueue.size());

                for (auto it = failedQueue.begin(); it != failedQueue.end(); ) {
                    SMSMessage queuedSms = it->sms;

                    // Release mutex while sending network request
                    xSemaphoreGive(gsmMutex);
                    bool sent = telegram.sendSMS(queuedSms);
                    xSemaphoreTake(gsmMutex, portMAX_DELAY);

                    if (sent) {
                        Serial.printf("[Retry Worker] SMS dari %s BERHASIL dikirim ulang!\n", queuedSms.phone.c_str());
                        if (queuedSms.index > 0) {
                            gsm.deleteSMS(queuedSms.index);
                        }
                        it = failedQueue.erase(it);
                    } else {
                        it->retryCount++;
                        Serial.printf("[Retry Worker] Percobaan #%d gagal untuk SMS dari %s.\n", it->retryCount, queuedSms.phone.c_str());

                        if (it->retryCount >= MAX_RETRY_LIMIT) {
                            Serial.printf("[Retry Worker] ❌ Pesan dari %s mencapai batas maksimal (%d). Dihapus dari SIM.\n",
                                            queuedSms.phone.c_str(), MAX_RETRY_LIMIT);
                            if (queuedSms.index > 0) {
                                gsm.deleteSMS(queuedSms.index);
                            }
                            it = failedQueue.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }

        // Deteksi Trigger Hardware Interrupt Pin RI (SMS Baru Masuk)
        if (SIM800_RI_PIN >= 0 && gsm.isRingTriggered()) {
            gsm.clearRingTrigger();
            Serial.println("\n[Interrupt Event] 🔔 Sinyal Pulsa LOW dari Pin RI Terdeteksi (Ada Aktivitas SMS/Panggilan)!");
        }

        xSemaphoreGive(gsmMutex);
    }

    delay(15);
}