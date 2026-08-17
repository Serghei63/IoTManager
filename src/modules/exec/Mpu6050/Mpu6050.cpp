#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include <math.h>

class MpuCube : public IoTItem {
private:
    bool _debug = false;
    bool _isFirstRun = true;
    int _lastSide = 0;
    unsigned long _lastCheck = 0;
    
    int16_t ax, ay, az;
    int _address;       

    bool initMPU() {
        Wire.beginTransmission(_address);
        Wire.write(0x6B); // PWR_MGMT_1
        Wire.write(0x00); // Пробуждаем MPU6050
        if (Wire.endTransmission() != 0) return false;

        if (_debug) {
            Serial.printf("[MPU] Successfully initialized at address 0x%02X\n", _address);
        }
        return true;
    }

    bool readAccel() {
        Wire.beginTransmission(_address);
        Wire.write(0x3B); // ACCEL_XOUT_H
        
        if (Wire.endTransmission(true) != 0) {
            if (_debug) Serial.println(F("[MPU Error] I2C Bus Fail! Re-initializing MPU..."));
            initMPU();
            return false; 
        }
        
        Wire.requestFrom((uint8_t)_address, (uint8_t)6);
        if (Wire.available() >= 6) {
            ax = (Wire.read() << 8) | Wire.read();
            ay = (Wire.read() << 8) | Wire.read();
            az = (Wire.read() << 8) | Wire.read();
            return true; 
        }
        
        return false;
    }

    int detectSide() {
        if (!readAccel()) {
            return _lastSide; 
        }

        double x = (double)ax;
        double y = (double)ay;
        double z = (double)az;

        int currentSide = _lastSide;

        double magnitudeX = sqrt(y*y + z*z);
        double magnitudeY = sqrt(x*x + z*z);
        double magnitudeZ = sqrt(x*x + y*y);

        if (abs(z) > magnitudeZ) {
            currentSide = (z > 0) ? 1 : 2; 
        } 
        else if (abs(x) > magnitudeX) {
            currentSide = (x > 0) ? 3 : 4; 
        } 
        else if (abs(y) > magnitudeY) {
            currentSide = (y > 0) ? 5 : 6; 
        }

        return currentSide;
    }

public:
    MpuCube(String parameters) : IoTItem(parameters) {
        String addrStr;
        jsonRead(parameters, F("addr"), addrStr);
        _address = (int)strtol(addrStr.c_str(), NULL, 16);

        long dbgValue = 0;
        jsonRead(parameters, F("debug"), dbgValue);
        _debug = (dbgValue == 1);

        setInterval(0); 
    }

    void doByInterval() override {}

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;
            Wire.begin(); 
            initMPU();
        }

        // Опрос граней каждые 300 миллисекунд
        if (millis() - _lastCheck > 300) {
            _lastCheck = millis();
            
            int activeSide = detectSide();

            if (activeSide != _lastSide) {
                _lastSide = activeSide;
                value.valD = activeSide;
                
                if (_debug) {
                    Serial.printf("[MPU Side Changed] New Side: %d\n", activeSide);
                }
                
                // Генерируем событие для автоматизации в IoTmanager
                regEvent(String(activeSide), "MpuCube");
            }
        }
    }

    ~MpuCube() {}
};

void *getAPI_Mpu6050(String subtype, String param) {
    if (subtype == F("MpuCube")) {
        String addr;
        jsonRead(param, F("addr"), addr);
        
        if (addr == "") {
            scanI2C();
            return nullptr; 
        }

        return new MpuCube(param);
    }
    return nullptr;
}