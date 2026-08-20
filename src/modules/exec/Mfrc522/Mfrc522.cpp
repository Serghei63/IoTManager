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

    String _lastUid = "";
    unsigned long _lastScanTime = 0;
    const unsigned long SCAN_INTERVAL = 250;  // Опрос раз в 250 мс
    const unsigned long CLEAR_TIMEOUT = 1500; // Автосброс UID через 1.5 сек после убирания метки

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
                // Создаем специальный объект пина CS для MFRC522v2
                _csPinObj = new MFRC522DriverPinSimple((uint8_t)_csPin);

                if (_rstPin != -1) {
                    _rstPinObj = new MFRC522DriverPinSimple((uint8_t)_rstPin);
                }

                // Инициализируем SPI драйвер
                _driver = new MFRC522DriverSPI(*_csPinObj, SPI);
            } else {
                if (_debug) Serial.println(F("[RFID] ERROR: CS Pin not set for SPI!"));
            }
        }

        if (_driver) {
            _mfrc522 = new MFRC522(*_driver);
            _mfrc522->PCD_Init();
            if (_debug) Serial.printf("[RFID] Initialized via %s\n", _bus.c_str());
        }
    }

    void loop() override {
        if (!_mfrc522) return;

        // Не спамим шину, проверяем по таймеру
        if (millis() - _lastScanTime < SCAN_INTERVAL) return;
        _lastScanTime = millis();

        // Проверяем наличие карты
        if (_mfrc522->PICC_IsNewCardPresent() && _mfrc522->PICC_ReadCardSerial()) {
            String currentUid = "";
            for (byte i = 0; i < _mfrc522->uid.size; i++) {
                if (i > 0) currentUid += ":";
                if (_mfrc522->uid.uidByte[i] < 0x10) currentUid += "0";
                currentUid += String(_mfrc522->uid.uidByte[i], HEX);
            }
            currentUid.toUpperCase();

            if (currentUid != _lastUid) {
                _lastUid = currentUid;
                value.valS = _lastUid;

                if (_debug) Serial.printf("[RFID] Tag Read: %s\n", _lastUid.c_str());

                // Передаем новое значение и событие в сценарии
                regEvent(value.valS, "RfidTag");
            }

            // Останавливаем работу с текущей меткой
            _mfrc522->PICC_HaltA();
            _mfrc522->PCD_StopCrypto1();
        } else {
            // Если метку убрали — сбрасываем значение через таймаут
            if (_lastUid != "" && (millis() - _lastScanTime > CLEAR_TIMEOUT)) {
                _lastUid = "";
                value.valS = "";
                regEvent("", "RfidTagCleared");
                if (_debug) Serial.println(F("[RFID] Tag Removed"));
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

        // Если выбрана шина I2C, проверяем заполненность адреса
        if (bus == "i2c") {
            String addr;
            jsonRead(param, "addr", addr);

            // Если адрес пустой — запускаем сканер и отменяем создание
            if (addr == "") {
                scanI2C();
                return nullptr;
            }
        }

        return new Mfrc522Item(param);
    }
    return nullptr;
}