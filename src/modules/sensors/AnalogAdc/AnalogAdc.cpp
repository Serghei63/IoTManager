#include "Global.h"
#include "classes/IoTItem.h"

extern IoTGpio IoTgpio;

class AnalogAdc : public IoTItem
{
private:
    unsigned int _pin;
    unsigned int _avgSteps;
    unsigned int realSteps = 0;
    float _adcAverage = 0;
    unsigned int _period = 0;
    unsigned long _lastSoundingMillis = 0;

public:
    AnalogAdc(String parameters) : IoTItem(parameters)
    {
        _pin = jsonReadInt(parameters, "pin");
        _avgSteps = jsonReadInt(parameters, "avgSteps");
        if (!_avgSteps)
        {
            jsonRead(parameters, F("int"), _interval, false);
        }
        else
        {
            _period = (_interval * 1000) / _avgSteps; // переводим интервал в миллисекунды для расчета периода опроса
        }
    }

    void doByInterval()
    {
        if (_avgSteps <= 1) {
            value.valD = IoTgpio.analogRead(_pin);
        } else {
            if (realSteps > 0) {
                value.valD = _adcAverage / (float)realSteps;
            } else {
                value.valD = IoTgpio.analogRead(_pin); // страховка, если за интервал не успели сделать ни одного замера
            }
            // Сбрасываем накопленные данные для следующего интервала
            _adcAverage = 0;
            realSteps = 0;
        }
        regEvent(value.valD, "AnalogAdc");
    }

    void loop()
    {
        if (_avgSteps > 1)
        {
            if (millis() - _lastSoundingMillis >= _period)
            {
                _lastSoundingMillis = millis();
                int sounding = IoTgpio.analogRead(_pin);
                _adcAverage += sounding;
                realSteps++;
            }
        }
        IoTItem::loop();
    }

    ~AnalogAdc(){};
};

void *getAPI_AnalogAdc(String subtype, String param)
{
    if (subtype == F("AnalogAdc"))
    {
        return new AnalogAdc(param);
    }
    else
    {
        return nullptr;
    }
}
