#include "Global.h"
#include "classes/IoTItem.h"
#include <SdFat.h>

// Если в проекте уже есть глобальный объект SD (из глобального модуля SD):
extern SdFat sd; // или extern SdFs sd; в зависимости от вашей версии SdFat

class LogSd : public IoTItem {
private:
    String _filePath;
    String _ids;
    int _points = 0;

    // Вспомогательный метод для дозаписи через SdFat
    bool appendToFileSD(const String& path, const String& data) {
        FsFile file; // Для версии SdFat v2+ (FsFile универсален)
        // Открываем на запись, создаем если нет, пишем в конец файла (AT_END)
        if (!file.open(path.c_str(), O_WRONLY | O_CREAT | O_AT_END)) {
            return false;
        }
        file.print(data);
        file.close();
        return true;
    }

    // Подсчет строк с помощью SdFat
    int countLinesSD(const String& path) {
        FsFile file;
        if (!file.open(path.c_str(), O_READ)) return 0;

        int lines = 0;
        while (file.available()) {
            if (file.read() == '\n') {
                lines++;
            }
        }
        file.close();
        return lines;
    }

public:
    LogSd(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, F("id"), _id);
        jsonRead(parameters, F("path"), _filePath);
        jsonRead(parameters, F("ids"), _ids);
        jsonRead(parameters, F("points"), _points, false);

        long interval = 0;
        jsonRead(parameters, F("int"), interval, false); 
        if (interval > 0) {
            setInterval(interval * 60);
        } else {
            setInterval(0);
        }
    }

    void writeLog() {
        if (!isTimeSynch || _ids.length() == 0) return;

        // 1. Текущее время
        time_t now = time(nullptr);
        int tz = 3;
        #if defined(settingsFlashJson)
            jsonRead(settingsFlashJson, F("timezone"), tz, false);
        #endif
        now += tz * 3600;

        struct tm* timeinfo = gmtime(&now);

        char dateBuf[12];
        char timeBuf[10];
        snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        // 2. Имя файла на SD
        String currentPath = _filePath;
        if (currentPath == "") {
            currentPath = "/db/" + String(dateBuf) + ".csv";
        }

        // 3. Лимит точек (строк)
        if (_points > 0 && sd.exists(currentPath.c_str())) {
            int currentLines = countLinesSD(currentPath);
            if (currentLines >= (_points + 1)) {
                sd.remove(currentPath.c_str());
                SerialPrint("I", F("LogSd"), "'" + _id + "' Line limit (" + String(_points) + ") reached. Recreating file.");
            }
        }

        // 4. Проверка директории и файла
        if (!sd.exists(currentPath.c_str())) {
            if (currentPath.startsWith("/db/") && !sd.exists("/db")) {
                sd.mkdir("/db");
            }

            // Форматируем заголовок Time;id1;id2...
            String formattedHeader = _ids;
            formattedHeader.replace(',', ';');
            String header = "Time;" + formattedHeader + "\n";

            if (appendToFileSD(currentPath, header)) {
                SerialPrint("I", F("LogSd"), "'" + _id + "' Created log file on SD: " + currentPath);
            } else {
                SerialPrint("E", F("LogSd"), "'" + _id + "' Failed to create file on SD! Check card.");
                return;
            }
        }

        // 5. Сборка CSV строки
        String logLine = String(timeBuf);
        String tmpIds = _ids;

        while (tmpIds.length() > 0) {
            int commaIndex = tmpIds.indexOf(',');
            String currentId;

            if (commaIndex != -1) {
                currentId = tmpIds.substring(0, commaIndex);
                tmpIds = tmpIds.substring(commaIndex + 1);
            } else {
                currentId = tmpIds;
                tmpIds = ""; 
            }

            currentId.trim();

            if (currentId.length() > 0) {
                if (isItemExist(currentId)) {
                    logLine += ";" + getItemValue(currentId);
                } else {
                    logLine += ";null";
                }
            }
            
            yield(); 
        }

        logLine += "\n";

        // 6. Запись в SdFat
        if (appendToFileSD(currentPath, logLine)) {
            SerialPrint("i", F("LogSd"), "'" + _id + "' Logged to SD " + currentPath + ": " + logLine);
        } else {
            SerialPrint("E", F("LogSd"), "'" + _id + "' Write error to SD!");
        }
    }

    void setValue(const IoTValue &Value, bool genEvent = true) override {
        value = Value;
        writeLog();
    }

    void doByInterval() override {
        writeLog();
    }
};

void *getAPI_LogSd(String subtype, String param) {
    if (subtype == F("LogSd")) {
        return new LogSd(param);
    }
    return nullptr;
}