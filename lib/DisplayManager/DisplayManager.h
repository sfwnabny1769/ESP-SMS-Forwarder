#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class DisplayManager {
private:
    Adafruit_SSD1306 _display;
    bool _isAvailable;
    bool _isDisplayOn;
    unsigned long _wakeUntil;
    unsigned long _timeoutMs;

    void drawSignalArea(int x, int y, int signalCsq, String simStatus, String regStatus);
    void drawSignalBars(int x, int y, int signalCsq);
    void drawCrossIcon(int x, int y);
    void drawLoadingCircle(int x, int y);
    void drawTelegramPlane(int x, int y);
    void drawGearIcon(int x, int y);

public:
    DisplayManager();

    bool begin(int sdaPin = 8, int sclPin = 9, uint8_t i2cAddr = 0x3C, unsigned long timeoutMs = 15000);
    
    // Power & Sleep Control
    void wakeUp(unsigned long customDurationMs = 0);
    void sleep();
    void update();
    bool isDisplayOn() const { return _isDisplayOn; }
    bool isAvailable() const { return _isAvailable; }

    // Tampilan Layar
    void showBootSplash();
    void showStatus(bool wifiConnected, String ipAddress, int signalCsq, String operatorName, String simStatus, String regStatus, int smsCount, bool telegramConfigured, bool serverSyncEnabled, float voltage = 0.0f, bool isSystemOk = true);
    void showNewSMS(String senderPhone, String messageSnippet);
    void showPortalMode(String apSSID, String apPassword, String apIP);
    void showMessage(String title, String message, unsigned long wakeDurationMs = 5000);
};

#endif // DISPLAY_MANAGER_H
