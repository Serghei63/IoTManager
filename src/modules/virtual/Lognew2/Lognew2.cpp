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

    int _publishType = -2;
    int _wsNum = -1;
    int days = 5;
    int daysShow = 1;

    int points = 300;   

    IoTItem *dateIoTItem = nullptr;

    String prevDate = "";
    bool firstTimeInit = true;

    String getSecTime() {
        if (unixTime > 9999999999ULL) {
            return String(unixTime / 1000ULL);
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
            SerialPrint("E", F("Lognew2"), "'" + id + "' user set more points than allowed, value reset to 300");
        }
        
        long interval = 1;
        jsonRead(parameters, F("int"), interval); // в минутах 
        setInterval(interval * 60);

        jsonRead(parameters, F("daysSave"), days);
        jsonRead(parameters, F("daysShow"), daysShow); 
        
        if (days <= 0) days = 5; 
        if (daysShow <= 0) daysShow = 1;

        jsonRead(parameters, F("typeChart"), typeChart); 
        jsonRead(parameters, F("label1"), label1);
        jsonRead(parameters, F("label2"), label2);
        
        if (typeChart == "") typeChart = "line";
        if (label1 == "") label1 = "Линия 1";
        if (label2 == "") label2 = "Линия 2";

        // Создаем экземпляр класса даты
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

        if (hasDayChanged())
            deleteOldFile();

        regEvent(value, F("Lognew2"));

        String secTime = getSecTime();
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
            if (getTodayDateDotFormated() != getDateDotFormatedFromUnix(getFileUnixLocalTime(filePath)))
            {
                SerialPrint("i", F("Lognew2"), "'" + id + "' Old file detected, creating new log");
                createNewFileWithData(logData2);
                return;
            }
        }

        size_t size = 0;
        int lines = countJsonObj(filePath, size);

        // Если лимит 300 точек не превышен и день тот же - пишем в этот же файл
        if (lines < points && !hasDayChanged())
        {
            addNewDataToExistingFile(filePath, logData2);
        }
        else
        {
            // При наступлении 301-й точки создаем НОВЫЙ файл за эти же сутки!
            SerialPrint("i", F("Lognew2"), "'" + id + "' Reached " + String(lines) + " points. Creating segment file.");
            createNewFileWithData(logData2);
        }
        deleteLastFile();
    }

    void createNewFileWithData(String &logData)
    {
        logData = logData + ",";
        String path = "/lg/" + id + "/" + String(unixTimeShort) + ".txt";
        
        if (writeEmptyFile(path) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' File writing error");
            return;
        }
        
        if (addFile(path, logData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' DB update failed");
            return;
        }

        if (saveDataDB(id, path) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' db file writing error, return");
            return;
        }
    }

    void addNewDataToExistingFile(String &path, String &logData)
    {
        logData = logData + ",";
        if (addFile(path, logData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' Append data error");
            return;
        }
    }

    bool hasDayChanged()
    {
        bool changed = false;
        String currentDate = getTodayDateDotFormated();
        
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

    // ==================== СКЛЕЙКА ФАЙЛОВ И ПУБЛИКАЦИЯ ====================
    void publishValue()
    {
        String dir = "/lg/" + id;
        filesList = getFilesList(dir);

        SerialPrint("i", F("Lognew2"), "file list: " + filesList);

        String combinedData = "";
        unsigned long reqUnixTime = strDateToUnix(getItemValue(id + "-date")); 
        unsigned long showIntervalSec = (unsigned long)daysShow * 86400;

        bool hasPoints = false;

        // Собираем точки из всех файлов за выбранный интервал времени
        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            String fullPath = dir + path;
            
            unsigned long fileUnixTimeLocal = getFileUnixLocalTime(fullPath);

            // Если файл попадает в диапазон запрашиваемых дней
            if (fileUnixTimeLocal >= (reqUnixTime > showIntervalSec ? reqUnixTime - showIntervalSec : 0) && fileUnixTimeLocal <= reqUnixTime + 86400)
            {
                File file = FileFS.open(fullPath, "r");
                if (file) {
                    String content = file.readString();
                    file.close();
                    if (content.length() > 0) {
                        combinedData += content;
                        hasPoints = true;
                    }
                }
            }
            filesList = deleteBeforeDelimiter(filesList, ";");
        }

        if (hasPoints)
        {
            if (combinedData.endsWith(",")) {
                combinedData.remove(combinedData.length() - 1);
            }

            String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";
            String topic = mqttRootDevice + "/" + id + "/status";

            // Собираем единый JSON из всех файлов
            String json = "{";
            json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
            json += "\"topic\":\"" + topic + "\",";
            json += "\"typeChart\":\"" + typeChart + "\",";
            json += "\"series\":" + seriesArray + ",";
            json += "\"status\":[" + combinedData + "]";
            json += "}";

            if (_publishType == TO_MQTT || _publishType == TO_MQTT_WS)
            {
                publishChartMqtt(id, json);
            }

            if (_publishType == TO_WS || _publishType == TO_MQTT_WS)
            {
                sendStringToWs("charta", json, _wsNum);
            }
            
            SerialPrint("i", F("Lognew2"), "Sent merged chart JSON (" + String(json.length()) + " bytes)");
        }
        else
        {
            clearValue();
        }
    }

    String getAdditionalJson()
    {
        String topic = mqttRootDevice + "/" + id;
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray;
        json += "}";

        return json;
    }

    void publishChartToWsSinglePoint(String value)
    {
        String topic = mqttRootDevice + "/" + id;
        String value2 = getItemValue(lognid2);
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray + ",";
        json += "\"status\":[{\"x\":" + String(unixTime) + ",\"y1\":" + value + ",\"y2\":" + value2 + "}]";
        json += "}";

        sendStringToWs("chartb", json, -1);
    }

    void clearValue()
    {
        String topic = mqttRootDevice + "/" + id + "/status";
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":0,";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray + ",";
        json += "\"status\":[]";
        json += "}";

        sendStringToWs("chartb", json, -1);
    }

    void clearHistory()
    {
        String dir = "/lg/" + id;
        cleanDirectory(dir);
    }

    void deleteLastFile()
    {
        IoTFSInfo tmp = getFSInfo();
        if (tmp.freePer <= 10.00)
        {
            String dir = "/lg/" + id;
            filesList = getFilesList(dir);
            int i = 0;
            while (filesList.length())
            {
                String path = selectToMarker(filesList, ";");
                path = dir + path;
                i++;
                if (i == 1)
                {
                    removeFile(path);
                    SerialPrint("!", "Lognew2", String(i) + ") " + path + " => oldest files been deleted");
                    return;
                }
                filesList = deleteBeforeDelimiter(filesList, ";");
            }
        }
    }

    void deleteOldFile()
    {
        String dir = "/lg/" + id;
        filesList = getFilesList(dir);
        int i = 0;
        
        // Переводим хранение дней в секунды корректно
        unsigned long maxAgeSec = (unsigned long)days * 86400;

        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            String fullPath = dir + path;
            
            unsigned long fileUnix = getFileUnixLocalTime(fullPath);
            i++;

            if (fileUnix < (unixTime > maxAgeSec ? unixTime - maxAgeSec : 0))
            {
                removeFile(fullPath);
                SerialPrint("i", "Lognew2", String(i) + ") " + fullPath + " => old file deleted");
            }

            filesList = deleteBeforeDelimiter(filesList, ";");
        }
    }

    void setPublishDestination(int publishType, int wsNum) override
    {
        _publishType = publishType;
        _wsNum = wsNum;
    }

    String getValue() override { return ""; }

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
        return 86400; 
    }

    unsigned long getFileUnixLocalTime(String path) { 
        return gmtTimeToLocal(selectToMarkerLast(deleteToMarkerLast(path, "."), "/").toInt() + START_DATETIME); 
    }

    void setValue(const IoTValue &Value, bool genEvent = true) override
    {
        value = Value;
        regEvent(value.valS, "Lognew2", false, genEvent);
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
/*
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

    int _publishType = -2;
    int _wsNum = -1;
    int days = 1;
    int daysShow = 1;

    int points = 300;   

    IoTItem *dateIoTItem = nullptr;

    String prevDate = "";
    bool firstTimeInit = true;

    String getSecTime() {
        if (unixTime > 9999999999ULL) {
            return String(unixTime / 1000ULL);
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
            SerialPrint("E", F("Lognew2"), "'" + id + "' user set more points than allowed, value reset to 300");
        }
        
        long interval = 1;
        jsonRead(parameters, F("int"), interval); // в минутах 
        setInterval(interval * 60);

        jsonRead(parameters, F("daysSave"), days);
        days = days * 86400;
        jsonRead(parameters, F("daysShow"), daysShow); 
        daysShow = daysShow * 86400;

        // Корректное чтение параметров подписи
        jsonRead(parameters, F("typeChart"), typeChart); 
        jsonRead(parameters, F("label1"), label1);
        jsonRead(parameters, F("label2"), label2);
        
        if (days == 0) days = 5 * 86400; 
        if (typeChart == "") typeChart = "line";
        if (label1 == "") label1 = "Линия 1";
        if (label2 == "") label2 = "Линия 2";

        // Создаем экземпляр класса даты
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

        if (hasDayChanged())
            deleteOldFile();

        regEvent(value, F("Lognew2"));

        String secTime = getSecTime();
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
            if (getTodayDateDotFormated() != getDateDotFormatedFromUnix(getFileUnixLocalTime(filePath)))
            {
                SerialPrint("i", F("Lognew2"), "'" + id + "' Old file detected, creating new log");
                createNewFileWithData(logData2);
                return;
            }
        }

        size_t size = 0;
        int lines = countJsonObj(filePath, size);
        SerialPrint("i", F("Lognew2"), "'" + id + "' lines = " + String(lines) + ", size = " + String(size));

        if (lines <= points && !hasDayChanged())
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
        logData = logData + ",";
        String path = "/lg/" + id + "/" + String(unixTimeShort) + ".txt";
        
        if (writeEmptyFile(path) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' File writing error");
            return;
        }
        
        if (addFile(path, logData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' DB update failed");
            return;
        }

        if (saveDataDB(id, path) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' db file writing error, return");
            return;
        }
    }

    void addNewDataToExistingFile(String &path, String &logData)
    {
        logData = logData + ",";
        if (addFile(path, logData) != "success")
        {
            SerialPrint("E", F("Lognew2"), "'" + id + "' Append data error");
            return;
        }
    }

    bool hasDayChanged()
    {
        bool changed = false;
        String currentDate = getTodayDateDotFormated();
        
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

        SerialPrint("i", F("Lognew2"), "file list: " + filesList);

        int f = 0;
        bool noData = true;

        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            path = dir + path;
            f++;
            
            unsigned long fileUnixTimeLocal = getFileUnixLocalTime(path);
            unsigned long reqUnixTime = strDateToUnix(getItemValue(id + "-date")); 

            if (fileUnixTimeLocal > reqUnixTime - daysShow && fileUnixTimeLocal < reqUnixTime + 86400)
            {
                noData = false;
                String json = getAdditionalJson(); // Передаст label1, label2, typeChart, maxCount, topic
                
                if (_publishType == TO_MQTT || _publishType == TO_MQTT_WS)
                {
                 String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";
    
                 // Передаем путь к файлу, id, maxCount, серии и текущий typeChart ("line" или "bar")
                 publishChartFileToMqtt(path, id, calculateMaxCount(), seriesArray, typeChart);
                }

                else if (_publishType == TO_WS)
                {
                    sendFileToWsByFrames(path, "charta", json, _wsNum, WEB_SOCKETS_FRAME_SIZE);
                }
                else if (_publishType == TO_MQTT_WS)
                {
                    sendFileToWsByFrames(path, "charta", json, _wsNum, WEB_SOCKETS_FRAME_SIZE);
                    //publishChartFileToMqtt(path, id, calculateMaxCount()); 
                    publishNewChartFileToMqtt(path);
                }
                
                SerialPrint("i", F("Lognew2"), String(f) + ") " + path + ", " + getDateTimeDotFormatedFromUnix(fileUnixTimeLocal) + ", sent");
            }
            else
            {
                SerialPrint("i", F("Lognew2"), String(f) + ") " + path + ", " + getDateTimeDotFormatedFromUnix(fileUnixTimeLocal) + ", skipped");
            }

            filesList = deleteBeforeDelimiter(filesList, ";");
        }

        if (noData)
        {
            clearValue();
        }
    }
     void publishNewChartFileToMqtt(String path)
   //void publishChartFileToMqtt(path, id, calculateMaxCount(), seriesArray, typeChart);
    {
        if (!FileFS.exists(path)) {
            SerialPrint("E", F("Lognew2"), "MQTT File not found: " + path);
            return;
        }

        File file = FileFS.open(path, "r");
        if (!file) {
            SerialPrint("E", F("Lognew2"), "MQTT File open error: " + path);
            return;
        }

        String fileData = file.readString();
        file.close();

        if (fileData.length() == 0) {
            SerialPrint("E", F("Lognew2"), "MQTT File is empty");
            return;
        }

        // Убираем лишнюю запятую на конце файла
        if (fileData.endsWith(",")) {
            fileData.remove(fileData.length() - 1);
        }

        // ВАЖНО: Суффикс /status обязателен для MQTT IoTmanager!
        String topic = mqttRootDevice + "/" + id + "/status";
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray + ",";
        json += "\"status\":[" + fileData + "]";
        json += "}";

        // Публикуем в топик .../logn21/status
        bool result = mqtt.publish(topic.c_str(), json.c_str(), false);
        
        if (result) {
            SerialPrint("i", F("Lognew2"), "MQTT OK -> " + topic + " (" + String(json.length()) + " bytes)");
        } else {
            SerialPrint("E", F("Lognew2"), "MQTT FAILED! Packet too large or disconnected. Size: " + String(json.length()));
        }
    }

String getAdditionalJson()
{
    String topic = mqttRootDevice + "/" + id;
    String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

    String json = "{";
    json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
    json += "\"topic\":\"" + topic + "\",";
    json += "\"typeChart\":\"" + typeChart + "\",";
    json += "\"series\":" + seriesArray;
    json += "}";

    return json;
}

    void publishChartToWsSinglePoint(String value)
    {
        String topic = mqttRootDevice + "/" + id;
        String value2 = getItemValue(lognid2);
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":" + String(calculateMaxCount()) + ",";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray + ",";
        json += "\"status\":[{\"x\":" + String(unixTime) + ",\"y1\":" + value + ",\"y2\":" + value2 + "}]";
        json += "}";

        sendStringToWs("chartb", json, -1);
    }

    void clearValue()
    {
        String topic = mqttRootDevice + "/" + id;
        String seriesArray = "[\"" + label1 + "\",\"" + label2 + "\"]";

        String json = "{";
        json += "\"maxCount\":0,";
        json += "\"topic\":\"" + topic + "\",";
        json += "\"typeChart\":\"" + typeChart + "\",";
        json += "\"series\":" + seriesArray + ",";
        json += "\"status\":[]";
        json += "}";

        sendStringToWs("chartb", json, -1);
    }

    void clearHistory()
    {
        String dir = "/lg/" + id;
        cleanDirectory(dir);
    }

    void deleteLastFile()
    {
        IoTFSInfo tmp = getFSInfo();
        if (tmp.freePer <= 10.00)
        {
            String dir = "/lg/" + id;
            filesList = getFilesList(dir);
            int i = 0;
            while (filesList.length())
            {
                String path = selectToMarker(filesList, ";");
                path = dir + path;
                i++;
                if (i == 1)
                {
                    removeFile(path);
                    SerialPrint("!", "Lognew2", String(i) + ") " + path + " => oldest files been deleted");
                    return;
                }
                filesList = deleteBeforeDelimiter(filesList, ";");
            }
        }
    }

    void deleteOldFile()
    {
        String dir = "/lg/" + id;
        filesList = getFilesList(dir);
        int i = 0;
        while (filesList.length())
        {
            String path = selectToMarker(filesList, ";");
            String pathTodel = path;
            pathTodel.replace("/", "");
            pathTodel.replace(".txt", "");
            int pathTodel_ = pathTodel.toInt();
            path = dir + path;
            i++;
            if (pathTodel_ < unixTimeShort - days)
            {
                removeFile(path);
                SerialPrint("i", "Lognew2", String(i) + ") " + path + " => old files clean");
            }

            filesList = deleteBeforeDelimiter(filesList, ";");
        }
    }

    void setPublishDestination(int publishType, int wsNum) override
    {
        _publishType = publishType;
        _wsNum = wsNum;
    }

    String getValue() override { return ""; }

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
        return 86400; 
    }

    unsigned long getFileUnixLocalTime(String path) { 
        return gmtTimeToLocal(selectToMarkerLast(deleteToMarkerLast(path, "."), "/").toInt() + START_DATETIME); 
    }

    void setValue(const IoTValue &Value, bool genEvent = true) override
    {
        value = Value;
        regEvent(value.valS, "Lognew2", false, genEvent);
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
*/