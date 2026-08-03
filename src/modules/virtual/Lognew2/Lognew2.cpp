#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include "NTP.h"

void *getAPI_Date2(String params);

class Lognew2 : public IoTItem
{
private:
    String lognid1;
    String lognid2;
    String id;
    String filesList = "";
    String typeChart = "line"; 

    String label1;
    String label2;

    int days = 1;
    int daysShow = 1;

    int _publishType = -2;
    int _wsNum = -1;

    int points = 300;

    IoTItem *dateIoTItem = nullptr;

    String prevDate = "";
    bool firstTimeInit = true;

    // Вспомогательный метод для получения корректного времени в секундах (10 знаков)
    String getSecTime() {
        if (unixTime > 9999999999ULL) {
            return String(unixTime / 1000ULL); // Корректировка, если прилетели мс
        }
        return String(unixTime);
    }

public:
    Lognew2(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("lognid1"), lognid1);
        jsonRead(parameters, F("lognid2"), lognid2);
        jsonRead(parameters, F("id"), id);
        jsonRead(parameters, F("points"), points);
        
        if (points > 300 || points <= 0)
        {
            points = 300;
        }
        
        long interval = 1;
        jsonRead(parameters, F("int"), interval); 
        setInterval(interval * 60);
        
        jsonRead(parameters, F("typeChart"), typeChart, false); 
        jsonRead(parameters, F("label1"), label1, false);
        jsonRead(parameters, F("label2"), label2, false);
        jsonRead(parameters, F("daysSave"), days, false); 
        jsonRead(parameters, F("daysShow"), daysShow, false);
        
        if (days == 0) days = 5; 
        if (typeChart == "") typeChart = "line";
        if (label1 == "") label1 = "Линия 1";
        if (label2 == "") label2 = "Линия 2";

