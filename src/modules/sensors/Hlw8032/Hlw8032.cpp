#include "Global.h"
#include "classes/IoTItem.h"
#include "HLW8032.h"

// Глобальный объект сенсора
HLW8032 HL;
static bool hlwInitialized = false;

// Однократная инициализация Hardware Serial для HLW8032
void initHLW8032Once() {
    if (!hlwInitialized) {
        // По умолчанию используем Serial1 и пин RX 4 (можно вынести в конфиг)
        HL.begin(Serial1, 4);
        hlwInitialized = true;
        SerialPrint("I", F("HLW8032"), F("Hardware Serial initialized on Pin 4"));
    }
}

// ------------------- Напряжение -------------------
class Hlw8032v : public IoTItem {
public:
    Hlw8032v(String parameters) : IoTItem(parameters) {
        initHLW8032Once();
    }

    void loop() override {
        // Постоянно подчитываем буфер UART, чтобы не терять кадры
        HL.SerialReadLoop();
    }

    void doByInterval() override {
        if (HL.SerialRead == 1) {
            value.valD = HL.GetVol() * 0.001f;
            regEvent(value.valD, "Hlw8032v");
        }
    }

    ~Hlw8032v() {}
};

// ------------------- Ток -------------------
class Hlw8032i : public IoTItem {
public:
    Hlw8032i(String parameters) : IoTItem(parameters) {
        initHLW8032Once();
    }

    void loop() override {
        HL.SerialReadLoop();
    }

    void doByInterval() override {
        if (HL.SerialRead == 1) {
            value.valD = HL.GetCurrent();
            regEvent(value.valD, "Hlw8032i");
        }
    }

    ~Hlw8032i() {}
};

void* getAPI_Hlw8032(String subtype, String param) {
    if (subtype == F("Hlw8032v")) {
        return new Hlw8032v(param);
    } else if (subtype == F("Hlw8032i")) {
        return new Hlw8032i(param);
    }
    return nullptr;
}
