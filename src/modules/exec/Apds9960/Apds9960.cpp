#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include <Adafruit_APDS9960.h>

// Глобальный флаг для прерывания
volatile bool g_apdsInterrupted = false;

void IRAM_ATTR apdsISR() {
    g_apdsInterrupted = true;
}

class ApdsGesture : public IoTItem {
private:
    Adafruit_APDS9960 apds;
    bool _debug = false;
    bool _isFirstRun = true;
    int _interruptPin;
    int _address;

bool initAPDS() {
        Wire.beginTransmission(_address);
        if (Wire.endTransmission() != 0) {
            if (_debug) Serial.printf("[APDS Error] Device not found at I2C address: 0x%02X\n", _address);
            return false;
        }

        if (!apds.begin(10, APDS9960_AGAIN_4X, _address)) {
            if (_debug) Serial.println(F("[APDS Error] Failed to initialize APDS9960 chip!"));
            return false;
        }

        // Включаем режим распознавания жестов (прерывания включаются внутри сами)
        apds.enableGesture(true);

        if (_debug) {
            Serial.printf("[APDS] Configured successfully at 0x%02X. INT Pin: %d\n", _address, _interruptPin);
        }
        return true;
    }

public:
    ApdsGesture(String parameters) : IoTItem(parameters) {
        // Читаем I2C адрес (дефолт для APDS9960 всегда 0x39)
        String addrStr;
        jsonRead(parameters, F("addr"), addrStr);
        _address = (addrStr.length() > 0) ? (int)strtol(addrStr.c_str(), NULL, 16) : 0x39;

        // Читаем пин аппаратного прерывания (пин INT датчика)
        long pinValue;
        jsonRead(parameters, F("pin"), pinValue);
        _interruptPin = (int)pinValue;

        // Настройка дебага из веб-интерфейса
        long dbgValue = 0;
        jsonRead(parameters, F("debug"), dbgValue);
        _debug = (dbgValue == 1);

        // Отключаем опрос по таймеру, датчик сам дернет нас за пин INT
        setInterval(0);
    }

    void doByInterval() override {}

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;
            Wire.begin();
            
            if (initAPDS()) {
                pinMode(_interruptPin, INPUT_PULLUP); // APDS прижимает пин INT к земле
                
                // Гарантируем запуск службы прерываний GPIO в ESP32
                // Разделение логики для ESP32 и ESP8266
                #ifdef ESP32
                gpio_install_isr_service(0); 
                #endif

                // Прерывание срабатывает по спаду (FALLING), когда датчик прижимает линию к GND
                attachInterrupt(digitalPinToInterrupt(_interruptPin), apdsISR, FALLING);
            }
        }

// Если датчик поймал жест и дернул прерывание
        if (g_apdsInterrupted) {
            g_apdsInterrupted = false;

            // Вычитываем жест из чипа
            uint8_t gesture = apds.readGesture();
            int gestureCode = 0;

            switch (gesture) {
                case APDS9960_UP:    gestureCode = 1; break;
                case APDS9960_DOWN:  gestureCode = 2; break;
                case APDS9960_LEFT:  gestureCode = 3; break;
                case APDS9960_RIGHT: gestureCode = 4; break;
            }

            if (gestureCode > 0) {
                value.valD = gestureCode;
                
                if (_debug) {
                    Serial.printf("[APDS Gesture] Detected code: %d\n", gestureCode);
                }

                // Генерируем штатное событие умного дома
                regEvent(String(gestureCode), "ApdsGesture");
            }
        }
    }

    ~ApdsGesture() {
        detachInterrupt(digitalPinToInterrupt(_interruptPin));
    }
};

// Функция интеграции в API.cpp менеджера
void *getAPI_Apds9960(String subtype, String param) {
    if (subtype == F("ApdsGesture")) {
        String addr;
        jsonRead(param, F("addr"), addr);
        
        // Штатное поведение автосканера
        if (addr == "") {
            Serial.println(F("[APDS] Address empty! Launching scanI2C()..."));
            scanI2C();
            return nullptr; 
        }

        return new ApdsGesture(param);
    }
    return nullptr;
}