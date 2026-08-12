#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include <math.h>

// Глобальный флаг для ISR
volatile bool g_mpuCubeInterrupted = false;

void IRAM_ATTR mpuCubeISR() {
    g_mpuCubeInterrupted = true;
}

class MpuCube : public IoTItem {
private:
    bool _debug = false; // Теперь управляется из конфига
    bool _isFirstRun = true;
    int _lastSide = 0;
    
    // Переменные сырых данных
    int16_t ax, ay, az;

    // Конфигурируемые из веб-интерфейса параметры
    int _address;       
    int _interruptPin;  
    int _motionThr;     

    // Метод инициализации
    bool initMPU() {
        Wire.beginTransmission(_address);
        Wire.write(0x6B); // PWR_MGMT_1
        Wire.write(0x00); // Пробуждение
        if (Wire.endTransmission() != 0) return false;

        // Конфигурация детектора движения
        Wire.beginTransmission(_address);
        Wire.write(0x1C); Wire.write(0x01); // HPF
        Wire.write(0x37); Wire.write(0x00); // Импульсный INT
        Wire.write(0x38); Wire.write(0x40); // Включаем MOT_EN
        Wire.write(0x1F); Wire.write(_motionThr); 
        Wire.write(0x20); Wire.write(40);   
        Wire.endTransmission();

        if (_debug) {
            Serial.printf("[MPU] Configured at 0x%02X. Interrupt Pin: %d. Motion Thr: %d\n", 
                          _address, _interruptPin, _motionThr);
        }
        return true;
    }

    // Сброс флага прерывания на чипе
    void clearMpuInterrupt() {
        Wire.beginTransmission(_address);
        Wire.write(0x3A); 
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)_address, (uint8_t)1);
        if (Wire.available()) {
            Wire.read(); 
        }
    }

    // Чтение векторов ускорения с реанимацией шины
    bool readAccel() {
        Wire.beginTransmission(_address);
        Wire.write(0x3B); 
        
        if (Wire.endTransmission(false) != 0) {
            if (_debug) Serial.println(F("[MPU Error] I2C Bus Crash detected! Resetting Wire..."));
            
            Wire.endTransmission(true); 
            Wire.begin();               
            
            Wire.beginTransmission(_address);
            Wire.write(0x6B); Wire.write(0x00); 
            Wire.endTransmission();
            
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

    // Математический расчет грани (> 45 градусов к горизонту)
    int detectSide() {
        if (!readAccel()) {
            return _lastSide; 
        }

        if (_debug) {
            Serial.printf("[MPU Raw] X: %6d | Y: %6d | Z: %6d\n", ax, ay, az);
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

        long pinValue;
        jsonRead(parameters, F("pin"), pinValue);
        _interruptPin = (int)pinValue;

        long thrValue;
        if (!jsonRead(parameters, F("thr"), thrValue)) {
            thrValue = 65; 
        }
        _motionThr = (int)thrValue;

        // Читаем настройку дебага из веба (0 или 1)
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
            if (initMPU()) {
                pinMode(_interruptPin, INPUT);
                // Разделение логики для ESP32 и ESP8266
                #ifdef ESP32
                gpio_install_isr_service(0); 
                #endif
                attachInterrupt(digitalPinToInterrupt(_interruptPin), mpuCubeISR, FALLING);
                clearMpuInterrupt();
            }
        }

        if (g_mpuCubeInterrupted) {
            g_mpuCubeInterrupted = false;
            clearMpuInterrupt();
            int activeSide = detectSide();

            if (activeSide != _lastSide) {
                _lastSide = activeSide;
                value.valD = activeSide;
                
                if (_debug) {
                    Serial.printf("[MPU New Side] Target side locked: %d\n", activeSide);
                }
                
                regEvent(String(activeSide), "MpuCube");
            }
        }
    }

    ~MpuCube() {
        detachInterrupt(digitalPinToInterrupt(_interruptPin));
    }
};

void *getAPI_Mpu6050(String subtype, String param) {
    if (subtype == F("MpuCube")) {
        String addr;
        jsonRead(param, F("addr"), addr);
        
        if (addr == "") {
            Serial.println(F("[MPU] Address is empty in config! Launching scanI2C()..."));
            scanI2C();
            return nullptr; 
        }

        return new MpuCube(param);
    }
    return nullptr;
}