#include "Global.h"
#include "classes/IoTItem.h"
#include <Wire.h>
#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>

class Mfrc522Item : public IoTItem {
   private:
    String _bus = "i2c";
    uint8_t _i2cAddr = 0;
    int _csPin = -1;
    int _rstPin = -1;
    bool _debug = false;

    // Объекты оберток пинов для библиотеки v2
    MFRC522DriverPinSimple* _csPinObj = nullptr;
    MFRC522DriverPinSimple* _rstPinObj = nullptr;

    // Указатели на драйвер и главный объект RFID
    MFRC522Driver* _driver = nullptr;
    MFRC522* _mfrc522 = nullptr;

    uint32_t _lastUidNum = 0;
    unsigned long _lastScanTime = 0;
    const unsigned long SCAN_INTERVAL = 250;  // Опрос раз в 250 мс
    const unsigned long CLEAR_TIMEOUT = 1500; // Автосброс в 0 через 1.5 сек после убирания метки

   public:
    Mfrc522Item(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "bus", _bus);
        jsonRead(parameters, "debug", _debug);
        _bus.toLowerCase();

        if (_bus == "i2c") {
            String addrStr;
            jsonRead(parameters, "addr", addrStr);
            _i2cAddr = hexStringToUint8(addrStr);

            if (_i2cAddr != 0) {
                _driver = new MFRC522DriverI2C(_i2cAddr, Wire);
            } else {
                if (_debug) Serial.println(F("[RFID] ERROR: Invalid I2C Address!"));
            }

        } else if (_bus == "spi") {
            jsonRead(parameters, "cs", _csPin);
            jsonRead(parameters, "rst", _rstPin);

            if (_csPin != -1) {
                // Инициализируем шину SPI
                SPI.begin();

                // Аппаратный сброс RC522 при наличии RST пина
                if (_rstPin != -1) {
                    _rstPinObj = new MFRC522DriverPinSimple((uint8_t)_rstPin);
                    pinMode(_rstPin, OUTPUT);
                    digitalWrite(_rstPin, LOW);
                    delay(50);
                    digitalWrite(_rstPin, HIGH);
                    delay(50);
                }

                _csPinObj = new MFRC522DriverPinSimple((uint8_t)_csPin);
                _driver = new MFRC522DriverSPI(*_csPinObj, SPI);
            } else {
                if (_debug) Serial.println(F("[RFID] ERROR: CS Pin not set for SPI!"));
            }
        }

        if (_driver) {
            _mfrc522 = new MFRC522(*_driver);
            _mfrc522->PCD_Init();

            if (_debug) {
                Serial.printf("[RFID] Initialized via %s\n", _bus.c_str());
                Serial.print(F("[RFID] Firmware Version: "));
                MFRC522Debug::PCD_DumpVersionToSerial(*_mfrc522, Serial);
            }
        }
    }

    void loop() override {
        if (!_mfrc522) return;

        unsigned long now = millis();

        // Не спамим шину, проверяем по интервалу
        if (now - _lastScanTime < SCAN_INTERVAL) return;

        bool cardPresent = false;

        // 1. Проверяем появление НОВОЙ метки
        if (_mfrc522->PICC_IsNewCardPresent() && _mfrc522->PICC_ReadCardSerial()) {
            cardPresent = true;
        } 
        // 2. Если новая не найдена, но метка была ранее — проверяем, лежит ли она всё еще
        else if (_lastUidNum != 0) {
            byte bufferATQA[2];
            byte bufferSize = sizeof(bufferATQA);

            // Будим метку в поле
            MFRC522::StatusCode status = _mfrc522->PICC_WakeupA(bufferATQA, &bufferSize);
            if (status == MFRC522::StatusCode::STATUS_OK || status == 0) {
                if (_mfrc522->PICC_ReadCardSerial()) {
                    cardPresent = true;
                }
            }
        }

        if (cardPresent) {
            _lastScanTime = now; // Фиксируем время успешного чтения

            // Конвертируем 4 байта UID в 32-битное число (DEC)
            uint32_t currentUidNum = 0;
            for (byte i = 0; i < _mfrc522->uid.size; i++) {
                currentUidNum = (currentUidNum << 8) | _mfrc522->uid.uidByte[i];
            }

            // Передаем событие только при изменении метки
            if (currentUidNum != _lastUidNum) {
                _lastUidNum = currentUidNum;
                value.valD = (double)_lastUidNum; // Записываем числовое значение (как в RCswitch)

                if (_debug) Serial.printf("[RFID] Tag Read DEC: %u\n", _lastUidNum);

                // Регистрируем событие по ID элемента из JSON-конфига
                regEvent(value.valD, _id);
            }

            // Усыпляем метку до следующего цикла опроса
            _mfrc522->PICC_HaltA();
            _mfrc522->PCD_StopCrypto1();

        } else {
            // Метка убрана, сбрасываем в 0 по таймауту
            if (_lastUidNum != 0 && (now - _lastScanTime > CLEAR_TIMEOUT)) {
                _lastUidNum = 0;
                value.valD = 0;

                if (_debug) Serial.println(F("[RFID] Tag Removed (Reset to 0)"));

                regEvent(value.valD, _id);
            }
        }
    }

    ~Mfrc522Item() {
        if (_mfrc522) { delete _mfrc522; _mfrc522 = nullptr; }
        if (_driver) { delete _driver; _driver = nullptr; }
        if (_csPinObj) { delete _csPinObj; _csPinObj = nullptr; }
        if (_rstPinObj) { delete _rstPinObj; _rstPinObj = nullptr; }
    }
};

void* getAPI_Mfrc522(String subtype, String param) {
    if (subtype == F("Mfrc522")) {
        String bus = "i2c";
        jsonRead(param, "bus", bus);
        bus.toLowerCase();

        if (bus == "i2c") {
            String addr;
            jsonRead(param, "addr", addr);

            if (addr == "") {
                scanI2C();
                return nullptr;
            }
        }

        return new Mfrc522Item(param);
    }
    return nullptr;
}