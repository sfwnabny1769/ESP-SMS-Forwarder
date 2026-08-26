#include "GSMManager.h"

GSMManager::GSMManager() {
    _serial = nullptr;
    _rxPin = 16;
    _txPin = 17;
    _baudRate = 9600;
    _state = GSM_STATE_UNINITIALIZED;
    _lastStateTimer = 0;
    _lastHealthCheck = 0;
    _lastRegistrationCheck = 0;
    _lastCregCode = -1;
    _lastSimStatus = "";
    _lastCsq = -1;
    _lastOperator = "";
    _hasAnnouncedRegistration = false;
}

bool GSMManager::begin(HardwareSerial* serialPort, int rxPin, int txPin, long baudRate) {
    _serial = serialPort;
    _rxPin = rxPin;
    _txPin = txPin;
    _baudRate = baudRate;

    _serial->begin(_baudRate, SERIAL_8N1, _rxPin, _txPin);
    
    Serial.println("\n[GSM Boot] ==================================================");
    Serial.println("[GSM Boot] Inisialisasi Serial GSM SIM800L (Baud: 9600)...");
    Serial.println("[GSM Boot] ==================================================");
    _state = GSM_STATE_CHECK_AT;
    _lastStateTimer = millis();

    return true;
}

String GSMManager::sendCommand(String command, uint32_t timeout) {
    if (!_serial) return "";

    // Clear input buffer
    while (_serial->available()) {
        _serial->read();
    }

    _serial->println(command);
    String response = "";
    uint32_t start = millis();

    while (millis() - start < timeout) {
        while (_serial->available()) {
            char c = _serial->read();
            response += c;
        }
        
        // Early break if response completes
        if (response.indexOf("OK\r") != -1 || 
            response.indexOf("ERROR\r") != -1 || 
            response.indexOf("+CMS ERROR") != -1 || 
            response.indexOf("+CME ERROR") != -1) {
            break;
        }
        delay(10);
    }

    return response;
}

bool GSMManager::waitForResponse(String target, uint32_t timeout) {
    uint32_t start = millis();
    String buffer = "";
    while (millis() - start < timeout) {
        while (_serial && _serial->available()) {
            char c = _serial->read();
            buffer += c;
            if (buffer.indexOf(target) != -1) {
                return true;
            }
        }
        delay(10);
    }
    return false;
}

int GSMManager::getRegistrationCode() {
    String resp = sendCommand("AT+CREG?", 1500);
    int idx = resp.indexOf("+CREG:");
    if (idx != -1) {
        String sub = resp.substring(idx + 6);
        int commaIdx = sub.indexOf(",");
        if (commaIdx != -1) {
            return sub.substring(commaIdx + 1, commaIdx + 2).toInt();
        }
    }
    return -1;
}

bool GSMManager::isNetworkRegistered() {
    int code = getRegistrationCode();
    return (code == 1 || code == 5);
}

int GSMManager::getSignalDbm() {
    int csq = getSignal();
    if (csq == 0) return -113;
    if (csq == 1) return -111;
    if (csq >= 2 && csq <= 30) return -113 + (csq * 2);
    if (csq == 31) return -51;
    return -999; // Unknown
}

String GSMManager::getSignalQualityText() {
    int csq = getSignal();
    if (csq >= 20 && csq <= 31) return "Sangat Baik (Excellent)";
    if (csq >= 15 && csq < 20) return "Baik (Good)";
    if (csq >= 10 && csq < 15) return "Cukup (Moderate)";
    if (csq >= 1 && csq < 10) return "Lemah (Marginal)";
    return "Tidak Ada Sinyal / Belum Terdeteksi";
}

void GSMManager::logStatusSummary() {
    String sim = getSIMStatus();
    int csq = getSignal();
    int dbm = getSignalDbm();
    String reg = getRegistrationStatus();
    String op = getOperator();

    Serial.println("\n--------------------------------------------------");
    Serial.println("  [DIAGNOSTIK MODUL GSM SIM800L & KARTU SIM]");
    Serial.println("--------------------------------------------------");
    Serial.printf("  1. Keadaan SIM Card     : %s\n", sim.c_str());
    Serial.printf("  2. Kuat Sinyal CSQ      : %d/31 (%d dBm) - %s\n", csq, dbm, getSignalQualityText().c_str());
    Serial.printf("  3. Registrasi Jaringan  : %s\n", reg.c_str());
    Serial.printf("  4. Operator Seluler     : %s\n", op.c_str());
    Serial.println("--------------------------------------------------\n");
}

