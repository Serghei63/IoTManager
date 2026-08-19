/*
#include "Global.h"
#include "classes/IoTItem.h"
#include <ctime>

class IoTMath : public IoTItem {
private:

    time_t convertTime(float day, float month, float year, float hour, float minute) {
        // Преобразование из float в int
        int d = static_cast<int>(day);
        int m = static_cast<int>(month);
        int y = static_cast<int>(year);
        int h = static_cast<int>(hour);
        int min = static_cast<int>(minute);

        if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1900 || h < 0 || h > 23 || min < 0 || min > 59) {
            SerialPrint("E", F("IoTMath"), F("Invalid date or time parameters!"));
            return -1;
        }

        // Структура для хранения даты и времени
        struct tm t;
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = min;
        t.tm_sec = 0;
        t.tm_isdst = -1;  // Пусть система сама определяет DST

        return mktime(&t);
    }

    bool nowInTimePeriod(String startTime, String endTime) {
        int h1 = selectToMarker(startTime, ":").toInt();
        int min1 = selectToMarkerLast(startTime, ":").toInt();
        int h2 = selectToMarker(endTime, ":").toInt();
        int min2 = selectToMarkerLast(endTime, ":").toInt();

        int sumMin1 = h1 * 60 + min1;
        int sumMin2 = h2 * 60 + min2;

        int nowMinutes = _time_local.hour * 60 + _time_local.minute;

        if (sumMin1 <= sumMin2) {
            return nowMinutes >= sumMin1 && nowMinutes <= sumMin2;
        } else {
            return nowMinutes >= sumMin1 && nowMinutes <= 24 * 60 || nowMinutes >= 0 && nowMinutes <= sumMin2;
        }
    }


public:
    IoTMath(String parameters) : IoTItem(parameters) {}

    IoTValue execute(String command, std::vector<IoTValue> &param) {
        if(command == "map" && param.size() == 5) {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = map(param[0].valD, param[1].valD, param[2].valD, param[3].valD, param[4].valD);
            //SerialPrint("i", F("IoTMath"), F("Mapping value done."));
            return valTmp;
        } else if(command == "convertTime" && param.size() == 5) {
            uint32_t unixTime = convertTime(param[0].valD, param[1].valD, param[2].valD, param[3].valD, param[4].valD);

            if (unixTime == -1) {
                SerialPrint("E", F("IoTMath"), F("Failed to convert time."));
                return {};
            }

            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = static_cast< float > (unixTime);
            return valTmp;
        } else if(command == "nowInTimePeriod" && param.size() == 2) {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = nowInTimePeriod(param[0].valS, param[1].valS); 
            return valTmp;
        }
        // Вставляем внутрь метода execute() класса IoTMath:
else if (command == "parseTimeToMinutes" && param.size() == 1) {
        // Если переданное значение уже является числом (например, напрямую из valD элемента Runtime)
        if (param[0].isDecimal) {
            return param[0]; 
        }

        String timeStr = param[0].valS;
        timeStr.trim();
        int colonIdx = timeStr.indexOf(':');
        
        IoTValue valTmp;
        valTmp.isDecimal = true;
        
        if (colonIdx == -1) {
            // Если двоеточия нет, попробуем просто перевести строку в float (на случай, если пришло чистое число минут)
            valTmp.valD = timeStr.toFloat();
        } else {
            int hours = timeStr.substring(0, colonIdx).toInt();
            int minutes = timeStr.substring(colonIdx + 1).toInt();
            valTmp.valD = static_cast<float>((hours * 60) + minutes);
        }
        return valTmp;
    }

        SerialPrint("E", F("IoTMath"), F("Unknown command or wrong parameters."));
        return {};
    }
};

void *getAPI_IoTMath(String subtype, String param) {
    if (subtype == F("IoTMath")) {
        return new IoTMath(param);
    }
    return nullptr;
}
*/
#include "Global.h"
#include "classes/IoTItem.h"
#include <ctime>
#include <cmath>

class IoTMath : public IoTItem {
private:

    // Извлечение конкретной цифры из числа по запрашиваемому разряду
    int parseDigit(float value, int position, bool hideLeadingZeros = true) {
        // Проверка на знак (-)
        if (position == -99) {
            return (value < 0) ? 1 : 0;
        }

        float absVal = std::fabs(value);
        long long integerPart = static_cast<long long>(absVal);

        // Целая часть (позиции >= 0: 0-единицы, 1-десятки, 2-сотни, 3-тысячи и т.д.)
        if (position >= 0) {
            if (hideLeadingZeros && position > 0) {
                long long threshold = 1;
                for (int i = 0; i < position; i++) {
                    threshold *= 10;
                }
                // Если число меньше порога разряда — это незначащий ноль
                if (integerPart < threshold) {
                    return -1; // Сигнал "разряд отсутствует"
                }
            }

            for (int i = 0; i < position; i++) {
                integerPart /= 10;
            }
            return static_cast<int>(integerPart % 10);
        } 
        // Дробная часть (позиции < 0: -1-десятые, -2-сотые и т.д.)
        else {
            int precision = std::abs(position);
            float shifted = absVal * std::pow(10.0f, precision);
            
            // roundf() предотвращает погрешности float (например, 0.399999 -> 0.4)
            long long shiftedInt = static_cast<long long>(std::round(shifted));
            
            return static_cast<int>(shiftedInt % 10);
        }
    }