        dateIoTItem = (IoTItem *)getAPI_Date2("{\"id\": \"" + id + "-date\",\"int\":\"20\",\"subtype\":\"date\"}");
        if (dateIoTItem) {
            IoTItems.push_back(dateIoTItem);
            SerialPrint("I", F("Lognew2"), "Created date instance " + id);
        }
    }

    void doByInterval() override
    {
        if (!isItemExist(lognid1))
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' logging object 1 does not exist");
            return;
        }

        String value = getItemValue(lognid1);
        String value2 = getItemValue(lognid2);
        
        if (value == "" || value2 == "")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' logging value is empty");
            return;
        }
        
        if (!isTimeSynch)
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' Time not synchronized, skipping log");
            return;
        }

        regEvent(value, F("Lognew2"));

        String secTime = getSecTime(); // Работаем с 10-значным Unix timestamp
        String logData2 = "{\"x\":" + secTime + ",\"y1\":" + String(value.toFloat()) + ",\"y2\":" + String(value2.toFloat()) + "}";

        String filePath = readDataDB(id);
        if (filePath == "failed" || filePath == "")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' File path not found, creating new log");
            createNewFileWithData(logData2);
            return;
        }
        else
        {
            unsigned long fileTime = getFileUnixLocalTime(filePath);
            unsigned long todayStartUnix = strDateToUnix(getTodayDateDotFormated());
            
            if (fileTime < todayStartUnix)
            {
                SerialPrint("i", F("Lognew2"), "'" + id + "' Old file detected, creating new log");
                createNewFileWithData(logData2);
                return;
            }
        }

        if (!hasDayChanged())
        {
            addNewDataToExistingFile(filePath, logData2);
        }
        else
        {
            createNewFileWithData(logData2);
        }
        
        deleteLastFile();
    }

    void createNewFileWithData(String &logData)
    {
        String formattedData = logData + ",";
        String path = "/lg/" + id + "/" + String(unixTime) + ".txt"; 
        
        if (writeEmptyFile(path) != "success" || addFile(path, formattedData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' File writing error");
            return;
        }
        
        if (saveDataDB(id, path) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' DB update failed");
            return;
        }
        SerialPrint("i", F("Lognew2"), "'" + id + "' New file created: " + path);
    }

    void addNewDataToExistingFile(String &path, String &logData)
    {
        String formattedData = logData + ",";
        if (addFile(path, formattedData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' Append data error");
            return;
        }
    }

    bool hasDayChanged()
    {
        bool changed = false;
        String currentDate = getTodayDateDotFormated(); // Берётся по локальному времени (localtime)
        
        if (!firstTimeInit)
        {
            if (prevDate != currentDate)
            {
                changed = true;
                SerialPrint("i", F("Lognew2"), F("Day change event detected"));
#if defined(ESP8266)
                FileFS.gc();
#endif
            }
        }
        firstTimeInit = false;
        prevDate = currentDate;
        return changed;
    }

    void publishValue()
    {
        String dir = "/lg/" + id;
        filesList = getFilesList(dir);
        bool noData = true;

        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            path = dir + path;
            
            unsigned long fileUnixTimeLocal = getFileUnixLocalTime(path);
            unsigned long reqUnixTime = strDateToUnix(getItemValue(id + "-date")); 

            unsigned long startRange = reqUnixTime - ((unsigned long)daysShow * 86400);
            unsigned long endRange = reqUnixTime + 86400;

            if (fileUnixTimeLocal >= startRange && fileUnixTimeLocal < endRange)
            {
                noData = false;
                String json = getAdditionalJson();
                
                if (_publishType == TO_MQTT)
                {
                    publishNewChartFileToMqtt(path); 
                }
                else if (_publishType == TO_WS)
                {
                    sendFileToWsByFrames(path, "charta", json, _wsNum, WEB_SOCKETS_FRAME_SIZE);
                }
                else if (_publishType == TO_MQTT_WS)
                {
                    sendFileToWsByFrames(path, "charta", json, _wsNum, WEB_SOCKETS_FRAME_SIZE);
                    publishNewChartFileToMqtt(path); 
                }
            }

            filesList = deleteBeforeDelimiter(filesList, ";");
        }
        
        if (noData)
        {
            clearValue();
        }
    }

    void publishNewChartFileToMqtt(String path) 
    {
        File logFile = FileFS.open(path, "r");
        if (!logFile) {
            SerialPrint("E", F("Lognew2"), "Failed to open log file for MQTT: " + path);
            return;
        }

        String statusArray = "[";
        while (logFile.available()) {
            String line = logFile.readStringUntil(','); 
            line.trim();
            if (line.length() == 0) continue;

            if (statusArray.length() > 1) statusArray += ",";
            statusArray += line;
        }
        logFile.close();
        
        if (statusArray.endsWith(",")) {
            statusArray.remove(statusArray.length() - 1);
        }
        statusArray += "]";

        String topic = mqttRootDevice + "/" + id;
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";
        
        String finalJson = "{";
        finalJson += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        finalJson += "\"topic\":\"" + topic + "\",";
        finalJson += "\"typeChart\":\"" + typeChart + "\","; 
        finalJson += "\"series\":" + seriesArray + ",";
        finalJson += "\"status\":" + statusArray;            
        finalJson += "}";

        String mqttPath = topic + "/status";
        mqtt.publish(mqttPath.c_str(), finalJson.c_str(), true); 
    }

    String getAdditionalJson()
    {
        String topic = mqttRootDevice + "/" + id;
        return "{\"maxCount\":" + String(calculateMaxCount()) + ",\"topic\":\"" + topic + "\"}";
    }

    void publishChartToWsSinglePoint(String value)
    {
        String topic = mqttRootDevice + "/" + id;
        String value2 = getItemValue(lognid2); 
        String secTime = getSecTime();
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";     
        json += "\"series\":" + seriesArray + ",";     
        json += "\"status\":[{\"x\":" + secTime + ",\"y1\":" + value + ",\"y2\":" + value2 + "}]";
        json += "}";

        sendStringToWs("chartb", json, -1);
    }

    void clearValue()
    {
        String topic = mqttRootDevice + "/" + id;
        
        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"status\":[]";
        json += "}";

        String mqttPath = topic + "/status";
        mqtt.publish(mqttPath.c_str(), json.c_str(), true); 
        sendStringToWs("chartb", json, -1); 
    }

    void clearHistory()
    {
        cleanDirectory("/lg/" + id);
    }

    void deleteLastFile()
    {
        String dir = "/lg/" + id; 
        filesList = getFilesList(dir);
        
        unsigned long maxAgeSec = (unsigned long)days * 86400;
        if (unixTime < maxAgeSec) return; 
        unsigned long limitUnixTime = unixTime - maxAgeSec;

        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            path = dir + path;
            
            unsigned long fileUnixTime = getFileUnixLocalTime(path);
            
            if (fileUnixTime < limitUnixTime)
            {
                removeFile(path);
                SerialPrint("i", "Lognew2", "'" + id + "' File " + path + " deleted (older than " + String(days) + " days)");
            }
            
            filesList = deleteBeforeDelimiter(filesList, ";");
        }
    }

    void setPublishDestination(int publishType, int wsNum) override
    {
        _publishType = publishType;
        _wsNum = wsNum;
    }

    String getValue() override
    {
        return "";
    }

    void regEvent(const String &value, const String &consoleInfo, bool error = false, bool genEvent = true) override
    {
        String userDate = getItemValue(id + "-date");
        String currentDate = getTodayDateDotFormated();
        if (userDate == currentDate)
        {
            publishChartToWsSinglePoint(value);
        }
    }

    int calculateMaxCount()
    {
        return (points > 0) ? points : 300;
    }

    unsigned long getFileUnixLocalTime(String path)
    {
        String fileName = selectToMarkerLast(deleteToMarkerLast(path, "."), "/");
        return fileName.toInt(); 
    }

    void setValue(const IoTValue &Value, bool genEvent = true) override
    {
        value = Value;
        // Значение обновлено. Логирование производится строго по интервалу (int)
    }
}; 

// ==================== КЛАСС КАЛЕНДАРЯ (ДАТЫ) ====================

class Date : public IoTItem
{
private:
    bool firstTime = true;

public:
    String id;
    Date(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("id"), id);
        value.isDecimal = false;
    }

    void setValue(const String &valStr, bool genEvent = true)
    {
        value.valS = valStr;
        setValue(value, genEvent);
    }

    void setValue(const IoTValue &Value, bool genEvent = true)
    {
        value = Value;
        regEvent(value.valS, "", false, genEvent);
        
        for (auto *item : IoTItems)
        {
            if (item->getSubtype() == "Lognew2" && item->getID() == selectToMarker(id, "-"))
            {
                item->setPublishDestination(TO_MQTT_WS, -1);
                item->publishValue();
            }
        }
    }

    void setTodayDate()
    {
        setValue(getTodayDateDotFormated());
    }

    void doByInterval() override
    {
        if (isTimeSynch && firstTime)
        {
            setTodayDate();
            firstTime = false;
        }
    }
};

void *getAPI_Lognew2(String subtype, String param) 
{
    if (subtype == F("Lognew2"))
    {
        return new Lognew2(param);
    }
    return nullptr;
}

void *getAPI_Date2(String param)
{
    return new Date(param);
}