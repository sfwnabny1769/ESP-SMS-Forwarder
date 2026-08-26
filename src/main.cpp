#include <Arduino.h>                                                                                                                
#include "config.h"                                                                                                                 
#include "WifiManagerCustom.h"                                                                                                      
#include "GSMManager.h"                                                                                                             
#include "ApiClient.h"                                                                                                              
#include "TelegramClient.h"                                                                                                         
                                                                                                                                    
// ============================================================================                                                     
// Global Instances untuk Tri-Mode Gateway                                                                                          
// ============================================================================                                                     
WifiManagerCustom wifi;      // [Mode 2] Web Server on Chip & NVS Config Manager                                                    
GSMManager        gsm;       // [Hardware] SIM800L Engine & AT State Machine                                                        
ApiClient         api;       // [Mode 3] Laravel REST API Client                                                                    
TelegramClient    telegram;  // [Mode 1] Direct-to-Telegram HTTPS Client                                                            
                                                                                                                                    
unsigned long lastHeartbeatTime = 0;                                                                                                
                                                                                                                                    
void setup() {                                                                                                                      
    Serial.begin(115200);                                                                                                           
    delay(1000);                                                                                                                    
                                                                                                                                    
    Serial.println("\n==================================================");                                                         
    Serial.println("  ESP32-S3 SIM800L SMS Gateway (Tri-Mode Active)  ");                                                           
    Serial.println("==================================================");                                                           
    Serial.println("[Setup] Menginisialisasi WiFi, GSM SIM800L, Telegram, dan API...");                                             
                                                                                                                                    
    // 1. [MODE 2] Inisialisasi WiFi & Web Server on Chip (Membaca NVS Flash)                                                       
    wifi.begin();                                                                                                                   
                                                                                                                                    
    // 2. [HARDWARE] Inisialisasi Modul GSM SIM800L                                                                                 
    gsm.begin(&Serial2, SIM800_RX, SIM800_TX, SIM800_BAUD);                                                                         
                                                                                                                                    
    // 3. [MODE 1] Inisialisasi Direct Telegram Sender dari Flash NVS                                                               
    telegram.begin(wifi.getTelegramToken(), wifi.getTelegramChatId());                                                              
                                                                                                                                    
    // 4. [MODE 3] Inisialisasi Laravel API Client jika diaktifkan di Web Portal                                                    
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
    Serial.println("[Setup] Info: Tahan tombol BOOT (GPIO 0) selama 3 detik untuk membuka Web Portal Mode 2.\n");                   
}                                                                                                                                   
                                                                                                                                    
