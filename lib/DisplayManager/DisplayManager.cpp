#include "DisplayManager.h"

DisplayManager::DisplayManager() 
    : _display(128, 64, &Wire, -1), _isAvailable(false), _isDisplayOn(false), _wakeUntil(0), _timeoutMs(15000) {}

bool DisplayManager::begin(int sdaPin, int sclPin, uint8_t i2cAddr, unsigned long timeoutMs) {
    _timeoutMs = timeoutMs;
    
    // Inisialisasi I2C Wire dengan Pin khusus ESP32-S3
    Wire.begin(sdaPin, sclPin);

    // I2C Safe Probe
    Wire.beginTransmission(i2cAddr);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[OLED] Display SSD1306 TIDAK terdeteksi pada 0x%02X (SDA:%d, SCL:%d).\n", i2cAddr, sdaPin, sclPin);
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

    if (_isDisplayOn && _timeoutMs > 0 && millis() >= _wakeUntil) {
        sleep();
    }
}

// 1. Gambar Bar Sinyal CSQ (4 level bertingkat)
void DisplayManager::drawSignalBars(int x, int y, int signalCsq) {
    int bars = 0;
    if (signalCsq >= 20) bars = 4;
    else if (signalCsq >= 15) bars = 3;
    else if (signalCsq >= 10) bars = 2;
    else if (signalCsq > 0)  bars = 1;

    for (int i = 0; i < 4; i++) {
        int barHeight = (i + 1) * 2 + 2; // Tinggi bar: 4, 6, 8, 10
        int barY = y + (10 - barHeight);
        if (i < bars) {
            _display.fillRect(x + (i * 3), barY, 2, barHeight, SSD1306_WHITE);
        } else {
            _display.drawPixel(x + (i * 3), y + 9, SSD1306_WHITE);
        }
    }
}

// 2. Gambar Ikon Silang (SIM Tidak Terpasang / UNKNOWN)
void DisplayManager::drawCrossIcon(int x, int y) {
    _display.drawRect(x, y + 1, 10, 10, SSD1306_WHITE);
    _display.drawLine(x + 2, y + 3, x + 7, y + 8, SSD1306_WHITE);
    _display.drawLine(x + 7, y + 3, x + 2, y + 8, SSD1306_WHITE);
}

// 3. Gambar Lingkaran Loading Berputar (SIM Ready tapi Searching Sinyal)
void DisplayManager::drawLoadingCircle(int x, int y) {
    int cx = x + 4;
    int cy = y + 5;
    _display.drawCircle(cx, cy, 4, SSD1306_WHITE);

    // Animasi titik putar 8 frame
    int frame = (millis() / 120) % 8;
    const int8_t cosOffsets[8] = { 4,  3,  0, -3, -4, -3,  0,  3 };
    const int8_t sinOffsets[8] = { 0,  3,  4,  3,  0, -3, -4, -3 };
    int dotX = cx + cosOffsets[frame];
    int dotY = cy + sinOffsets[frame];
    _display.fillCircle(dotX, dotY, 1, SSD1306_WHITE);
}

// 4. Gambar Ikon Pesawat Kertas Telegram (Tilted Top-Right)
void DisplayManager::drawTelegramPlane(int x, int y) {
    _display.drawLine(x + 8, y + 1, x + 1, y + 4, SSD1306_WHITE); // Tepi atas
    _display.drawLine(x + 8, y + 1, x + 4, y + 8, SSD1306_WHITE); // Tepi kanan
    _display.drawLine(x + 8, y + 1, x + 4, y + 4, SSD1306_WHITE); // Tulang tengah
    _display.drawLine(x + 1, y + 4, x + 4, y + 4, SSD1306_WHITE); // Sayap kiri
    _display.drawLine(x + 4, y + 8, x + 4, y + 4, SSD1306_WHITE); // Sayap kanan
    _display.drawPixel(x + 2, y + 5, SSD1306_WHITE);
}

