#include "Global.h"
#include "classes/IoTItem.h"
#include "AM2302-Sensor.h"
#include <map>

// Структура для хранения сенсора и времени последнего опроса
struct SensorEntry {
    AM2302::AM2302_Sensor* sensor = nullptr;
    bool isInitialized = false;
    float lastTemp = NAN;
    float lastHumidity = NAN;
    unsigned long lastReadTime = 0;
};

static std::map<int, SensorEntry> am2302Sensors;

class Am2302Item : public IoTItem {
private:
    int _pin;
    bool _isTemperature; // true = Температура, false = Влажность

public:
    Am2302Item(int pin, bool isTemp, String parameters) 
        : IoTItem(parameters), _pin(pin), _isTemperature(isTemp) 
    {
        // Инициализируем физический датчик один раз на пин
        auto& entry = am2302Sensors[_pin];
        if (!entry.sensor) {
            entry.sensor = new AM2302::AM2302_Sensor(_pin);
        }
        if (!entry.isInitialized) {
            entry.sensor->begin();
            entry.isInitialized = true;
        }
    }

    void doByInterval() override {
        auto& entry = am2302Sensors[_pin];
        if (!entry.sensor) return;

        // Кэшируем показания: опрашиваем физический датчик не чаще раз в 2 секунды
        if (millis() - entry.lastReadTime > 2000 || entry.lastReadTime == 0) {
            // Читаем датчик один раз для ОБОИХ экземпляров (T и H)
            entry.sensor->read(); 
            entry.lastTemp = entry.sensor->get_Temperature();
            entry.lastHumidity = entry.sensor->get_Humidity();
            entry.lastReadTime = millis();
        }

        // Выбираем нужное значение
        float val = _isTemperature ? entry.lastTemp : entry.lastHumidity;

        // Проверка на корректность данных (!isnan)
        if (!isnan(val)) {
            value.valD = val;
            regEvent(value.valD, _isTemperature ? "Am2302t" : "Am2302h");
        } else {
            SerialPrint("E", _isTemperature ? "Am2302t" : "Am2302h", "Read error on pin " + String(_pin), _id);
        }
    }

    ~Am2302Item() {};
};

void* getAPI_Am2302(String subtype, String param) {
    if (subtype == F("Am2302t") || subtype == F("Am2302h")) {
        int pin = 0;
        jsonRead(param, "pin", pin);

        if (pin > 0) {
            bool isTemp = (subtype == F("Am2302t"));
            return new Am2302Item(pin, isTemp, param);
        }
    }
    return nullptr;
}