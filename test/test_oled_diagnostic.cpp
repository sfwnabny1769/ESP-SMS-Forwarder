#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SDA_PIN 8
#define SCL_PIN 9
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
uint8_t detectedAddress = 0;
int counter = 0;

void scanI2C() {
    Serial.println("\n--- [1] Memindai Alamat I2C pada SDA: GPIO 8, SCL: GPIO 9 ---");
    Wire.begin(SDA_PIN, SCL_PIN);
    
    int deviceCount = 0;
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            Serial.printf(" [FOUND] Perangkat I2C DITEMUKAN pada alamat: 0x%02X\n", address);
            if (detectedAddress == 0) detectedAddress = address;
            deviceCount++;
        }
    }

    if (deviceCount == 0) {
        Serial.println(" [ERROR] TIDAK ADA perangkat I2C yang terdeteksi!");
        Serial.println("  -> Tips: Cek kabel VCC/GND, dan pastikan kabel SDA & SCL tidak terbalik.");
    } else {
        Serial.printf(" [SUCCESS] Ditemukan %d perangkat I2C.\n", deviceCount);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\n==================================================");
    Serial.println("   DIAGNOSTIK HARDWARE OLED 0.96\" (SSD1306)       ");
    Serial.println("==================================================");

    // 1. Scan I2C Bus
    scanI2C();

    // Jika tidak ditemukan di 8/9, coba alamat default 0x3C
    if (detectedAddress == 0) {
        detectedAddress = 0x3C;
    }

    Serial.printf("\n--- [2] Mencoba Inisialisasi Layar pada 0x%02X ---\n", detectedAddress);

    // 2. Inisialisasi Layar
    if (!display.begin(SSD1306_SWITCHCAPVCC, detectedAddress)) {
        Serial.println(" [GAGAL] Inisialisasi SSD1306 gagal. Mencoba alamat alternatif 0x3D...");
        if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
            Serial.println(" [GAGAL] Inisialisasi SSD1306 pada 0x3D juga gagal!");
            return;
        } else {
            detectedAddress = 0x3D;
        }
    }

    Serial.println(" [BERHASIL] Layar OLED SSD1306 Terhubung & Aktif!");

    // 3. Test Flash Layar Penuh (Semua Piksel Nyala)
    display.clearDisplay();
    display.fillScreen(SSD1306_WHITE);
    display.display();
    delay(1000);

    // 4. Test Teks & Kotak
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    display.setCursor(10, 10);
    display.println("OLED TEST: OK!");
    
    display.setCursor(10, 26);
    display.printf("Addr : 0x%02X\n", detectedAddress);
    display.setCursor(10, 38);
    display.println("Pins : SDA 8, SCL 9");
    display.display();
}

void loop() {
    if (detectedAddress != 0) {
        counter++;
        display.fillRect(10, 50, 108, 10, SSD1306_BLACK);
        display.setCursor(10, 50);
        display.printf("Running: %d s", counter);
        display.display();
    }
    delay(1000);
}
