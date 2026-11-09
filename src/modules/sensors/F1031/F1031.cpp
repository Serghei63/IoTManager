#include "Global.h"
#include "classes/IoTItem.h"

extern IoTGpio IoTgpio;

class F1031V : public IoTItem {
private:
    uint8_t _pin = 255;
    float _maxFlow = 25.0f;    // Максимальный расход по конфигу (по умолчанию 25 л/мин)
    int _avgSamples = 10;      // Количество усреднений для сглаживания шумов ADC

public:
    F1031V(String parameters) : IoTItem(parameters) {
        // 1. Читаем аналоговый пин
        int pinVal = -1;
        jsonRead(parameters, F("pin"), pinVal);
        if (pinVal >= 0) {
            _pin = (uint8_t)pinVal;
            IoTgpio.pinMode(_pin, INPUT);
        } else {
            SerialPrint("E", F("F1031V"), "'" + _id + "' CRITICAL: 'pin' parameter missing!");
        }

        // 2. Читаем параметры датчика из конфига
        jsonRead(parameters, F("maxFlow"), _maxFlow, false); 
        jsonRead(parameters, F("samples"), _avgSamples, false);

        // 3. Интервал опроса
        long interval = 15;
        jsonRead(parameters, F("int"), interval, false);
        setInterval(interval > 0 ? interval : 0);
    }

    void readSensor() {
        if (_pin == 255) return;

        // 1. Считываем несколько сэмплов для усреднения (убираем аналоговый шум)
        uint32_t adcSum = 0;
        for (int i = 0; i < _avgSamples; i++) {
            adcSum += IoTgpio.analogRead(_pin);
            delay(2);
        }
        float rawAdc = (float)adcSum / (float)_avgSamples;

        // 2. Считаем напряжение на пине микроконтроллера
        #if defined(ESP32)
            float vPin = (rawAdc / 4095.0f) * 3.3f;
        #else
            float vPin = (rawAdc / 1023.0f) * 3.3f;
        #endif

        // 3. Восстанавливаем реальное напряжение датчика с учетом делителя (R1=10k, R2=18k)
        // Формула делителя: V_sensor = V_pin * (R1 + R2) / R2
        float vSensor = vPin * ((10.0f + 18.0f) / 18.0f);

        // 4. Пересчитываем Вольты в расход (Стандартная шкала: 0.5V = 0 л/мин, 4.5V = maxFlow)
        float flow = 0.0f;
        if (vSensor > 0.5f) {
            flow = ((vSensor - 0.5f) / 4.0f) * _maxFlow;
        }

        if (flow < 0.0f) flow = 0.0f; // Защита от отрицательных значений

        value.valD = flow;

        SerialPrint("I", F("F1031V"), "'" + _id + "' Raw ADC: " + String(rawAdc, 0) + 
                    " | Pin V: " + String(vPin, 2) + "V | Sensor V: " + String(vSensor, 2) + 
                    "V | Flow: " + String(value.valD, 2));

        // 5. Регистрация события в ядре IoTmanager
        regEvent(value.valD, F("F1031V"));
    }

    void doByInterval() override {
        readSensor();
    }

    ~F1031V() {};
};

void *getAPI_F1031(String subtype, String param) {
    if (subtype == F("F1031V") || subtype == F("F1031V_Item")) {
        return new F1031V(param);
    }
    return nullptr;
}