void GSMManager::update() {
    if (!_serial) return;

    unsigned long currentMillis = millis();

    // --- Non-blocking State Machine ---
    switch (_state) {
        case GSM_STATE_UNINITIALIZED:
            break;

        case GSM_STATE_CHECK_AT: {
            String resp = sendCommand("AT", 1000);
            if (resp.indexOf("OK") != -1) {
                Serial.println("[GSM Boot] Modul SIM800L merespon AT (OK).");
                _state = GSM_STATE_CONFIGURING;
                _lastStateTimer = currentMillis;
            } else if (currentMillis - _lastStateTimer > 8000) {
                Serial.println("[GSM Warning] Menunggu respon AT dari SIM800L... Cek kabel TX/RX & Power Supply.");
                _lastStateTimer = currentMillis;
            }
            break;
        }

        case GSM_STATE_CONFIGURING: {
            sendCommand("ATE0", 1000); // Disable Echo

            // 1. Memeriksa keadaan SIM Card
            String simResp = getSIMStatus();
            _lastSimStatus = simResp;
            if (simResp == "READY") {
                Serial.println("\n[GSM SIM] ==================================================");
                Serial.println("[GSM SIM] Status Kartu SIM: READY (SIM terpasang & aktif)");
                Serial.println("[GSM SIM] ==================================================");
            } else {
                Serial.printf("\n[GSM SIM Warning] Status SIM: %s (Periksa pemasangan kartu SIM / PIN)\n", simResp.c_str());
            }

            // 2. Konfigurasi SMS Mode & Storage SIM Card
            sendCommand("AT+CMGF=1", 1000);                    // SMS Text Mode
            sendCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000); // Set SMS storage ke memori Kartu SIM (SM)
            sendCommand("AT+CNMI=2,1,0,0,0", 1000);            // Notifikasi index SMS masuk (+CMTI: "SM",x)

            Serial.println("[GSM Boot] Konfigurasi SMS Text Mode & Penyimpanan Memori SIM (SM) Selesai.");
            Serial.println("[GSM Boot] Memulai pemantauan registrasi jaringan dan sinyal seluler...\n");

            _state = GSM_STATE_READY;
            _lastHealthCheck = currentMillis;
            _lastRegistrationCheck = 0; // Trigger immediate registration check
            break;
        }

        case GSM_STATE_READY: {
            // Periodic network registration and signal status monitoring (every 5-10s)
            if (currentMillis - _lastRegistrationCheck >= 5000) {
                _lastRegistrationCheck = currentMillis;

                int cregCode = getRegistrationCode();
                int currentCsq = getSignal();

                // Log state transitions in registration
                if (cregCode != _lastCregCode || !_hasAnnouncedRegistration) {
                    _lastCregCode = cregCode;

                    if (cregCode == 1) { // +CREG: 0,1 (Registered Home)
                        _hasAnnouncedRegistration = true;
                        String op = getOperator();
                        int dbm = (currentCsq >= 2 && currentCsq <= 30) ? (-113 + (currentCsq * 2)) : -113;

                        Serial.println("\n================================================================================");
                        Serial.println("  [GSM NOTIFIKASI] >> TERHUBUNG KE SINYAL JARINGAN! (+CREG: 0,1) <<");
                        Serial.println("================================================================================");
                        Serial.println("  ✓ Status Registrasi : Terhubung Jaringan Utama (Registered Home)");
                        Serial.printf("  ✓ Kuat Sinyal CSQ   : %d/31 (%d dBm) [%s]\n", currentCsq, dbm, getSignalQualityText().c_str());
                        Serial.printf("  ✓ Operator Provider : %s\n", op.c_str());
                        Serial.println("================================================================================\n");

                        // Otomatis tarik pesan yang tersimpan di memori SIM saat pertama kali terhubung ke jaringan
                        Serial.println("[GSM Auto-Sync] Memeriksa apakah ada SMS yang tersimpan di kartu SIM...");
                        syncStoredSMS(true);

                    } else if (cregCode == 5) { // +CREG: 0,5 (Registered Roaming)
                        _hasAnnouncedRegistration = true;
                        String op = getOperator();
                        Serial.println("\n================================================================================");
                        Serial.println("  [GSM NOTIFIKASI] >> TERHUBUNG KE SINYAL JARINGAN (Roaming) (+CREG: 0,5) <<");
                        Serial.printf("  ✓ Status: Roaming | Kuat Sinyal: %d/31 | Operator: %s\n", currentCsq, op.c_str());
                        Serial.println("================================================================================\n");

                        syncStoredSMS(true);

                    } else if (cregCode == 2) { // +CREG: 0,2 (Searching)
                        Serial.printf("[GSM Registrasi] +CREG: 0,2 - Sedang mencari sinyal jaringan provider... (CSQ: %d/31)\n", currentCsq);
                    } else if (cregCode == 0) { // +CREG: 0,0 (Not registered)
                        Serial.printf("[GSM Registrasi] +CREG: 0,0 - Belum terdaftar ke jaringan seluler. (CSQ: %d/31)\n", currentCsq);
                    } else if (cregCode == 3) { // +CREG: 0,3 (Registration Denied)
                        Serial.println("[GSM Warning] +CREG: 0,3 - Registrasi kartu SIM DITOLAK oleh provider (Registration Denied)!");
                    } else {
                        Serial.printf("[GSM Registrasi] +CREG status: %d (CSQ: %d/31)\n", cregCode, currentCsq);
                    }
                }
            }

            // Periodic health check every 45s
            if (currentMillis - _lastHealthCheck >= 45000) {
                _lastHealthCheck = currentMillis;
                String resp = sendCommand("AT", 1000);
                if (resp.indexOf("OK") == -1) {
                    Serial.println("[GSM Warning] Modul SIM800L tidak merespon! Memulai re-inisialisasi...");
                    _state = GSM_STATE_CHECK_AT;
                    _lastStateTimer = currentMillis;
                    _hasAnnouncedRegistration = false;
                    _lastCregCode = -1;
                    return;
                }
            }
            break;
        }

        case GSM_STATE_ERROR:
            if (currentMillis - _lastStateTimer >= 5000) {
                Serial.println("[GSM Recovery] Mencoba memulihkan modul dari error state...");
                _state = GSM_STATE_CHECK_AT;
                _lastStateTimer = currentMillis;
            }
            break;
    }

    // --- Stream Incoming Unsolicited Data & SMS Detector ---
    while (_serial->available()) {
        String line = _serial->readStringUntil('\n');
        line.trim();

        // 1. Detect SIM800L Power Drop / Hardware Restart (RDY)
        if (line.indexOf("RDY") != -1) {
            Serial.println("[GSM Warning] SIM800L Power Drop / Hardware Restart Terdeteksi! (RDY)");
            _state = GSM_STATE_CHECK_AT; // Trigger automatic re-initialization
            _lastStateTimer = currentMillis;
            _hasAnnouncedRegistration = false;
            _lastCregCode = -1;
        }
        // 2. Detect Power Voltage Dip Alarm
        else if (line.indexOf("UNDER-VOLTAGE") != -1) {
            Serial.println("[GSM Alarm] KRITIKAL: Catu daya SIM800L drop di bawah batas minimal! (UNDER-VOLTAGE)");
        }
        // 3. Detect Normal Power Down
        else if (line.indexOf("POWER DOWN") != -1) {
            Serial.println("[GSM Alarm] KRITIKAL: SIM800L Mengalami Shutdown! (POWER DOWN)");
            _state = GSM_STATE_CHECK_AT;
            _hasAnnouncedRegistration = false;
            _lastCregCode = -1;
        }
        // 4. Detect Network Registration URC (+CREG: 1 or +CREG: 5)
        else if (line.indexOf("+CREG:") != -1) {
            Serial.printf("[GSM Registrasi Event] %s\n", line.c_str());
            _lastRegistrationCheck = 0; // Trigger immediate evaluation
        }
        // 5. Detect SMS notification index: +CMTI: "SM",3
        else if (line.indexOf("+CMTI:") != -1) {
            int commaIdx = line.indexOf(",", line.indexOf("+CMTI:"));
            if (commaIdx != -1) {
                int smsIndex = line.substring(commaIdx + 1).toInt();
                Serial.printf("\n[GSM Notifikasi] SMS Masuk Baru Terdeteksi pada Memori SIM Index #%d!\n", smsIndex);
                _pendingSMSIndexes.push(smsIndex);
            }
        }
        // 6. Detect Direct SMS Stream: +CMT: "+62812...",,"26/08/26,21:40:00+28"
        else if (line.indexOf("+CMT:") != -1) {
            Serial.println("\n[GSM Notifikasi] Direct SMS (+CMT) Terdeteksi dari Serial!");
            SMSMessage msg = parseCMTResponse(line);
            if (msg.phone.length() > 0 && msg.message.length() > 0) {
                _smsQueue.push(msg);
            }
        }
    }

    // Process queued SMS indexes into SMSMessage objects
    if (!_pendingSMSIndexes.empty() && _state == GSM_STATE_READY) {
        int targetIdx = _pendingSMSIndexes.front();
        _pendingSMSIndexes.pop();

        delay(150);

        String raw = sendCommand("AT+CMGR=" + String(targetIdx), 3000);

        if (raw.indexOf("+CMGR:") != -1) {
            SMSMessage msg = parseCMGRResponse(targetIdx, raw);
            if (msg.phone.length() > 0) {
                Serial.println("\n[GSM SMS Diterima]");
                Serial.printf("  Index   : #%d\n", msg.index);
                Serial.printf("  Pengirim: %s\n", msg.phone.c_str());
                Serial.printf("  Waktu   : %s\n", msg.datetime.c_str());
                Serial.printf("  Pesan   : %s\n\n", msg.message.c_str());

                _smsQueue.push(msg);
            } else {
                Serial.println("[GSM Warning] Parsing SMS gagal. Melewati index.");
            }
        } else {
            Serial.println("[GSM Warning] Gagal membaca SMS dari index memory.");
        }
    }
}

