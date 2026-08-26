#ifndef GSM_MANAGER_H
#define GSM_MANAGER_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <queue>
#include <vector>

struct SMSMessage {
    int index;
    String phone;
    String message;
    String datetime;
};

enum GSMState {
    GSM_STATE_UNINITIALIZED,
    GSM_STATE_CHECK_AT,
    GSM_STATE_CONFIGURING,
    GSM_STATE_READY,
    GSM_STATE_ERROR
};

class GSMManager {
private:
    HardwareSerial* _serial;
    int _rxPin;
    int _txPin;
    long _baudRate;

    GSMState _state;
    unsigned long _lastStateTimer;
    unsigned long _lastHealthCheck;
    unsigned long _lastRegistrationCheck;
    
    // Status tracking for state change logging
    int _lastCregCode;
    String _lastSimStatus;
    int _lastCsq;
    String _lastOperator;
    bool _hasAnnouncedRegistration;

    std::queue<int> _pendingSMSIndexes;
    std::queue<SMSMessage> _smsQueue;

    // Internal AT Command helpers
    String sendCommand(String command, uint32_t timeout = 2000);
    bool waitForResponse(String target, uint32_t timeout = 2000);

    // Parsing helpers
    SMSMessage parseCMGRResponse(int index, String rawResponse);
    SMSMessage parseCMTResponse(String headerLine);

public:
    GSMManager();

    bool begin(HardwareSerial* serialPort = &Serial2, int rxPin = 16, int txPin = 17, long baudRate = 9600);
    void update();
    bool hasNewSMS();
    SMSMessage readSMS();
    bool deleteSMS(int index);
    bool sendSMS(String number, String message);

    // Diagnostics & Signal/Registration monitoring
    int getSignal();
    int getSignalDbm();
    String getSignalQualityText();
    String getOperator();
    String getSIMStatus();
    String getRegistrationStatus();
    int getRegistrationCode();
    bool isNetworkRegistered();
    void logStatusSummary();

    // Pull / Sync SMS stored on SIM card memory
    int syncStoredSMS(bool deleteAfterRead = true);

    // Remote AT Command execution
    String executeCustomAT(String command, uint32_t timeout = 3000);
};

#endif // GSM_MANAGER_H
