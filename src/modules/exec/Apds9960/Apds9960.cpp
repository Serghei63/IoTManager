#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include <SparkFun_APDS9960.h>

class ApdsGesture : public IoTItem {
   private:
    SparkFun_APDS9960 apds = SparkFun_APDS9960();
    bool _debug = false;
    bool _isFirstRun = true;
    bool _initOK = false;
    unsigned long _lastPoll = 0;
    const unsigned long POLL_INTERVAL = 50;
    int _address; 

    uint8_t _gain = GGAIN_4X;
    uint8_t _ledDrive = LED_DRIVE_100MA;

    bool initAPDS() {
        #ifdef ESP32
        Wire.setTimeOut(1000);
        #endif

        if (!apds.init()) return false;

        // Применяем настройки усиления и тока ИК-диода из JSON
        apds.setGestureGain(_gain);
        apds.setGestureLEDDrive(_ledDrive);

        if (!apds.enableGestureSensor(false)) return false;

        return true;
    }

   public:
    ApdsGesture(String parameters) : IoTItem(parameters) {

        String addrStr;
        jsonRead(parameters, F("addr"), addrStr);
        _address = (int)strtol(addrStr.c_str(), NULL, 16);
        jsonRead(parameters, "debug", _debug);

        // Парсим параметры gain (1, 2, 4) и led (12, 25, 50, 100)
        int g = 4, l = 100;
        jsonRead(parameters, "gain", g);
        jsonRead(parameters, "led", l);

        if (g == 1) _gain = GGAIN_1X;
        else if (g == 2) _gain = GGAIN_2X;
        else _gain = GGAIN_4X;

        if (l == 12) _ledDrive = LED_DRIVE_12_5MA;
        else if (l == 25) _ledDrive = LED_DRIVE_25MA;
        else if (l == 50) _ledDrive = LED_DRIVE_50MA;
        else _ledDrive = LED_DRIVE_100MA;
    }

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;
            _initOK = initAPDS();
        }

        if (!_initOK) return;

        if (millis() - _lastPoll < POLL_INTERVAL) return;
        _lastPoll = millis();

        int gesture = apds.readGesture();

        if (gesture != DIR_NONE && gesture > 0) {
            int gestureCode = 0;

            switch (gesture) {
                case DIR_UP:    gestureCode = 1; break; // Вверх
                case DIR_DOWN:  gestureCode = 2; break; // Вниз
                case DIR_LEFT:  gestureCode = 3; break; // Влево
                case DIR_RIGHT: gestureCode = 4; break; // Вправо
            }

            if (gestureCode > 0) {
                value.valD = gestureCode;
                
                if (_debug) {
                    Serial.printf("[APDS Gesture] Detected code: %d\n", gestureCode);
                }

                regEvent(String(gestureCode), "ApdsGesture");
            }
        }
    }
};

void* getAPI_Apds9960(String subtype, String param) {

    if (subtype == F("ApdsGesture")) {
        return new ApdsGesture(param);
    } else {
        String addr;
        jsonRead(param, "addr", addr);

        if (addr == "") {
            Serial.println(F("[APDS] Address empty! Launching scanI2C()..."));
            scanI2C();
            return nullptr;
        }

        return new ApdsGesture(param);
    }

    return nullptr;
}