SMSMessage GSMManager::parseCMGRResponse(int index, String raw) {
    SMSMessage sms;
    sms.index = index;
    sms.phone = "";
    sms.message = "";
    sms.datetime = "";

    int cmgrPos = raw.indexOf("+CMGR:");
    if (cmgrPos == -1) {
        return sms;
    }

    // Locate quotes relative to +CMGR:
    int firstQuote  = raw.indexOf("\"", cmgrPos);
    int secondQuote = raw.indexOf("\"", firstQuote + 1);
    int thirdQuote  = raw.indexOf("\"", secondQuote + 1);
    int fourthQuote = raw.indexOf("\"", thirdQuote + 1);

    if (thirdQuote != -1 && fourthQuote != -1) {
        sms.phone = raw.substring(thirdQuote + 1, fourthQuote);
        sms.phone.trim();
    }

    // Locate timestamp quotes
    int fifthQuote = raw.indexOf("\"", fourthQuote + 1);
    int sixthQuote = raw.indexOf("\"", fifthQuote + 1);
    int seventhQuote = raw.indexOf("\"", sixthQuote + 1);
    int eighthQuote = raw.indexOf("\"", seventhQuote + 1);

    String rawTime = "";
    if (seventhQuote != -1 && eighthQuote != -1) {
        rawTime = raw.substring(seventhQuote + 1, eighthQuote);
    } else if (fifthQuote != -1 && sixthQuote != -1) {
        rawTime = raw.substring(fifthQuote + 1, sixthQuote);
    }

    if (rawTime.length() >= 17) {
        String year = "20" + rawTime.substring(0, 2);
        String month = rawTime.substring(3, 5);
        String day = rawTime.substring(6, 8);
        String time = rawTime.substring(9, 17);
        sms.datetime = year + "-" + month + "-" + day + " " + time;
    }

    // Parse message body text (line after header)
    int headerEnd = raw.indexOf("\n", cmgrPos);
    if (headerEnd != -1) {
        String body = raw.substring(headerEnd + 1);
        int okIdx = body.indexOf("OK");
        if (okIdx != -1) {
            body = body.substring(0, okIdx);
        }
        body.trim();
        sms.message = body;
    }

    return sms;
}

