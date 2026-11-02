#include "Global.h"
#include "classes/IoTItem.h"
#include "ESPConfiguration.h"
#include "MqttClient.h"
#include "WsServer.h"



class Pie : public IoTItem
{
private:
    String id1;
    String id2;
    String id3;
    String id;
    String labels = "";
   
    String label1;
    String label2;
    String label3;

    int _publishType = -2;
    int _wsNum = -1;

public:
    Pie(String parameters) : IoTItem(parameters)
    {
        jsonRead(parameters, F("id1"), id1);
        jsonRead(parameters, F("id2"), id2);
        jsonRead(parameters, F("id3"), id3);
        jsonRead(parameters, F("id"), id);
        jsonRead(parameters, F("labels"), labels);

        jsonRead(parameters, F("label1"), label1);
        jsonRead(parameters, F("label2"), label2);
        jsonRead(parameters, F("label3"), label3);

         // Задаем дефолтные значения, если на вебе поля оставили пустыми
        if (label1 == "") label1 = "Сектор 1";
        if (label2 == "") label2 = "Сектор 2";
        if (label3 == "") label3 = "Сектор 3";
    }
    void doByInterval()
    {
        String value = getItemValue(id);
        if (id == "")
        {
            SerialPrint("E", F("Pie"), "'" + id + "' value is empty, return");
            return;
        }

        String value1 = getItemValue(id1);
        if (id1 == "")
        {
            SerialPrint("E", F("Pie"), "'" + id1 + "' value is empty, return");
            return;
        }

        String value2 = getItemValue(id2);
        if (id2 == "")
        {
            SerialPrint("E", F("Pie"), "'" + id2 + "' value is empty, return");
            return;
        }

        String value3 = getItemValue(id3);
        if (id3 == "")
        {
            SerialPrint("E", F("Pie"), "'" + id3 + "' value is empty, return");
            return;
        }

        regEvent(value, F("Pie"));
        publishValue();
    }

    void publishValue()
    {
        String value1 = getItemValue(id1);
        String value2 = getItemValue(id2);
        String value3 = getItemValue(id3);

        if (value1 == "" || value2 == "" || value3 == "")
        {
            SerialPrint("E", F("Pie"), "'" + id + "' one or more source values are empty, MQTT not sent");
            return;
        }

        String topic = mqttRootDevice + "/" + id;

          // Собираем валидный JSON-пакет, где status — это массив чисел, а descr (или labels) — массив строк
         String json = "{\"topic\":\"" + topic + "\",\"status\":[" + value1 + "," + value2 + "," + value3 + "],\"labels\":[\"" + label1 + "\",\"" + label2 + "\",\"" + label3 + "\"]}";

        bool sendMqtt = (_publishType == TO_MQTT || _publishType == TO_MQTT_WS || _publishType == -2);
        bool sendWs = (_publishType == TO_WS || _publishType == TO_MQTT_WS);

        if (sendMqtt)
        {
            publishChartMqtt(id, json);
            
        }

        if (sendWs)
        {
            sendStringToWs("chartb", json, _wsNum);
        }
    }
    

    void publishChartToWsSinglePoint(String value)
    {
        String topic = mqttRootDevice + "/" + id;
        String value1 = getItemValue(id1);
        String value2 = getItemValue(id2);
        String value3 = getItemValue(id3);
        String json = "{\"topic\":\"" + topic + "\",\"status\":[" + value1 + "," + value2 + "," + value3 + "]}";
        sendStringToWs("chartb", json, -1);
    }

    void clearValue()
    {
        String topic = mqttRootDevice + "/" + id;
        String json = "{\"topic\":\"" + topic + "\",\"status\":[]}";
        sendStringToWs("chartb", json, -1);
    }


    void setPublishDestination(int publishType, int wsNum)
    {
        _publishType = publishType;
        _wsNum = wsNum;
    }

    String getValue()
    {
        return "";
    }

    void regEvent(const String &value, const String &consoleInfo, bool error = false, bool genEvent = true)
    {
    }

};

void *getAPI_Pie(String subtype, String param){
    if (subtype == F("Pie")){
        return new Pie(param);
    }

        return nullptr;

}
