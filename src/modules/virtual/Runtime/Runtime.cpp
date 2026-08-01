#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include <FS.h>

class Runtime : public IoTItem {
private:
    String _targetID;
    unsigned long _lastStateChangeMillis; // Время последнего изменения/проверки
    uint64_t _totalMillis;                // Копилка миллисекунд (64 бита)
    String _filePath;                     // Путь к файлу сохранения
    bool _isTargetActive;                 // Текущий флаг состояния реле

    // Функция форматирования времени ЧЧ:ММ
    String formatDeviceTime(uint64_t ms) {
        uint64_t totalSeconds = ms / 1000;
        uint64_t totalMinutes = totalSeconds / 60;
        uint64_t hours = totalMinutes / 60;
        uint64_t minutes = totalMinutes % 60;

        char buf[32];
        sprintf(buf, "%02llu:%02llu", hours, minutes);
        return String(buf);
    }

public:
    Runtime(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "target", _targetID);
        _totalMillis = 0;
        value.isDecimal = false;
        value.valS = "00:00";
        _filePath = "/db/rt_" + _id + ".txt";

        // Восстановление данных при старте
        if (FileFS.exists(_filePath)) {
            fs::File f = FileFS.open(_filePath, FILE_READ);
            if (f) {
                String savedVal = f.readString();
                _totalMillis = strtoull(savedVal.c_str(), NULL, 10);
                value.valS = formatDeviceTime(_totalMillis);
                f.close();
            }
        }

        // Запоминаем стартовое состояние реле
        _isTargetActive = (getItemValue(_targetID) == "1");
        _lastStateChangeMillis = millis();
    }

    void loop() {
        IoTItem::loop(); 

        unsigned long currentMillis = millis();
        bool activeNow = (getItemValue(_targetID) == "1");

        if (_isTargetActive) {
            unsigned long elapsed = currentMillis - _lastStateChangeMillis;
            _totalMillis += elapsed; 
            
            if (!activeNow) {
                _isTargetActive = false;
                
                // Текстовое значение для экрана (ЧЧ:ММ)
                value.valS = formatDeviceTime(_totalMillis);
                
                // Числовое значение для графиков расхода (чистые Минуты)
                value.valD = (float)_totalMillis / 1000.0 / 60.0; 
                
                regEvent(value.valS, F("Runtime"));

                // Сохраняем миллисекунды во Flash
                fs::File f = FileFS.open(_filePath, FILE_WRITE);
                if (f) {
                    f.print(_totalMillis);
                    f.close();
                }
            }
        } 
        else if (activeNow) {
            _isTargetActive = true;
        }

        _lastStateChangeMillis = currentMillis;
    }

    void doByInterval() {
        if (_isTargetActive) {
            unsigned long cm = millis();
            unsigned long elapsed = cm - _lastStateChangeMillis;
            uint64_t currentTotal = _totalMillis + elapsed;
            
            value.valS = formatDeviceTime(currentTotal);
            // Тоже обновляем числовые минуты в реальном времени
            value.valD = (float)currentTotal / 1000.0 / 60.0; 
            
            publishValue(); 
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) {
        if (command == F("reset")) {
            _totalMillis = 0;
            value.valS = "00:00";
            regEvent(value.valS, F("Runtime"));
            _lastStateChangeMillis = millis();

            fs::File f = FileFS.open(_filePath, FILE_WRITE);
            if (f) {
                f.print(_totalMillis);
                f.close();
            }
        }
        return {};
    }
};

void* getAPI_Runtime(String subtype, String param) {
    if (subtype == F("Runtime")) {
        return new Runtime(param);
    }
    return nullptr;
}