SMSMessage GSMManager::parseCMTResponse(String headerLine) {
    SMSMessage sms;
    sms.index = -1; // Direct incoming SMS stream
    sms.phone = "";
    sms.message = "";
    sms.datetime = "";

    // Header example: +CMT: "+62812345678",,"26/08/26,21:40:00+28"
    int q1 = headerLine.indexOf("\"");
    int q2 = headerLine.indexOf("\"", q1 + 1);
    if (q1 != -1 && q2 != -1) {
        sms.phone = headerLine.substring(q1 + 1, q2);
        sms.phone.trim();
    }

    int q3 = headerLine.indexOf("\"", q2 + 1);
    int q4 = headerLine.indexOf("\"", q3 + 1);
    int q5 = headerLine.indexOf("\"", q4 + 1);
    int q6 = headerLine.indexOf("\"", q5 + 1);

    String rawTime = "";
    if (q5 != -1 && q6 != -1) {
        rawTime = headerLine.substring(q5 + 1, q6);
    } else if (q3 != -1 && q4 != -1) {
        rawTime = headerLine.substring(q3 + 1, q4);
    }

    if (rawTime.length() >= 17) {
        String year = "20" + rawTime.substring(0, 2);
        String month = rawTime.substring(3, 5);
        String day = rawTime.substring(6, 8);
        String time = rawTime.substring(9, 17);
        sms.datetime = year + "-" + month + "-" + day + " " + time;
    }

    // Read next line for message body
    unsigned long startWait = millis();
    while (!_serial->available() && (millis() - startWait < 1500)) {
        delay(10);
    }

    if (_serial->available()) {
        String body = _serial->readStringUntil('\n');
        body.trim();
        sms.message = body;
    }

    Serial.println("\n[GSM Direct SMS Diterima]");
    Serial.printf("  Pengirim: %s\n", sms.phone.c_str());
    Serial.printf("  Waktu   : %s\n", sms.datetime.c_str());
    Serial.printf("  Pesan   : %s\n\n", sms.message.c_str());

    return sms;
}

