#include "DisplayManager.h"

DisplayManager::DisplayManager() 
    : _display(128, 64, &Wire, -1), _isAvailable(false), _isDisplayOn(false), _wakeUntil(0), _timeoutMs(15000) {}

bool DisplayManager::begin(int sdaPin, int sclPin, uint8_t i2cAddr, unsigned long timeoutMs) {
    _timeoutMs = timeoutMs;
    
    // Inisialisasi I2C Wire dengan Pin khusus ESP32-S3
    Wire.begin(sdaPin, sclPin);

    // I2C Safe Probe: Cek apakah OLED benar-benar terhubung agar tidak hang jika kabel lepas
    Wire.beginTransmission(i2cAddr);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[OLED] Display SSD1306 TIDAK terdeteksi pada I2C Address 0x%02X (SDA:%d, SCL:%d).\n", i2cAddr, sdaPin, sclPin);
        _isAvailable = false;
        return false;
    }

    if (!_display.begin(SSD1306_SWITCHCAPVCC, i2cAddr)) {
        Serial.println("[OLED] Inisialisasi SSD1306 Gagal.");
        _isAvailable = false;
        return false;
    }

    _isAvailable = true;
    _isDisplayOn = true;
    _wakeUntil = millis() + 5000; // Nyalakan 5 detik untuk boot splash

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    showBootSplash();

    Serial.println("[OLED] Layar SSD1306 Berhasil Diinisialisasi (Smart Sleep Aktif).");
    return true;
}

void DisplayManager::wakeUp(unsigned long customDurationMs) {
    if (!_isAvailable) return;

    if (!_isDisplayOn) {
        _display.ssd1306_command(SSD1306_DISPLAYON);
        _isDisplayOn = true;
    }

    unsigned long duration = (customDurationMs > 0) ? customDurationMs : _timeoutMs;
    _wakeUntil = millis() + duration;
}

void DisplayManager::sleep() {
    if (!_isAvailable || !_isDisplayOn) return;

    _display.clearDisplay();
    _display.display();
    _display.ssd1306_command(SSD1306_DISPLAYOFF);
    _isDisplayOn = false;
}

void DisplayManager::update() {
    if (!_isAvailable) return;

    // Cek apakah waktu aktif layar sudah habis (Smart Timeout)
    if (_isDisplayOn && _timeoutMs > 0 && millis() >= _wakeUntil) {
        sleep();
    }
}

void DisplayManager::drawSignalBars(int x, int y, int signalCsq) {
    // Gambar 4 bar sinyal bertingkat
    int bars = 0;
    if (signalCsq >= 20) bars = 4;
    else if (signalCsq >= 15) bars = 3;
    else if (signalCsq >= 10) bars = 2;
    else if (signalCsq > 0)  bars = 1;

    for (int i = 0; i < 4; i++) {
        int barHeight = (i + 1) * 2 + 2; // Tinggi bar 4, 6, 8, 10
        int barY = y + (10 - barHeight);
        if (i < bars) {
            _display.fillRect(x + (i * 3), barY, 2, barHeight, SSD1306_WHITE);
        } else {
            _display.drawPixel(x + (i * 3), y + 9, SSD1306_WHITE);
        }
    }
}

void DisplayManager::drawWiFiIcon(int x, int y, bool connected) {
    if (connected) {
        _display.fillCircle(x + 4, y + 6, 2, SSD1306_WHITE);
        _display.drawCircle(x + 4, y + 6, 5, SSD1306_WHITE);
    } else {
        _display.drawLine(x, y + 2, x + 8, y + 10, SSD1306_WHITE);
        _display.drawLine(x + 8, y + 2, x, y + 10, SSD1306_WHITE);
    }
}

void DisplayManager::showBootSplash() {
    if (!_isAvailable) return;

    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    _display.setCursor(14, 10);
    _display.println("ESP32-S3 GATEWAY");
    _display.setCursor(20, 24);
    _display.println("SIM800L Engine");

    _display.drawRect(10, 40, 108, 8, SSD1306_WHITE);
    _display.fillRect(12, 42, 104, 4, SSD1306_WHITE);

    _display.setCursor(28, 52);
    _display.println("Booting Up...");
    _display.display();
}

