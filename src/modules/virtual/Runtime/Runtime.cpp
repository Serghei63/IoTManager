/*
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
    */
 /*  
#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include <FS.h>

class Runtime : public IoTItem {
private:
    String _targetID;
    unsigned long _lastStateChangeMillis; 
    uint64_t _totalMillis;                
    String _filePath;                     
    bool _isTargetActive;                 

    // Гарантия наличия папки /db/
    void ensureDirectoryExists(const String& path) {
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash > 0) {
            String dir = path.substring(0, lastSlash);
            if (!FileFS.exists(dir)) {
                FileFS.mkdir(dir);
            }
        }
    }

    // Форматирование времени с учетом ДНЕЙ (d)
    String formatDeviceTime(uint64_t ms) {
        uint64_t totalSeconds = ms / 1000;
        uint64_t totalMinutes = totalSeconds / 60;
        uint64_t totalHours = totalMinutes / 60;
        uint64_t days = totalHours / 24;
        
        uint64_t hours = totalHours % 24;
        uint64_t minutes = totalMinutes % 60;

        char buf[32];
        if (days > 0) {
            // Формат для работы более суток: "1d 05:20"
            sprintf(buf, "%llud %02llu:%02llu", days, hours, minutes);
        } else {
            // Стандартный формат до 24 часов: "05:20"
            sprintf(buf, "%02llu:%02llu", hours, minutes);
        }
        return String(buf);
    }

    void saveToFS() {
        ensureDirectoryExists(_filePath);
        fs::File f = FileFS.open(_filePath, FILE_WRITE);
        if (f) {
            f.print(_totalMillis);
            f.close();
        } else {
            SerialPrint("E", "Runtime", "Failed to save: " + _filePath);
        }
    }

public:
Runtime(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "target", _targetID);
        
        // 1. Считываем интервал из config.json (в секундах)
        int interval = 60; // Дефолтное значение 60 секунд, если в конфиге не задано
        jsonRead(parameters, "int", interval);
        
        // 2. Регистрируем интервал в ядре IoTmanager!
        setInterval(interval); 

        _totalMillis = 0;
        value.isDecimal = false;
        value.valS = "00:00";
        value.valD = 0.0;
        _filePath = "/db/rt_" + _id + ".txt";

        // Сохраняем/создаем директорию
        if (!FileFS.exists("/db")) {
            FileFS.mkdir("/db");
        }

        // Восстановление данных при старте
        if (FileFS.exists(_filePath)) {
            fs::File f = FileFS.open(_filePath, FILE_READ);
            if (f) {
                String savedVal = f.readString();
                _totalMillis = strtoull(savedVal.c_str(), NULL, 10);
                value.valS = formatDeviceTime(_totalMillis);
                value.valD = (float)_totalMillis / 1000.0 / 60.0;
                f.close();
            }
        }

        _isTargetActive = (getItemValue(_targetID) == "1");
        _lastStateChangeMillis = millis();
    }

    void loop() override {
        IoTItem::loop(); 

        unsigned long currentMillis = millis();
        bool activeNow = (getItemValue(_targetID) == "1");

        // Если реле сейчас активна — накапливаем миллисекунды
        if (_isTargetActive) {
            unsigned long elapsed = currentMillis - _lastStateChangeMillis;
            _totalMillis += elapsed; 
            
            // Фиксируем момент ВЫКЛЮЧЕНИЯ
            if (!activeNow) {
                _isTargetActive = false;
                
                value.valS = formatDeviceTime(_totalMillis);
                value.valD = (float)_totalMillis / 1000.0 / 60.0; 
                
                regEvent(value.valS, F("Runtime"));
                saveToFS(); // Сохраняем в файл при выключении
            }
        } 
        // Фиксируем момент ВКЛЮЧЕНИЯ
        else if (activeNow) {
            _isTargetActive = true;
        }

        _lastStateChangeMillis = currentMillis;
    }

    // Вызывается автоматически по таймауту элемента (например, каждые 60 секунд)
    void doByInterval() override {
        if (_isTargetActive) {
            unsigned long cm = millis();
            unsigned long elapsed = cm - _lastStateChangeMillis;
            _totalMillis += elapsed;
            _lastStateChangeMillis = cm;

            value.valS = formatDeviceTime(_totalMillis);
            value.valD = (float)_totalMillis / 1000.0 / 60.0; // Всегда актуальные минуты
            
            // Генерируем событие для веба и математики!
            regEvent(value.valS, F("Runtime"));
            
            // Периодически сохраняем на случай внезапного перезапуска ESP
            saveToFS(); 
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == F("reset")) {
            _totalMillis = 0;
            value.valS = "00:00";
            value.valD = 0.0;
            regEvent(value.valS, F("Runtime"));
            _lastStateChangeMillis = millis();

            saveToFS();
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
*/
#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include <FS.h>

