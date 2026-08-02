#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include "NTP.h"
#include <SdFat.h>

extern SdFat sd;

// Декларация внешней функции из MqttClient.cpp для отправки графиков
bool publishChartFileToMqtt(String path, String id, int maxCount);
void *getAPI_SDDate(String params);

class SDLoging : public IoTItem
{
private:
    String logid;
    String id;
    int _publishType = -2;
    int _wsNum = -1;
    int points;
    IoTItem *dateIoTItem;

    String prevDate = "";
    bool firstTimeInit = true;
    String _logDir;

    // Единый внутренний метод для записи данных
    void processLogging(String value)
    {
        if (value == "" || value == "failed") return;

        if (!isTimeSynch) {
            SerialPrint("E", F("SDLoging"), "'" + id + "' Синхронизация времени отсутствует, запись пропущена");
            return;
        }

        regEvent(value, F("SDLoging"));

        String logData;
        jsonWriteInt(logData, "x", unixTime, false);
        jsonWriteFloat(logData, "y1", value.toFloat(), false);

        if (!sd.exists("/lg")) sd.mkdir("/lg");
        if (!sd.exists(_logDir.c_str())) sd.mkdir(_logDir.c_str());

        String filePath = "";
        if (isItemExist(id)) {
            filePath = readDataDB(id);
        }

        if (filePath == "failed" || filePath == "" || !sd.exists(filePath.c_str())) {
            createNewFileWithData(logData);
            return;
        } else {
            if (getTodayDateDotFormated() != getDateDotFormatedFromUnix(getFileUnixLocalTime(filePath))) {
                createNewFileWithData(logData);
                return;
            }
        }

        // Подсчет количества точек в файле
        int lines = 0;
        FsFile checkFile = sd.open(filePath.c_str(), O_RDONLY);
        if (checkFile) {
            while (checkFile.available()) {
                if (checkFile.read() == '}') lines++;
            }
            checkFile.close();
        }
        SerialPrint("i", F("SDLoging"), "'" + id + "' точек в текущем файле: " + String(lines));

        if (lines < points && !hasDayChanged()) {
            addNewDataToExistingFile(filePath, logData);
        } else {
            createNewFileWithData(logData);
        }
    }

public:
    SDLoging(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("logid"), logid);
        jsonRead(parameters, F("id"), id);
        jsonRead(parameters, F("points"), points);
        
        if (points > 2000) points = 2000;

        long interval;
        jsonRead(parameters, F("int"), interval);
        setInterval(interval * 60);

        _logDir = "/lg/" + id;

        // Создаем дочерний элемент даты
        dateIoTItem = (IoTItem *)getAPI_SDDate("{\"id\": \"" + id + "-date\",\"int\":\"20\",\"subtype\":\"sddate\"}");
        IoTItems.push_back(dateIoTItem);
        SerialPrint("I", F("SDLoging"), "'" + id + "' модуль инициализирован.");
    }

    void doByInterval()
    {
        if (!isItemExist(logid)) {
            SerialPrint("E", F("SDLoging"), "'" + id + "' целевой датчик не существует");
            return;
        }
        processLogging(getItemValue(logid));
    }

    void setValue(const IoTValue &Value, bool genEvent = true)
    {
        value = Value;
        processLogging(String(value.valD));
    }

    void createNewFileWithData(String &logData)
    {
        logData = logData + ",";
        String path = _logDir + "/" + String(unixTime) + ".txt";

        FsFile file = sd.open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (!file) {
            SerialPrint("E", F("SDLoging"), "'" + id + "' Ошибка создания файла: " + path);
            return;
        }

        file.print(logData);
        file.flush();
        file.close();

        saveDataDB(id, path);
        SerialPrint("i", F("SDLoging"), "'" + id + "' Новый файл дня создан -> " + path);
    }

    void addNewDataToExistingFile(String &path, String &logData)
    {
        logData = logData + ",";
        
        FsFile file = sd.open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND); 
        if (!file) {
            SerialPrint("E", F("SDLoging"), "'" + id + "' Ошибка записи в файл: " + path);
            return;
        }

        file.print(logData);
        file.flush();
        file.close();
       
        SerialPrint("i", F("SDLoging"), "'" + id + "' Точка добавлена -> " + path);
    }

    bool hasDayChanged()
    {
        bool changed = false;
        String currentDate = getTodayDateDotFormated();
        if (!firstTimeInit) {
            if (prevDate != currentDate) {
                changed = true;
                SerialPrint("i", F("NTP"), F("Смена суток обнаружена"));
            }
        }
        firstTimeInit = false;
        prevDate = currentDate;
        return changed;
    }

    // Умная публикация истории с привязкой к выбранной пользователем дате
    void publishValue()
    {
        // Узнаем какую дату выбрал пользователь в приложении (формат ДД.ММ.ГГГГ)
        String userDate = getItemValue(id + "-date");
        if (userDate == "") userDate = getTodayDateDotFormated();

        String filePath = "";

        // Ищем на SD-карте файл, соответствующий выбранной дате
        auto dir = sd.open(_logDir.c_str());
        if (dir) {
            while (true) {
                auto entry = dir.openNextFile();
                if (!entry) break;
                
                if (!entry.isDir()) {
                    char name[64];
                    entry.getName(name, sizeof(name));
                    String fileName = String(name);
                    
                    if (fileName.endsWith(".txt")) {
                        String fullPath = _logDir + "/" + fileName;
                        // Проверяем дату создания файла
                        if (getDateDotFormatedFromUnix(getFileUnixLocalTime(fullPath)) == userDate) {
                            filePath = fullPath;
                            entry.close();
                            break;
                        }
                    }
                }
                entry.close();
            }
            dir.close();
        }

        // Если файл под архивную дату не найден, но это сегодняшний день — берем активный файл
        if (filePath == "" && userDate == getTodayDateDotFormated()) {
            filePath = readDataDB(id);
        }

        if (filePath == "" || !sd.exists(filePath.c_str())) {
            SerialPrint("i", F("SDLoging"), "Нет данных на SD для даты: " + userDate);
            clearValue();
            return;
        }

        SerialPrint("i", F("SDLoging"), "Выгрузка истории для даты " + userDate + " из файла: " + filePath);
        
        // Отправка в MQTT для обновления графика в приложении
        publishChartFileToMqtt(filePath, id, 100);

        // Резервная отправка в локальный WebSocket (если приложение подключено локально)
        if (_publishType == TO_WS || _publishType == TO_MQTT_WS || _publishType == -2) {
            sendSDFileToWsByFrames(filePath, "charta", getAdditionalJson(), _wsNum);
        }
    }