int GSMManager::syncStoredSMS(bool deleteAfterRead) {
    Serial.println("\n[GSM SIM Storage] ==================================================");
    Serial.println("[GSM SIM Storage] Membaca semua pesan yang tersimpan di kartu SIM...");
    Serial.println("[GSM SIM Storage] Mengirim perintah AT+CMGL=\"ALL\"...");

    // Pastikan storage mengarah ke memori SIM
    sendCommand("AT+CMGF=1", 1000);
    sendCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"", 1000);

    String rawList = sendCommand("AT+CMGL=\"ALL\"", 6000);

    if (rawList.indexOf("+CMGL:") == -1) {
        Serial.println("[GSM SIM Storage] Memori kartu SIM kosong. Tidak ada pesan tersimpan.");
        Serial.println("[GSM SIM Storage] ==================================================\n");
        return 0;
    }

    int count = 0;
    int currentPos = 0;

    while (true) {
        int cmglPos = rawList.indexOf("+CMGL:", currentPos);
        if (cmglPos == -1) break;

        // Parse Index: +CMGL: <index>, ...
        int colonPos = rawList.indexOf(":", cmglPos);
        int commaPos = rawList.indexOf(",", colonPos);
        if (commaPos == -1) break;

        int index = rawList.substring(colonPos + 1, commaPos).toInt();

        // Cari batas akhir baris header
        int headerEnd = rawList.indexOf("\n", cmglPos);
        if (headerEnd == -1) break;

        String headerLine = rawList.substring(cmglPos, headerEnd);

        // Cari nomor pengirim (phone)
        int quote1 = headerLine.indexOf("\"");
        int quote2 = headerLine.indexOf("\"", quote1 + 1);
        int quote3 = headerLine.indexOf("\"", quote2 + 1);
        int quote4 = headerLine.indexOf("\"", quote3 + 1);

        String phone = "";
        if (quote3 != -1 && quote4 != -1) {
            phone = headerLine.substring(quote3 + 1, quote4);
            phone.trim();
        }

        // Cari timestamp
        int quote5 = headerLine.indexOf("\"", quote4 + 1);
        int quote6 = headerLine.indexOf("\"", quote5 + 1);
        int quote7 = headerLine.indexOf("\"", quote6 + 1);
        int quote8 = headerLine.indexOf("\"", quote7 + 1);

        String rawTime = "";
        if (quote7 != -1 && quote8 != -1) {
            rawTime = headerLine.substring(quote7 + 1, quote8);
        } else if (quote5 != -1 && quote6 != -1) {
            rawTime = headerLine.substring(quote5 + 1, quote6);
        }

        String datetime = "";
        if (rawTime.length() >= 17) {
            String year = "20" + rawTime.substring(0, 2);
            String month = rawTime.substring(3, 5);
            String day = rawTime.substring(6, 8);
            String time = rawTime.substring(9, 17);
            datetime = year + "-" + month + "-" + day + " " + time;
        }

        // Cari isi body pesan hingga baris +CMGL: berikutnya atau OK
        int nextCmgl = rawList.indexOf("+CMGL:", headerEnd + 1);
        int okPos = rawList.indexOf("OK", headerEnd + 1);

        int bodyEnd = rawList.length();
        if (nextCmgl != -1 && (okPos == -1 || nextCmgl < okPos)) {
            bodyEnd = nextCmgl;
        } else if (okPos != -1) {
            bodyEnd = okPos;
        }

        String body = rawList.substring(headerEnd + 1, bodyEnd);
        body.trim();

        if (phone.length() > 0 && body.length() > 0) {
            SMSMessage msg;
            msg.index = index;
            msg.phone = phone;
            msg.message = body;
            msg.datetime = datetime;

            Serial.printf("  [SIM SMS #%d] Dari: %s | Waktu: %s\n", index, phone.c_str(), datetime.c_str());
            Serial.printf("  [Isi Pesan]   : %s\n", body.c_str());

            _smsQueue.push(msg);
            count++;

            // Jika deleteAfterRead aktif, hapus dari SIM card agar memori tidak penuh
            if (deleteAfterRead) {
                deleteSMS(index);
                Serial.printf("  [Hapus Memori]: SMS #%d berhasil dihapus dari kartu SIM.\n", index);
            }
        }

        currentPos = (nextCmgl != -1) ? nextCmgl : (okPos != -1 ? okPos + 2 : rawList.length());
        if (nextCmgl == -1) break;
    }

    Serial.printf("[GSM SIM Storage] Selesai! Berhasil menarik %d pesan tersimpan dari kartu SIM.\n", count);
    Serial.println("[GSM SIM Storage] ==================================================\n");

    return count;
}

