#include <Arduino.h>
#include "config.h"
#include "WifiManagerCustom.h"
#include "GSMManager.h"
#include "ApiClient.h"

// Global Manager Instances
WifiManagerCustom wifi;
GSMManager        gsm;
ApiClient         api;

unsigned long lastHeartbeatTime = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("  ESP32-S3 SIM800L SMS Gateway Starting...");
    Serial.println("==================================================");
    Serial.println("[Setup] Menginisialisasi WiFi Manager, SIM800L, dan API Client...");

    // 1. Inisialisasi WiFi & Web Provisioning Portal (Membaca NVS Flash)
    wifi.begin();

    // 2. Inisialisasi Modul GSM SIM800L
    gsm.begin(&Serial2, SIM800_RX, SIM800_TX, SIM800_BAUD);

    // 3. Inisialisasi API Client menggunakan URL Server dan Token dari memori Flash
    api.begin(wifi.getApiUrl(), wifi.getDeviceToken());

    Serial.println("\n[Setup] Konfigurasi Aktif:");
    Serial.printf(" - WiFi SSID    : %s\n", wifi.getSSID().c_str());
    Serial.printf(" - API Server   : %s\n", wifi.getApiUrl().c_str());
    Serial.printf(" - Device Token : %s\n", wifi.getDeviceToken().c_str());
    Serial.println("[Setup] Info: Tahan tombol BOOT (GPIO 0) selama 3 detik kapan saja untuk membuka Web Portal Konfigurasi.\n");
}

void loop() {
    // 1. Maintain WiFi Connection, Web Portal DNS/Server, and BOOT button trigger
    wifi.update();
    gsm.update();

    unsigned long currentMillis = millis();

    // 2. Heartbeat Timer (every HEARTBEAT_INTERVAL)
    if (wifi.isConnected() && (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL)) {
        lastHeartbeatTime = currentMillis;

        int signal = gsm.getSignal();
        String opName = gsm.getOperator();
        String simStatus = gsm.getSIMStatus();
        String regStatus = gsm.getRegistrationStatus();
        String pendingCmd = "";

        // Send heartbeat & receive any pending remote AT command from Web Console
        bool hbSuccess = api.heartbeat(signal, opName, simStatus, regStatus, pendingCmd);

        if (hbSuccess && pendingCmd.length() > 0) {
            Serial.println("\n[Remote Console] Menerima Perintah dari Web:");
            Serial.println("> " + pendingCmd);

            // Cek jika perintah adalah penarikan pesan tersimpan di kartu SIM (SYNC_SIM_SMS)
            if (pendingCmd == "SYNC_SIM_SMS" || pendingCmd == "PULL_SMS") {
                Serial.println("[Remote Console] Menjalankan Penarikan Seluruh SMS dari Memori Kartu SIM...");
                
                int pulledCount = gsm.syncStoredSMS(true);

                // Kirim seluruh SMS yang ditarik ke API Laravel
                int forwardedCount = 0;
                while (gsm.hasNewSMS()) {
                    SMSMessage sms = gsm.readSMS();
                    if (api.sendSMS(sms)) {
                        forwardedCount++;
                    }
                }

                String syncResult = "[SYNC_SIM_SMS OK] Berhasil menarik " + String(pulledCount) + 
                                    " pesan dari memori kartu SIM dan meneruskan " + String(forwardedCount) + 
                                    " pesan ke database Laravel.";
                
                Serial.println("[Remote Console] " + syncResult);
                api.sendATResponse(pendingCmd, syncResult);

            } else {
                // Eksekusi AT Command kustom umum
                String result = gsm.executeCustomAT(pendingCmd, 4000);

                Serial.println("[Remote Console] Hasil Eksekusi SIM800L:");
                Serial.println(result);

                // Kirim hasil eksekusi kembali ke Web Console Laravel
                api.sendATResponse(pendingCmd, result);
            }
        }
    }

    // 3. SMS Detection & Forwarding Flow (Real-time incoming SMS)
    if (gsm.hasNewSMS()) {
        SMSMessage sms = gsm.readSMS();

        Serial.printf("\n[Forwarder] Meneruskan SMS dari %s ke Laravel API...\n", sms.phone.c_str());

        if (wifi.isConnected() && api.sendSMS(sms)) {
            if (sms.index > 0) {
                gsm.deleteSMS(sms.index);
                Serial.printf("[Forwarder] SMS #%d berhasil disimpan ke Web dan dihapus dari SIM.\n", sms.index);
            } else {
                Serial.println("[Forwarder] SMS streaming berhasil disimpan ke Web.");
            }
        } else {
            Serial.println("[Forwarder Warning] API error atau WiFi terputus. SMS tetap disimpan di SIM untuk dicoba ulang.");
        }
    }

    delay(10); // Yield to system tasks
}