void sendSDFileToWsByFrames(String path, String page, String topicJson, int wsNum) {
        FsFile file = sd.open(path.c_str(), O_RDONLY);
        if (!file) return;

        // Создаем изменяемую строку для стартового JSON
        String startJson = "{\"maxCount\":86400,\"topic\":\"" + mqttRootDevice + "/" + id + "\",\"status\":[";
        sendStringToWs(page, startJson, wsNum);

        String buffer = "";
        buffer.reserve(512);

        while (file.available()) {
            buffer += (char)file.read();
            if (buffer.length() >= 500) {
                sendStringToWs(page, buffer, wsNum);
                buffer = "";
                delay(10); 
            }
        }
        if (buffer.length() > 0) {
            sendStringToWs(page, buffer, wsNum);
        }

        // Создаем изменяемую строку для закрытия JSON-массива
        String endJson = "]}";
        sendStringToWs(page, endJson, wsNum);
        file.close();
    }

    String getAdditionalJson() {
        return "{\"maxCount\":86400,\"topic\":\"" + mqttRootDevice + "/" + id + "\"}";
    }

void publishChartToWsSinglePoint(String value) {
        String json = "{\"maxCount\":86400,\"topic\":\"" + mqttRootDevice + "/" + id + "\",\"status\":[{\"x\":" + String(unixTime) + ",\"y1\":" + value + "}]}";
        sendStringToWs("chartb", json, -1);
    }

void clearValue() {
        String json = "{\"maxCount\":0,\"topic\":\"" + mqttRootDevice + "/" + id + "\",\"status\":[]}";
        sendStringToWs("chartb", json, -1);
    }

    void setPublishDestination(int publishType, int wsNum) {
        _publishType = publishType;
        _wsNum = wsNum;
    }

    void regEvent(const String &value, const String &consoleInfo, bool error = false, bool genEvent = true) {
        if (getItemValue(id + "-date") == getTodayDateDotFormated()) {
            publishChartToWsSinglePoint(value);
        }
    }

unsigned long getFileUnixLocalTime(String path) { 
        int lastSlash = path.lastIndexOf('/');
        int lastDot = path.lastIndexOf('.');
        
        if (lastSlash != -1 && lastDot != -1 && lastDot > lastSlash) {
            String nameOnly = path.substring(lastSlash + 1, lastDot);
            unsigned long rawTime = strtoul(nameOnly.c_str(), NULL, 10);
            return gmtTimeToLocal(rawTime);
        }
        return 0;
    }
    
    String getValue() { return ""; }
    ~SDLoging() {};
};

// ========================================================================
// Сателлит Выбора Даты
// ========================================================================
class SDDate : public IoTItem
{
private:
    bool firstTime = true;

public:
    String id;
    SDDate(String parameters) : IoTItem(parameters)
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
        
        // Принудительно заставляем связанный логгер пересчитать файлы и выдать нужный день
        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            if ((*it)->getSubtype() == "SDLoging" && (*it)->getID() == selectToMarker(id, "-")) {
                (*it)->setPublishDestination(TO_MQTT_WS, -1);
                (*it)->publishValue();
            }
        }
    }

    void doByInterval()
    {
        if (isTimeSynch && firstTime) {
            setValue(getTodayDateDotFormated());
            firstTime = false;
        }
    }
};

void *getAPI_SDDate(String param) { return new SDDate(param); }

void *getAPI_SDLoging(String subtype, String param)
{
    if (subtype == F("SDLoging")) return new SDLoging(param);
    return nullptr;
}
