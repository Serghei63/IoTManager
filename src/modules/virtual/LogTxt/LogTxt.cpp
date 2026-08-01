#include "Global.h"
#include "classes/IoTItem.h"

class LogTxt : public IoTItem {
private:
    String _filePath;
    String _ids;

public:
    LogTxt(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, F("id"), _id);
        jsonRead(parameters, F("path"), _filePath);
        jsonRead(parameters, F("ids"), _ids);

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

        String currentPath = _filePath;
        if (currentPath == "") {
            currentPath = "/db/" + String(dateBuf) + ".txt";
        }

        // 3. Проверка директории /db и файла
        if (!FileFS.exists(currentPath)) {
            if (currentPath.startsWith("/db/")) {
                #if defined(ESP32) || defined(ESP8266)
                if (!LittleFS.exists("/db")) {
                    LittleFS.mkdir("/db");
                }
                #endif
            }

            String formattedHeader = _ids;
            formattedHeader.replace(',', ';');
            String header = "Time;" + formattedHeader + "\n";

            if (writeEmptyFile(currentPath) == "success") {
                addFile(currentPath, header);
                SerialPrint("I", F("LogTxt"), "'" + _id + "' Created daily log: " + currentPath);
            } else {
                SerialPrint("E", F("LogTxt"), "'" + _id + "' Failed to create file: " + currentPath);
                return;
            }
        }

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