void DisplayManager::showStatus(bool wifiConnected, String ipAddress, int signalCsq, String operatorName, String simStatus, int smsCount, String modeStr) {
    if (!_isAvailable) return;

    wakeUp(); // Perpanjang timer aktif layar

    _display.clearDisplay();

    // 1. Header Bar
    drawSignalBars(2, 2, signalCsq);
    _display.setTextSize(1);
    _display.setCursor(17, 3);
    
    // Potong nama operator jika terlalu panjang
    String shortOp = operatorName.length() > 6 ? operatorName.substring(0, 6) : operatorName;
    _display.print(shortOp.length() > 0 ? shortOp : "NO-SIM");

    drawWiFiIcon(75, 1, wifiConnected);

    _display.setCursor(92, 3);
    _display.printf("[%s]", modeStr.c_str());

    _display.drawLine(0, 14, 127, 14, SSD1306_WHITE);

    // 2. Body Status
    _display.setCursor(2, 18);
    _display.print("IP : ");
    _display.println(wifiConnected ? ipAddress : "Disconnected");

    _display.setCursor(2, 29);
    _display.printf("SIM: %s (CSQ:%d)\n", simStatus.c_str(), signalCsq);

    _display.setCursor(2, 40);
    _display.printf("SMS: %d Diterima\n", smsCount);

    _display.drawLine(0, 51, 127, 51, SSD1306_WHITE);

    // 3. Footer Bar
    _display.setCursor(2, 54);
    _display.print("Status: ");
    _display.print(wifiConnected ? "Gateway Online" : "Waiting WiFi...");

    _display.display();
}

void DisplayManager::showNewSMS(String senderPhone, String messageSnippet) {
    if (!_isAvailable) return;

    wakeUp(10000); // Nyalakan layar selama 10 detik

    _display.clearDisplay();
    _display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    _display.setTextSize(1);
    _display.setCursor(18, 4);
    _display.print(">> SMS BARU! <<");
    _display.drawLine(4, 14, 123, 14, SSD1306_WHITE);

    _display.setCursor(4, 18);
    _display.print("Dari: ");
    _display.println(senderPhone);

    _display.setCursor(4, 30);
    // Potong cuplikan isi pesan maksimal 35 karakter
    String snippet = messageSnippet.length() > 35 ? messageSnippet.substring(0, 35) + "..." : messageSnippet;
    _display.println(snippet);

    _display.setCursor(4, 52);
    _display.print("[Disimpan & Forward]");
    _display.display();
}

void DisplayManager::showPortalMode(String apSSID, String apIP) {
    if (!_isAvailable) return;

    wakeUp(300000); // Nyalakan layar selama mode portal (5 menit)

    _display.clearDisplay();
    _display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    _display.setTextSize(1);
    _display.setCursor(10, 4);
    _display.print("[ WEB CONFIG AP ]");
    _display.drawLine(4, 14, 123, 14, SSD1306_WHITE);

    _display.setCursor(4, 18);
    _display.print("SSID: ");
    _display.println(apSSID);

    _display.setCursor(4, 30);
    _display.print("IP  : ");
    _display.println(apIP);

    _display.setCursor(4, 44);
    _display.println("Buka di browser HP");
    _display.setCursor(4, 54);
    _display.println("Tahan 3s utk batal");
    _display.display();
}

void DisplayManager::showMessage(String title, String message, unsigned long wakeDurationMs) {
    if (!_isAvailable) return;

    wakeUp(wakeDurationMs);

    _display.clearDisplay();
    _display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    _display.setTextSize(1);
    _display.setCursor(6, 6);
    _display.println(title);
    _display.drawLine(4, 16, 123, 16, SSD1306_WHITE);

    _display.setCursor(6, 22);
    _display.println(message);
    _display.display();
}
