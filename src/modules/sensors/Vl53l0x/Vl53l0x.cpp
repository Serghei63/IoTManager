#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include "Adafruit_VL53L0X.h"

class VL53L0X_Item : public IoTItem {
private:
    uint8_t _xshutPin = 255;
    Adafruit_VL53L0X _lox;
    uint16_t _distance = 0;

public:
    // Сканер I2C
    void scanI2C() {
        SerialPrint("I", F("VL53L0X"), "'" + _id + "' --- START I2C SCAN ---");
        byte count = 0;

        #if defined(ESP32)
            Wire.begin(); 
        #endif

        for (byte address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            byte error = Wire.endTransmission();

            if (error == 0) {
                SerialPrint("I", F("VL53L0X"), "'" + _id + "' [FOUND] Device at 0x" + String(address, HEX));
                count++;
            }
        }

        if (count == 0) {
            SerialPrint("E", F("VL53L0X"), "'" + _id + "' [ERROR] No I2C devices found on bus!");
        } else {
            SerialPrint("I", F("VL53L0X"), "'" + _id + "' Scan finished. Total devices: " + String(count));
        }
    }

    VL53L0X_Item(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, F("id"), _id);
        
        // Чтение XSHUT пина из конфигурации (пробуем "xshut", если нет - "pin")
        int pinVal = -1;
        if (!jsonRead(parameters, F("xshut"), pinVal)) {
            jsonRead(parameters, F("pin"), pinVal);
        }

        if (pinVal >= 0) {
            _xshutPin = (uint8_t)pinVal;
            pinMode(_xshutPin, OUTPUT);
            
            // Включаем лазер при старте для теста
            digitalWrite(_xshutPin, HIGH); 
            delay(50);

            SerialPrint("I", F("VL53L0X"), "'" + _id + "' Parsed XSHUT pin: " + String(_xshutPin));

            // Запускаем сканер прямо в конструкторе
            scanI2C();
            
            // Сразу усыпляем обратно до первого замера
            digitalWrite(_xshutPin, LOW);
        } else {
            SerialPrint("E", F("VL53L0X"), "'" + _id + "' CRITICAL: 'xshut' or 'pin' is missing in config!");
        }

        long interval = 5;
        jsonRead(parameters, F("int"), interval, false);
        if (interval > 0) {
            setInterval(interval);
        } else {
            setInterval(0);
        }
    }

    void readSensor() {
        if (_xshutPin == 255) {
            SerialPrint("E", F("VL53L0X"), "'" + _id + "' Skipping read: pin is invalid (255)");
            return;
        }

        // 1. Включаем лазер (пробуждаем из XSHUT)
        digitalWrite(_xshutPin, HIGH);
        delay(30); // 30 мс даем микросхеме завестись

        // 2. Инициализация и измерение Adafruit
             if (_lox.begin(0x29)) {
            VL53L0X_RangingMeasurementData_t measure;
            
            // Чтение дистанции
            _lox.rangingTest(&measure, false);

            // Phase failures / out of bounds имеют статус 4
            if (measure.RangeStatus != 4) {
                _distance = measure.RangeMilliMeter;
                SerialPrint("I", F("VL53L0X"), "'" + _id + "' Measured distance: " + String(_distance) + " mm");
            } else {
                _distance = 0;
                SerialPrint("E", F("VL53L0X"), "'" + _id + "' Out of range or measurement failure");
            }
        } else {
            _distance = 0;
            SerialPrint("E", F("VL53L0X"), "'" + _id + "' Adafruit_VL53L0X.begin() FAILED at 0x29");
        }

        // 3. Гасим лазер обратно (отключаем шину для работы с другими сенсорами)
        digitalWrite(_xshutPin, LOW);

        // 4. Отправляем значение в систему IoTmanager
        value.valD = _distance;
        regEvent(value.valD, F("VL53L0X"));
    }

    void doByInterval() override {
        readSensor();
    }
};

// Фабрика вызова
void *getAPI_Vl53l0x(String subtype, String param) {
    if (subtype == F("VL53L0X") || subtype == F("VL53L0X_Item")) {
        return new VL53L0X_Item(param);
    }
    return nullptr;
}