void loop() {                                                                                                                       
    // 1. Maintain WiFi Connection, Web Portal DNS/HTTP Server, dan Deteksi Tombol BOOT                                             
    wifi.update();                                                                                                                  
    gsm.update();                                                                                                                   
                                                                                                                                    
    unsigned long currentMillis = millis();                                                                                         
                                                                                                                                    
    // 2. [MODE 3] Heartbeat & Remote AT Console Loop (Hanya jika Mode 3 Aktif & WiFi Konek)                                        
    if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {                                        
        if (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {                                                              
            lastHeartbeatTime = currentMillis;                                                                                      
                                                                                                                                    
            int signal = gsm.getSignal();                                                                                           
            String opName = gsm.getOperator();                                                                                      
            String simStatus = gsm.getSIMStatus();                                                                                  
            String regStatus = gsm.getRegistrationStatus();                                                                         
            String pendingCmd = "";                                                                                                 
                                                                                                                                    
            // Kirim heartbeat ke Laravel & terima antrean perintah AT dari Web Console                                             
            bool hbSuccess = api.heartbeat(signal, opName, simStatus, regStatus, pendingCmd);                                       
                                                                                                                                    
            if (hbSuccess && pendingCmd.length() > 0) {                                                                             
                Serial.println("\n[Remote Console] Menerima Perintah dari Web Laravel:");                                           
                Serial.println("> " + pendingCmd);                                                                                  
                                                                                                                                    
                // Cek jika perintah adalah penarikan pesan tersimpan di kartu SIM (SYNC_SIM_SMS)                                   
                if (pendingCmd == "SYNC_SIM_SMS" || pendingCmd == "PULL_SMS") {                                                     
                    Serial.println("[Remote Console] Menjalankan Penarikan Seluruh SMS dari Memori Kartu SIM...");                  
                                                                                                                                    
                    int pulledCount = gsm.syncStoredSMS(false); // Jangan hapus dulu, biarkan loop forwarder yang memproses         
                                                                                                                                    
                    String syncResult = "[SYNC_SIM_SMS OK] Berhasil menarik " + String(pulledCount) + " pesan dari kartu SIM.";     
                    Serial.println("[Remote Console] " + syncResult);                                                               
                    api.sendATResponse(pendingCmd, syncResult);                                                                     
                                                                                                                                    
                } else {                                                                                                            
                    // Eksekusi AT Command kustom umum                                                                              
                    String result = gsm.executeCustomAT(pendingCmd, 4000);                                                          
                    Serial.println("[Remote Console] Hasil Eksekusi SIM800L:\n" + result);                                          
                    api.sendATResponse(pendingCmd, result);                                                                         
                }                                                                                                                   
            }                                                                                                                       
        }                                                                                                                           
    }                                                                                                                               
                                                                                                                                    
    // 3. [MODE 1 & MODE 3] Alur Forwarding SMS Masuk (Real-time Inbound SMS)                                                       
    if (gsm.hasNewSMS()) {                                                                                                          
        SMSMessage sms = gsm.readSMS();                                                                                             
                                                                                                                                    
        Serial.println("\n==================================================");                                                     
        Serial.printf("[SMS Inbound] Dari: %s | Waktu: %s\n", sms.phone.c_str(), sms.datetime.c_str());                             
        Serial.printf("[SMS Inbound] Pesan: %s\n", sms.message.c_str());                                                            
        Serial.println("==================================================");                                                       
                                                                                                                                    
        bool tgSuccess = false;                                                                                                     
        bool apiSuccess = false;                                                                                                    
                                                                                                                                    
        // 🟢 Langkah A [MODE 1]: Selalu utamakan kirim langsung ke Telegram (24/7 Standalone)                                      
        if (telegram.isConfigured() && wifi.isConnected()) {                                                                        
            Serial.println("[Forwarder] [Mode 1] Mengirim langsung ke Telegram Cloud API...");                                      
            tgSuccess = telegram.sendSMS(sms);                                                                                      
        } else if (!telegram.isConfigured()) {                                                                                      
            Serial.println("[Forwarder] [Mode 1] Lewat: Telegram Bot Token / Chat ID belum diset.");                                
        }                                                                                                                           
                                                                                                                                    
        // 🔵 Langkah B [MODE 3]: Kirim ke Backend Laravel jika sinkronisasi aktif                                                  
        if (wifi.isServerSyncEnabled() && wifi.getApiUrl().length() > 0 && wifi.isConnected()) {                                    
            Serial.println("[Forwarder] [Mode 3] Meneruskan ke Laravel REST API...");                                               
            apiSuccess = api.sendSMS(sms);                                                                                          
        }                                                                                                                           
                                                                                                                                    
        // 🗑️ Langkah C: Hapus SMS dari memori SIM card jika sudah terkirim atau ditangani                                          
        if (tgSuccess || apiSuccess || !wifi.isConnected()) {                                                                       
            if (sms.index > 0) {                                                                                                    
                gsm.deleteSMS(sms.index);                                                                                           
                Serial.printf("[Forwarder] SMS #%d berhasil diproses dan dibersihkan dari kartu SIM.\n", sms.index);                
            }                                                                                                                       
        } else {                                                                                                                    
            Serial.println("[Forwarder Warning] Pengiriman gagal. SMS tetap disimpan di SIM untuk dicoba ulang.");                  
        }                                                                                                                           
    }                                                                                                                               
                                                                                                                                    
    delay(10); // Yield ke FreeRTOS background tasks                                                                                
}    