// 5. Gambar Ikon Kubus 3D Laravel (Isometric Favicon Cube 8x8)
void DisplayManager::drawLaravelIcon(int x, int y) {
    // Hexagon luar isometric
    _display.drawLine(x + 4, y, x + 8, y + 2, SSD1306_WHITE);     // Top -> Top-Right
    _display.drawLine(x + 8, y + 2, x + 8, y + 6, SSD1306_WHITE); // Top-Right -> Bottom-Right
    _display.drawLine(x + 8, y + 6, x + 4, y + 8, SSD1306_WHITE); // Bottom-Right -> Bottom
    _display.drawLine(x + 4, y + 8, x, y + 6, SSD1306_WHITE);     // Bottom -> Bottom-Left
    _display.drawLine(x, y + 6, x, y + 2, SSD1306_WHITE);         // Bottom-Left -> Top-Left
    _display.drawLine(x, y + 2, x + 4, y, SSD1306_WHITE);         // Top-Left -> Top

    // Tulang 3D dalam (Origami Cube)
    _display.drawLine(x + 4, y + 4, x + 4, y, SSD1306_WHITE);     // Center -> Top
    _display.drawLine(x + 4, y + 4, x, y + 6, SSD1306_WHITE);     // Center -> Bottom-Left
    _display.drawLine(x + 4, y + 4, x + 8, y + 6, SSD1306_WHITE); // Center -> Bottom-Right
}

// 6. Gambar Ikon Gear Mini ⚙️ (7x7)
void DisplayManager::drawGearIcon(int x, int y) {
    _display.drawCircle(x + 3, y + 3, 2, SSD1306_WHITE);
    _display.drawPixel(x + 3, y, SSD1306_WHITE);     // Gigi atas
    _display.drawPixel(x + 3, y + 6, SSD1306_WHITE); // Gigi bawah
    _display.drawPixel(x, y + 3, SSD1306_WHITE);     // Gigi kiri
    _display.drawPixel(x + 6, y + 3, SSD1306_WHITE); // Gigi kanan
    _display.drawPixel(x + 1, y + 1, SSD1306_WHITE); // Sudut kiri atas
    _display.drawPixel(x + 5, y + 1, SSD1306_WHITE); // Sudut kanan atas
    _display.drawPixel(x + 1, y + 5, SSD1306_WHITE); // Sudut kiri bawah
    _display.drawPixel(x + 5, y + 5, SSD1306_WHITE); // Sudut kanan bawah
    _display.drawPixel(x + 3, y + 3, SSD1306_BLACK); // Lubang as tengah
}

