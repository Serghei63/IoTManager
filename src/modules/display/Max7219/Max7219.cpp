#include "Global.h"          // Шаблон ядра
#include "classes/IoTItem.h" // Базовый класс элемента

#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include "6bite_rus.h"       // Подключение шрифта кириллицы
#include "effect.h"          // Эффекты анимации

class Max7219 : public IoTItem
{
private:
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

    MD_Parola* Display = nullptr;

    String _id2show = "";
    int _intensity = 5;         // Яркость по умолчанию (0..15)
    int _SPEED_IN_TIME = 50;    // Скорость входа анимации
    int _SPEED_OUT_TIME = 50;   // Скорость выхода анимации
    int _PAUSE_TIME = 2000;     // Пауза между анимациями
    int _inFX = 0;              // Эффект входа
    int _outFX = 0;             // Эффект выхода
    int _setTextAlignment = 1;  // Выравнивание (0 - Left, 1 - Center, 2 - Right)
    int _maxDevices = 5;        // Количество модулей по умолчанию

    String tmpStr = "Wait...";
    String lastValidStr = "";
    uint32_t prevMillis = 0;

public:
    Max7219(String parameters) : IoTItem(parameters)
    {
        // 1. Безопасное считывание настроек
        jsonRead(parameters, "id2show", _id2show);
        jsonRead(parameters, "intensity", _intensity);
        jsonRead(parameters, "SPEED_IN_TIME", _SPEED_IN_TIME);
        jsonRead(parameters, "SPEED_OUT_TIME", _SPEED_OUT_TIME);
        jsonRead(parameters, "PAUSE_TIME", _PAUSE_TIME);
        jsonRead(parameters, "effect_in", _inFX);
        jsonRead(parameters, "effect_out", _outFX);
        jsonRead(parameters, "setTextAlignment", _setTextAlignment);

        _maxDevices = jsonReadInt(parameters, "maxDevices");
        if (_maxDevices <= 0) _maxDevices = 5; // По умолчанию 5 модулей

        int clock = jsonReadInt(parameters, "clock"); 
        int data  = jsonReadInt(parameters, "data");  
        int cs    = jsonReadInt(parameters, "cs");    

        // Подставляем безопасные пины по умолчанию
        if (clock == 0) clock = 14;
        if (data == 0)  data = 13;
        if (cs == 0)    cs = 12; 

        // Коррекция значений
        if (_intensity < 0 || _intensity > 15) _intensity = 5;
        if (_inFX < 0 || _inFX > 28) _inFX = 0;
        if (_outFX < 0 || _outFX > 28) _outFX = 0;
        if (_setTextAlignment < 0 || _setTextAlignment > 2) _setTextAlignment = 1;

        // 2. Инициализация дисплея
        Display = new MD_Parola(HARDWARE_TYPE, data, clock, cs, _maxDevices);

        if (Display) {
            Display->begin();
            Display->setIntensity(_intensity);
            Display->setFont(_6bite_rus);
            Display->displayClear();

            // Старт вывода сообщения по умолчанию
            Display->displayText(
                tmpStr.c_str(),
                setTextAlignment[_setTextAlignment],
                _SPEED_IN_TIME,
                _PAUSE_TIME,
                effect[_inFX],
                effect[_outFX]
            );
        }
    }

    void loop() override
    {
        // Обязательно вызываем базовый loop класса IoTItem!
        IoTItem::loop();

        if (!Display) return;

        // Крутим анимацию дисплея
        if (Display->displayAnimate())
        {
            // Обновляем буфер только если строка реально изменилась
            if (tmpStr != lastValidStr && tmpStr.length() > 0) {
                lastValidStr = tmpStr;
                Display->setTextBuffer(lastValidStr.c_str());
            }
            Display->displayReset();
        }
    }

void doByInterval() override
{
    if (!Display) return;

    if (_id2show.length() > 0) {
        String val = getItemValue(_id2show);
        
        // Отладка в Serial Monitor
       // SerialPrint("I", "Max7219", "Looking for ID: '" + _id2show + "', Read Value: '" + val + "'");

        if (val.length() > 0) {
            tmpStr = val;
        } else {
            tmpStr = "No ID"; // Если ID не найден или значение пустое
        }
    } else {
        tmpStr = "No Conf";
    }

    // Обновление настроек из веба
    if (_intensity >= 0 && _intensity <= 15) {
        Display->setIntensity(_intensity);
    }

    Display->setPause(_PAUSE_TIME);
    Display->setSpeedInOut(_SPEED_IN_TIME, _SPEED_OUT_TIME);

    if (_inFX >= 0 && _inFX <= 28 && _outFX >= 0 && _outFX <= 28) {
        Display->setTextEffect(effect[_inFX], effect[_outFX]);
    }
}

// Вызывается ядром мгновенно при изменении любой переменной в системе
void onChange(String key, String value) 
{
    // Проверяем, изменилась ли именно та переменная, которую мы выводим
    if (key == _id2show) {
        if (value.length() > 0 && value != tmpStr) {
            tmpStr = value;
            
            // Если экран инициализирован, сразу же обновляем текст
            if (Display) {
                Display->setTextBuffer(tmpStr.c_str());
                Display->displayReset();
            }
            SerialPrint("I", "Max7219", "Instant update for " + key + " = " + value);
        }
    }
}

    ~Max7219() {
        if (Display) {
            Display->displayClear();
            delete Display;
            Display = nullptr;
        }
    }
};

void *getAPI_Max7219(String subtype, String param)
{
    if (subtype == F("Max7219"))
    {
        return new Max7219(param);
    }
    return nullptr;
}