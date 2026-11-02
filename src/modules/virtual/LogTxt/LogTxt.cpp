#include "Global.h"
#include "classes/IoTItem.h"

class LogTxt : public IoTItem {
private:
    String _filePath;
    String _ids;
    int _points = 0; // Максимальное количество точек (строк) в файле

    // Вспомогательный метод для подсчета строк в файле
    int countLines(const String& path) {
        if (!FileFS.exists(path)) return 0;
        
        File file = FileFS.open(path, "r");
        if (!file) return 0;

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
    LogTxt(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, F("id"), _id);
        jsonRead(parameters, F("path"), _filePath);
        jsonRead(parameters, F("ids"), _ids);
        jsonRead(parameters, F("points"), _points, false); // Читаем лимит точек из веб-интерфейса

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

        // 1. Вычисляем текущее локальное время
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

        // 2. Путь к файлу
        String currentPath = _filePath;
        if (currentPath == "") {
            currentPath = "/db/" + String(dateBuf) + ".txt";
        }

        // 3. Проверка лимита точек (строк) в существенном файле
        if (_points > 0 && FileFS.exists(currentPath)) {
            int currentLines = countLines(currentPath);
            // Учитываем, что 1-я строка — это заголовок Time;...
            if (currentLines >= (_points + 1)) {
                // Если превысили лимит точек, пересоздаем файл (или удаляем старый)
                FileFS.remove(currentPath);
                SerialPrint("I", F("LogTxt"), "'" + _id + "' Line limit (" + String(_points) + ") reached. Recreating file.");
            }
        }

        // 4. Проверка директории /db и создание файла при необходимости
        if (!FileFS.exists(currentPath)) {
            if (currentPath.startsWith("/db/")) {
                #if defined(ESP32) || defined(ESP8266)
                if (!LittleFS.exists("/db")) {
                    LittleFS.mkdir("/db");
                }
                #endif
            }

            // Заголовок формата CSV
            String formattedHeader = _ids;
            formattedHeader.replace(',', ';');
            String header = "Time;" + formattedHeader + "\n";

            if (writeEmptyFile(currentPath) == "success") {
                addFile(currentPath, header);
                SerialPrint("I", F("LogTxt"), "'" + _id + "' Created log file: " + currentPath);
            } else {
                SerialPrint("E", F("LogTxt"), "'" + _id + "' Failed to create file: " + currentPath);
                return;
            }
        }

        // 5. Сборка CSV строки с разделителем ';'
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

        // 6. Запись строки
        addFile(currentPath, logLine);
        SerialPrint("i", F("LogTxt"), "'" + _id + "' Logged to " + currentPath + ": " + logLine);
    }

    void setValue(const IoTValue &Value, bool genEvent = true) override {
        value = Value;
        writeLog();
    }

    void doByInterval() override {
        writeLog();
    }
};

void *getAPI_LogTxt(String subtype, String param) {
    if (subtype == F("LogTxt")) {
        return new LogTxt(param);
    }
    return nullptr;
}