bool GSMManager::hasNewSMS() {
    return !_smsQueue.empty();
}

SMSMessage GSMManager::readSMS() {
    if (_smsQueue.empty()) {
        SMSMessage emptySMS = { -1, "", "", "" };
        return emptySMS;
    }

    SMSMessage sms = _smsQueue.front();
    _smsQueue.pop();
    return sms;
}

bool GSMManager::deleteSMS(int index) {
    String cmd = "AT+CMGD=" + String(index);
    String resp = sendCommand(cmd, 2000);
    bool success = (resp.indexOf("OK") != -1);

    if (!success) {
        Serial.printf("[GSM] Gagal menghapus SMS #%d dari kartu SIM.\n", index);
    }
    return success;
}

bool GSMManager::sendSMS(String number, String message) {
    String cmdNumber = "AT+CMGS=\"" + number + "\"";
    _serial->println(cmdNumber);
    delay(300);

    _serial->print(message);
    _serial->write(26);

    return waitForResponse("OK", 5000);
}

int GSMManager::getSignal() {
    String resp = sendCommand("AT+CSQ", 1500);
    int csq = 0;
    int idx = resp.indexOf("+CSQ:");
    if (idx != -1) {
        String sub = resp.substring(idx + 5);
        sub.trim();
        int commaIdx = sub.indexOf(",");
        if (commaIdx != -1) {
            csq = sub.substring(0, commaIdx).toInt();
        }
    }
    return csq;
}

String GSMManager::getOperator() {
    String resp = sendCommand("AT+COPS?", 2000);
    int idx = resp.indexOf("\"");
    if (idx != -1) {
        int endIdx = resp.indexOf("\"", idx + 1);
        if (endIdx != -1) {
            return resp.substring(idx + 1, endIdx);
        }
    }
    return "UNKNOWN";
}

String GSMManager::getSIMStatus() {
    String resp = sendCommand("AT+CPIN?", 1500);
    int idx = resp.indexOf("+CPIN:");
    if (idx != -1) {
        String sub = resp.substring(idx + 6);
        sub.trim();
        int okIdx = sub.indexOf("\r");
        if (okIdx != -1) {
            sub = sub.substring(0, okIdx);
        }
        sub.trim();
        return sub;
    }
    return "UNKNOWN";
}

String GSMManager::getRegistrationStatus() {
    int stat = getRegistrationCode();
    switch (stat) {
        case 1: return "Registered Home (+CREG: 0,1)";
        case 2: return "Searching (+CREG: 0,2)";
        case 3: return "Registration Denied (+CREG: 0,3)";
        case 4: return "Unknown (+CREG: 0,4)";
        case 5: return "Registered Roaming (+CREG: 0,5)";
        case 0: return "Not Registered (+CREG: 0,0)";
        default: return "UNKNOWN";
    }
}

String GSMManager::executeCustomAT(String command, uint32_t timeout) {
    command.trim();
    return sendCommand(command, timeout);
}