    time_t convertTime(float day, float month, float year, float hour, float minute) {
        int d = static_cast<int>(day);
        int m = static_cast<int>(month);
        int y = static_cast<int>(year);
        int h = static_cast<int>(hour);
        int min = static_cast<int>(minute);

        if (d < 1 || d > 31 || m < 1 || m > 12 || y < 1900 || h < 0 || h > 23 || min < 0 || min > 59) {
            SerialPrint("E", F("IoTMath"), F("Invalid date or time parameters!"));
            return -1;
        }

        struct tm t;
        t.tm_year = y - 1900;
        t.tm_mon = m - 1;
        t.tm_mday = d;
        t.tm_hour = h;
        t.tm_min = min;
        t.tm_sec = 0;
        t.tm_isdst = -1;

        return mktime(&t);
    }

    bool nowInTimePeriod(String startTime, String endTime) {
        int h1 = selectToMarker(startTime, ":").toInt();
        int min1 = selectToMarkerLast(startTime, ":").toInt();
        int h2 = selectToMarker(endTime, ":").toInt();
        int min2 = selectToMarkerLast(endTime, ":").toInt();

        int sumMin1 = h1 * 60 + min1;
        int sumMin2 = h2 * 60 + min2;

        int nowMinutes = _time_local.hour * 60 + _time_local.minute;

        if (sumMin1 <= sumMin2) {
            return nowMinutes >= sumMin1 && nowMinutes <= sumMin2;
        } else {
            return (nowMinutes >= sumMin1 && nowMinutes <= 24 * 60) || (nowMinutes >= 0 && nowMinutes <= sumMin2);
        }
    }

public:
    IoTMath(String parameters) : IoTItem(parameters) {}

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "map" && param.size() == 5) {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = map(param[0].valD, param[1].valD, param[2].valD, param[3].valD, param[4].valD);
            return valTmp;
        } 
        else if (command == "parse" && param.size() >= 2) {
            float val = param[0].valD;
            int pos = static_cast<int>(param[1].valD);
            bool hideZeros = true;

            // Третий необязательный параметр: 0 = сохранять лидирующие нули, 1 = скрывать (по умолчанию 1)
            if (param.size() >= 3) {
                hideZeros = (param[2].valD != 0);
            }

            int resultDigit = parseDigit(val, pos, hideZeros);

            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = static_cast<float>(resultDigit);
            valTmp.valS = String(resultDigit);
            return valTmp;
        }
        else if (command == "convertTime" && param.size() == 5) {
            uint32_t unixTime = convertTime(param[0].valD, param[1].valD, param[2].valD, param[3].valD, param[4].valD);

            if (unixTime == (uint32_t)-1) {
                SerialPrint("E", F("IoTMath"), F("Failed to convert time."));
                return {};
            }

            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = static_cast<float>(unixTime);
            return valTmp;
        } 
        else if (command == "nowInTimePeriod" && param.size() == 2) {
            IoTValue valTmp;
            valTmp.isDecimal = true;
            valTmp.valD = nowInTimePeriod(param[0].valS, param[1].valS); 
            return valTmp;
        }
else if (command == "parseTimeToMinutes" && param.size() == 1) {
            if (param[0].isDecimal) {
                return param[0]; 
            }

            String timeStr = param[0].valS;
            timeStr.trim();
            
            long days = 0;
            int dayIdx = timeStr.indexOf('d');
            
            // Если в строке есть дни (например "1d 02:01" или "2d05:30")
            if (dayIdx != -1) {
                days = timeStr.substring(0, dayIdx).toInt();
                timeStr = timeStr.substring(dayIdx + 1);
                timeStr.trim();
            }

            int colonIdx = timeStr.indexOf(':');
            
            IoTValue valTmp;
            valTmp.isDecimal = true;
            
            if (colonIdx == -1) {
                valTmp.valD = timeStr.toFloat() + static_cast<float>(days * 1440);
            } else {
                long hours = timeStr.substring(0, colonIdx).toInt();
                long minutes = timeStr.substring(colonIdx + 1).toInt();
                valTmp.valD = static_cast<float>((days * 1440) + (hours * 60) + minutes);
            }
            return valTmp;
        }

        SerialPrint("E", F("IoTMath"), F("Unknown command or wrong parameters."));
        return {};
    }
};

void *getAPI_IoTMath(String subtype, String param) {
    if (subtype == F("IoTMath")) {
        return new IoTMath(param);
    }
    return nullptr;
}