class Runtime : public IoTItem {
private:
    String _targetID;
    unsigned long _lastStateChangeMillis; 
    uint64_t _totalMillis;                
    String _filePath;                     
    bool _isTargetActive;                 

    // Гарантия наличия папки /db/
    void ensureDirectoryExists(const String& path) {
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash > 0) {
            String dir = path.substring(0, lastSlash);
            if (!FileFS.exists(dir)) {
                FileFS.mkdir(dir);
            }
        }
    }

    // Форматирование времени с учетом ДНЕЙ (d)
    String formatDeviceTime(uint64_t ms) {
        uint64_t totalSeconds = ms / 1000;
        uint64_t totalMinutes = totalSeconds / 60;
        uint64_t totalHours = totalMinutes / 60;
        uint64_t days = totalHours / 24;
        
        uint64_t hours = totalHours % 24;
        uint64_t minutes = totalMinutes % 60;

        char buf[32];
        if (days > 0) {
            sprintf(buf, "%llud %02llu:%02llu", days, hours, minutes);
        } else {
            sprintf(buf, "%02llu:%02llu", hours, minutes);
        }
        return String(buf);
    }

    void saveToFS() {
        ensureDirectoryExists(_filePath);
        fs::File f = FileFS.open(_filePath, FILE_WRITE);
        if (f) {
            f.print(_totalMillis);
            f.close();
        } else {
            SerialPrint("E", "Runtime", "Failed to save: " + _filePath);
        }
    }

public:
    Runtime(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "target", _targetID);
        
        int interval = 60; // Дефолтное значение 60 секунд
        jsonRead(parameters, "int", interval);
        setInterval(interval); 

        _totalMillis = 0;
        value.isDecimal = false;
        value.valS = "00:00";
        value.valD = 0.0;
        _filePath = "/db/rt_" + _id + ".txt";

        if (!FileFS.exists("/db")) {
            FileFS.mkdir("/db");
        }

        if (FileFS.exists(_filePath)) {
            fs::File f = FileFS.open(_filePath, FILE_READ);
            if (f) {
                String savedVal = f.readString();
                _totalMillis = strtoull(savedVal.c_str(), NULL, 10);
                value.valS = formatDeviceTime(_totalMillis);
                value.valD = (float)_totalMillis / 1000.0 / 60.0;
                f.close();
            }
        }

        _isTargetActive = (getItemValue(_targetID) == "1");
        _lastStateChangeMillis = millis();
    }

    void loop() override {
        IoTItem::loop(); 

        unsigned long currentMillis = millis();
        bool activeNow = (getItemValue(_targetID) == "1");

        // Отслеживаем только МОМЕНТ СМЕНЫ СОСТОЯНИЯ
        if (_isTargetActive && !activeNow) {
            // Реле ВЫКЛЮЧИЛОСЬ: считаем финальную дельту
            unsigned long elapsed = currentMillis - _lastStateChangeMillis;
            _totalMillis += elapsed; 
            _lastStateChangeMillis = currentMillis;
            _isTargetActive = false;
            
            value.valS = formatDeviceTime(_totalMillis);
            value.valD = (float)_totalMillis / 1000.0 / 60.0; 
            
            regEvent(value.valS, F("Runtime"));
            saveToFS();
        } 
        else if (!_isTargetActive && activeNow) {
            // Реле ВКЛЮЧИЛОСЬ: фиксируем момент старта без списания
            _isTargetActive = true;
            _lastStateChangeMillis = currentMillis;
        }
    }

    // Вызывается автоматически по интервалу (например, каждые 60 секунд)
    void doByInterval() override {
        if (_isTargetActive) {
            unsigned long cm = millis();
            unsigned long elapsed = cm - _lastStateChangeMillis;
            
            _totalMillis += elapsed;
            // ВАЖНО: Точный сдвиг временной метки (без накопления погрешности)
            _lastStateChangeMillis = cm;

            value.valS = formatDeviceTime(_totalMillis);
            value.valD = (float)_totalMillis / 1000.0 / 60.0; 
            
            regEvent(value.valS, F("Runtime"));
            saveToFS(); 
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == F("reset")) {
            _totalMillis = 0;
            value.valS = "00:00";
            value.valD = 0.0;
            regEvent(value.valS, F("Runtime"));
            _lastStateChangeMillis = millis();

            saveToFS();
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