// 7. Selector Area Sinyal
void DisplayManager::drawSignalArea(int x, int y, int signalCsq, String simStatus, String regStatus) {
    bool isSimReady = (simStatus == "READY");
    bool isRegistered = isSimReady && (regStatus.indexOf("Registered") != -1 || signalCsq > 0);

    if (!isSimReady) {
        drawCrossIcon(x, y);
    } else if (!isRegistered || signalCsq <= 0 || signalCsq == 99) {
        drawLoadingCircle(x, y);
    } else {
        drawSignalBars(x, y, signalCsq);
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

void DisplayManager::showStatus(bool wifiConnected, String ipAddress, int signalCsq, String operatorName, String simStatus, String regStatus, int smsCount, bool telegramConfigured, bool serverSyncEnabled, bool laravelConnected, float voltage, bool isSystemOk) {
    if (!_isAvailable || !_isDisplayOn) return;

    _display.clearDisplay();

    // =========================================================================
    // 1. HEADER BAR: [GSM Provider] | [✈️ Telegram] | [🔶 Laravel]
    // =========================================================================
    
    // A. Sinyal GSM / SIM di Kiri (X: 1)
    drawSignalArea(1, 2, signalCsq, simStatus, regStatus);

    _display.setTextSize(1);
    _display.setCursor(14, 3);
    if (simStatus != "READY") {
        _display.print("NO-SIM");
    } else if (regStatus.indexOf("Registered") == -1 && signalCsq <= 0) {
        _display.print("SEARCH");
    } else {
        String shortOp = operatorName.length() > 4 ? operatorName.substring(0, 4) : operatorName;
        _display.print(shortOp.length() > 0 ? shortOp : "GSM");
    }

    // B. Telegram Cloud API di Tengah (X: 52)
    drawTelegramPlane(52, 2);
    _display.setCursor(63, 3);
    if (!telegramConfigured) {
        _display.print("OFF");
    } else if (!wifiConnected) {
        _display.print("NC");
    } else {
        _display.print("OK");
    }

    // C. Laravel REST API di Kanan (X: 89)
    drawLaravelIcon(89, 2);
    _display.setCursor(101, 3);
    if (!serverSyncEnabled) {
        _display.print("OFF");
    } else if (!wifiConnected) {
        _display.print("NC");
    } else if (laravelConnected) {
        _display.print("OK");
    } else {
        _display.print("ERR");
    }

    _display.drawLine(0, 14, 127, 14, SSD1306_WHITE);

    // =========================================================================
    // 2. BODY STATUS
    // =========================================================================
    _display.setCursor(2, 18);
    _display.print("IP : ");
    _display.println(wifiConnected ? ipAddress : "Disconnected");

    _display.setCursor(2, 29);
    _display.printf("SIM: %s (CSQ:%d)\n", simStatus.c_str(), signalCsq);

    _display.setCursor(2, 40);
    _display.printf("SMS: %d Diterima\n", smsCount);

    _display.drawLine(0, 51, 127, 51, SSD1306_WHITE);

    // =========================================================================
    // 3. FOOTER BAR (⚙️OK  4.18V  dd:hh:mm:ss)
    // =========================================================================
    drawGearIcon(1, 54);
    _display.setCursor(10, 54);
    _display.print(isSystemOk ? "OK" : "ERR");

    // Tegangan / Voltage
    _display.setCursor(26, 54);
    if (voltage > 1.0f) {
        _display.printf("%.2fV", voltage);
    } else {
        _display.print("V:--");
    }

    // Uptime Sistem (Format Presisi dd:hh:mm:ss)
    unsigned long totalSec = millis() / 1000;
    unsigned long days = totalSec / 86400;
    unsigned long hours = (totalSec % 86400) / 3600;
    unsigned long minutes = (totalSec % 3600) / 60;
    unsigned long seconds = totalSec % 60;

    _display.setCursor(61, 54);
    _display.printf("%02lu:%02lu:%02lu:%02lu", days, hours, minutes, seconds);

    _display.display();
}

void DisplayManager::showNewSMS(String senderPhone, String messageSnippet) {
    if (!_isAvailable) return;

    wakeUp(10000); // 10 detik

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
    String snippet = messageSnippet.length() > 35 ? messageSnippet.substring(0, 35) + "..." : messageSnippet;
    _display.println(snippet);

    _display.setCursor(4, 52);
    _display.print("[Disimpan & Forward]");
    _display.display();
}

void DisplayManager::showPortalMode(String apSSID, String apPassword, String apIP) {
    if (!_isAvailable) return;

    wakeUp(300000); // 5 menit

    _display.clearDisplay();
    _display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

    _display.setTextSize(1);
    _display.setCursor(14, 4);
    _display.print("[ CONFIG PORTAL ]");
    _display.drawLine(4, 14, 123, 14, SSD1306_WHITE);

    _display.setCursor(4, 17);
    _display.print("SSID: ");
    _display.println(apSSID);

    _display.setCursor(4, 28);
    _display.print("PASS: ");
    _display.println(apPassword);

    _display.setCursor(4, 39);
    _display.print("IP  : ");
    _display.println(apIP);

    _display.drawLine(4, 49, 123, 49, SSD1306_WHITE);
    _display.setCursor(10, 53);
    _display.println("Hubungkan HP ke AP");
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
