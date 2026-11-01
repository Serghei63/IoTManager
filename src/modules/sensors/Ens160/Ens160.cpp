#include "Global.h"
#include "classes/IoTItem.h"
#include "Wire.h"
#include "SparkFun_ENS160.h"

// Глобальный объект датчика и флаг успешной инициализации
SparkFun_ENS160 myENS; 
static bool ensInitialized = false;

// Вспомогательная функция безопасной инициализации
bool initENS160Once() {
    if (ensInitialized) return true;

    if (!myENS.begin()) {
        SerialPrint("E", F("ENS160"), F("Could not communicate with ENS160! Check wiring."));
        return false;
    }

    myENS.setOperatingMode(SFE_ENS160_RESET);
    myENS.setOperatingMode(SFE_ENS160_STANDARD);
    ensInitialized = true;
    SerialPrint("I", F("ENS160"), F("Sensor initialized successfully."));
    return true;
}

// ------------------- TVOC -------------------
class Ens160tvoc : public IoTItem {
public:
    Ens160tvoc(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getTVOC();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160tvoc");
            } else {
                SerialPrint("E", F("Ens160tvoc"), F("Read error"), _id);
            }
        }
    }
    ~Ens160tvoc() {}
};

// ------------------- eCO2 -------------------
class Ens160eco2 : public IoTItem {
public:
    Ens160eco2(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getECO2();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160eco2");
            } else {
                SerialPrint("E", F("Ens160eco2"), F("Read error"), _id);
            }
        }
    }
    ~Ens160eco2() {}
};

// ------------------- AQI -------------------
class Ens160aqi : public IoTItem {
public:
    Ens160aqi(String parameters) : IoTItem(parameters) {
        initENS160Once();
    }

    void doByInterval() override {
        if (!ensInitialized && !initENS160Once()) return;

        if (myENS.checkDataStatus()) {
            value.valD = myENS.getAQI();
            if (value.valD >= 0) {
                regEvent(value.valD, "Ens160aqi");
            } else {
                SerialPrint("E", F("Ens160aqi"), F("Read error"), _id);
            }
        }
    }
    ~Ens160aqi() {}
};

void* getAPI_Ens160(String subtype, String param) {
    if (subtype == F("Ens160tvoc")) {
        return new Ens160tvoc(param);
    } else if (subtype == F("Ens160eco2")) {
        return new Ens160eco2(param);
    } else if (subtype == F("Ens160aqi")) {
        return new Ens160aqi(param);
    }
    return nullptr;
}
