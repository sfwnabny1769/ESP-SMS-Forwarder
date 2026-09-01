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

// Struktur data untuk melacak jumlah percobaan retry
struct QueuedSMS {
    SMSMessage sms;
    int retryCount;
};

// Antrean penampung SMS yang gagal terkirim (Offline Retry Queue)
std::vector<QueuedSMS> failedQueue;

unsigned long lastHeartbeatTime = 0;
unsigned long lastRetryAttempt = 0;
unsigned long lastDisplayRefresh = 0;
unsigned long lastVoltageCheck = 0;
const unsigned long RETRY_INTERVAL = 15000; // Coba kirim ulang setiap 15 detik
const int MAX_RETRY_LIMIT = 5;              // Maksimal 5x percobaan sebelum ditandai gagal permanen

int totalSmsReceived = 0;
float gsmVoltage = 0.0f;
unsigned long bootButtonPressTime = 0;
bool bootButtonHandled = false;
bool portalDisplayShown = false;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("  ESP32-S3 SIM800L SMS Gateway (Tri-Mode Active)  ");
    Serial.println("==================================================");

    // 1. Inisialisasi Layar OLED (Safe Probe)
    display.begin(OLED_SDA, OLED_SCL, OLED_I2C_ADDRESS, OLED_TIMEOUT);

    // 2. [MODE 2] Inisialisasi WiFi & Web Server on Chip (Membaca NVS Flash)
    wifi.begin();

    // 3. [HARDWARE] Inisialisasi Modul GSM SIM800L
    gsm.begin(&Serial2, SIM800_RX, SIM800_TX, SIM800_BAUD);

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
}

void loop() {
    // 1. Maintain WiFi Connection & Background Server
    wifi.update();
    gsm.update();
    display.update(); // Maintain OLED Smart Sleep timer

    unsigned long currentMillis = millis();

    // Query Tegangan Baterai / Power SIM800L setiap 20 detik secara berkala
    if (currentMillis - lastVoltageCheck >= 20000 || gsmVoltage == 0.0f) {
        lastVoltageCheck = currentMillis;
        float v = gsm.getBatteryVoltage();
        if (v > 0.0f) gsmVoltage = v;
    }

    // 2. Mode Display Handler (Portal Mode vs Live Status Mode)
    if (wifi.isPortalRunning()) {
        if (!portalDisplayShown) {
            portalDisplayShown = true;
            Serial.printf("[OLED Portal] Menampilkan SSID: %s, PASS: %s, IP: %s\n",
                          wifi.getPortalSSID().c_str(),
                          wifi.getPortalPassword().c_str(),
                          wifi.getAPIP().c_str());
            display.showPortalMode(wifi.getPortalSSID(), wifi.getPortalPassword(), wifi.getAPIP());
        }
    } else {
        portalDisplayShown = false;

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
                        gsmVoltage,
                        gsm.getSIMStatus() == "READY"
                    );
                }
                bootButtonPressTime = 0;
            }
        }

        // Refresh data OLED berkala (tiap 1.5 detik) HANYA jika layar sedang menyala
        if (display.isDisplayOn() && (currentMillis - lastDisplayRefresh >= 1500)) {
            lastDisplayRefresh = currentMillis;
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
                gsmVoltage,
                gsm.getSIMStatus() == "READY"
            );
        }
    }

    // 3. [MODE 3] Heartbeat & Remote AT Console Loop
    if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {
        if (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
            lastHeartbeatTime = currentMillis;

            int signal = gsm.getSignal();
            String opName = gsm.getOperator();
            String simStatus = gsm.getSIMStatus();
            String regStatus = gsm.getRegistrationStatus();
            String pendingCmd = "";

            bool hbSuccess = api.heartbeat(signal, opName, simStatus, regStatus, pendingCmd);

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

    // 4. [MODE 1 & MODE 3] Alur Forwarding SMS Masuk (Real-time Inbound SMS)
    if (gsm.hasNewSMS()) {
        SMSMessage sms = gsm.readSMS();
        totalSmsReceived++;

        Serial.println("\n==================================================");
        Serial.printf("[SMS Inbound] Dari: %s | Waktu: %s\n", sms.phone.c_str(), sms.datetime.c_str());
        Serial.printf("[SMS Inbound] Pesan: %s\n", sms.message.c_str());
        Serial.println("==================================================");

        // Visual Notification di Layar OLED (Otomatis bangun & tampil 10 detik)
        display.showNewSMS(sms.phone, sms.message);

        bool tgSuccess = false;
        bool apiSuccess = false;

        // [MODE 1]: Kirim langsung ke Telegram
        if (telegram.isConfigured() && wifi.isConnected()) {
            Serial.println("[Forwarder] [Mode 1] Mengirim langsung ke Telegram Cloud API...");
            tgSuccess = telegram.sendSMS(sms);
        }

        // [MODE 3]: Kirim ke Backend Laravel
        if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {
            Serial.println("[Forwarder] [Mode 3] Meneruskan ke Laravel REST API...");
            apiSuccess = api.sendSMS(sms);
        }

        // Hapus SMS dari memori SIM jika berhasil dikirim
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
        
    // 5. [RETRY WORKER] Memproses Antrean SMS yang Tertunda Saat WiFi Kembali Online
    if (wifi.isConnected() && (currentMillis - lastRetryAttempt >= RETRY_INTERVAL)) {
        lastRetryAttempt = currentMillis;

        if (!failedQueue.empty()) {
            Serial.printf("\n[Retry Worker] Memproses %d pesan dalam antrean retry...\n", failedQueue.size());

            for (auto it = failedQueue.begin(); it != failedQueue.end(); ) {
                if (telegram.sendSMS(it->sms)) {
                    Serial.printf("[Retry Worker] SMS dari %s BERHASIL dikirim ulang!\n", it->sms.phone.c_str());
                    if (it->sms.index > 0) {
                        gsm.deleteSMS(it->sms.index);
                    }
                    it = failedQueue.erase(it);
                } else {
                    it->retryCount++;
                    Serial.printf("[Retry Worker] Percobaan #%d gagal untuk SMS dari %s.\n", it->retryCount, it->sms.phone.c_str());

                    if (it->retryCount >= MAX_RETRY_LIMIT) {
                        Serial.printf("[Retry Worker] ❌ Pesan dari %s mencapai batas maksimal (%d). Dihapus dari SIM.\n",
                                        it->sms.phone.c_str(), MAX_RETRY_LIMIT);
                        if (it->sms.index > 0) {
                            gsm.deleteSMS(it->sms.index);
                        }
                        it = failedQueue.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }

        // Auto-Sweep: Cek jika ada SMS tersangkut di memori SIM
        if (failedQueue.empty()) {
            int pulledCount = gsm.syncStoredSMS(false);
            if (pulledCount > 0) {
                Serial.printf("[Auto-Sweep] Ditemukan %d pesan tersimpan di SIM card. Memproses...\n", pulledCount);
            }
        }
    }

    delay(10);
}