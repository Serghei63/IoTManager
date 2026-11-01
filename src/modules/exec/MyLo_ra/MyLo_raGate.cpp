#include "Global.h"
#include "classes/IoTItem.h"
#include "Arduino.h"
#include "MyLo_raGate.h"

//#ifdef MYSENSORS

bool _debug; 
// callback библиотеки mysensors
void receive(const MyMessage& message) {
    String inMsg = String(message.getSender()) + "," +   // node-id
                   String(message.getSensor()) + "," +   // child-sensor-id
                   String(message.getType()) + "," +     // type of var
                   String(message.getCommand()) + "," +  // command
                   parseToString(message) + ";";         // value

    // Serial.println("=>" + inMsg);

    mysensorBuf += inMsg;
  
}


String parseToString(const MyMessage& message) {
    String value = "error";
    switch (message.getPayloadType()) {
        case 0:  // Payload type is string
            value = message.getString();
            return value;
        case 1:  // Payload type is byte
            value = String(message.getByte());
            return value;
        case 2:  // Payload type is INT16
            value = String(message.getInt());
            return value;
        case 3:  // Payload type is UINT16
            value = String(message.getUInt());
            return value;
        case 4:  // Payload type is INT32
            value = String(message.getInt());
            return value;
        case 5:  // Payload type is UINT32
            value = String(message.getUInt());
            return value;
        case 6:  // Payload type is binary
            value = String(message.getBool());
            return value;
        case 7:  // Payload type is float32
            value = String(message.getFloat());
            return value;
        default:
            return value;
    }
     return value; // <-- явный возврат на всякий случай
}

//#endif
class MyLo_raGate : public IoTItem {
   private:
   public:
    MyLo_raGate(String parameters) : IoTItem(parameters) { SerialPrint("i", "MyLo_ra", "Gate initialized"); }

    void doByInterval() {}

    void loop() { loopMyLo_raExecute(); }

          ~MyLo_raGate(){};

        IoTValue execute(String command, std::vector<IoTValue> &param) {

    if (command == "send") {
        
        if (param.size() >= 5) {
            uint8_t nodeId  = param[0].valD;
            uint8_t childId = param[1].valD;
            uint8_t type    = param[2].valD; // Передаваемый тип данных (V_...)
            uint8_t cmd     = param[3].valD; // Тип команды (C_SET=1, C_REQ=2)
            
            // Создаем пустой объект сообщения
            MyMessage msg;
            build(msg, nodeId, childId, (mysensors_command_t)cmd, (mysensors_data_t)type);

            bool ok = false;

            // Группируем типы данных MySensors по их формату
            switch (type) {
                // --- 1. BOOLEAN (Логические: 0 или 1) ---
                case V_STATUS:      // 2 (ранее V_LIGHT)
                case V_TRIPPED:     // 16 (датчики движения/открытия)
                case V_ARMED:       // 15
                case V_LOCK_STATUS: // 36
                    ok = send(msg.set((bool)param[4].valD));
                    break;

                // --- 2. FLOAT (Числа с плавающей точкой) ---
                case V_TEMP:        // 0 (Температура)
                case V_HUM:         // 1 (Влажность)
                case V_PRESSURE:    // 4 (Давление)
                case V_UV:          // 3 (УФ-индекс)
                case V_WEIGHT:      // 34 (Вес)
                case V_VOLTAGE:     // 38 (Напряжение)
                case V_CURRENT:     // 39 (Ток)
                    // Отправляем как float (указываем 2 знака после запятой)
                    ok = send(msg.set((float)param[4].valD, 2));
                    break;

                // --- 3. LONG / UNSIGNED LONG (32-битные числа) ---
                case V_VAR1:        // 24 (Пользовательские переменные)
                case V_VAR2:        // 25
                case V_VAR3:        // 26
                case V_VAR4:        // 27
                case V_VAR5:        // 28
                case V_DISTANCE:    // 29 (Расстояние может быть большим)
                case V_LIGHT_LEVEL: // 37
                    // Важно для переменных, хранящих большие значения (pt=4)
                    ok = send(msg.set((uint32_t)param[4].valD));
                    break;

                // --- 4. STRING (Строки текста) ---
                case V_TEXT:        // 47
                case V_CUSTOM:      // 48
                case V_IR_SEND:     // 32
                case V_IR_RECEIVE:  // 33
                case V_RGB:         // 40 (Обычно передается как HEX строка, напр. "FF0000")
                case V_RGBW:        // 41
                    // *Примечание: Если структура IoTValue поддерживает строки (например, param[4].valS),
                    // используйте msg.set(param[4].valS.c_str()). Если нет, конвертируем значение в строку.
                    ok = send(msg.set(String(param[4].valD).c_str()));
                    break;

                // --- 5. INTEGER (Стандартные целые числа: 16-бит) ---
                // Сюда попадают V_PERCENTAGE(3), V_WATT(17), V_VOLUME(20), V_DIRECTION(12) и всё остальное
                default:
                    ok = send(msg.set((int)param[4].valD));
                    break;
            }

            SerialPrint(
                "i",
                "MySensors",
                "Send node=" + String(nodeId) +
                " child=" + String(childId) +
                " type=" + String(type) +
                " cmd=" + String(cmd) +
                " value=" + String(param[4].valD) + // Можно выводить исходное значение для логов
                " result=" + String(ok)
            );
        }
    }
    return {};
}

    void loopMyLo_raExecute() {    

        if (mysensorBuf.length()) {
            String tmp = selectToMarker(mysensorBuf, ";");

            String nodeId = selectFromMarkerToMarker(tmp, ",", 0);         // node-id
            String childSensorId = selectFromMarkerToMarker(tmp, ",", 1);  // child-sensor-id
            String type = selectFromMarkerToMarker(tmp, ",", 2);           // type of var
            String command = selectFromMarkerToMarker(tmp, ",", 3);        // command
            String value = selectFromMarkerToMarker(tmp, ",", 4);          // value

            static bool presentBeenStarted = false;

            String ID = "n" + nodeId + "s" + childSensorId;
            static String infoJson = "{}";

            if (childSensorId == "255") {
                if (command == "3") {    // это особое внутреннее сообщение
                    if (type == "11") {  // название ноды
                        SerialPrint("i", "MyLo_ra", "===================== " + value + " =====================");
                    }
                    if (type == "12") {  // версия ноды
                        SerialPrint("i", "MyLo_ra", "Node version: " + value);
                    }
                }
            } else {
                if (command == "0") {  // это презентация
                    presentBeenStarted = true;
                    int num;
                    String widget;
                    String descr;
                    sensorType(type.toInt(), num, widget, descr);

                    descr.replace("#", " ");
                    SerialPrint("i", "MyLo_ra", "Presentation: " + ID + ": " + descr);
                }
                if (command == "1") {  // это данные
                    if (value != "") {
                        if (presentBeenStarted) {
                            presentBeenStarted = false;
                            SerialPrint("i", "MyLo_ra", "===================== " + nodeId + " =====================");
                        }

                        bool found = false;

                        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
                            if ((*it)->getID() == ID) {
                                found = true;
                                (*it)->setValue(value, true);
                            }
                        }

                        SerialPrint("i", "MyLo_ra", "node: " + nodeId + ", sensor: " + childSensorId + ", command: " + command + ", type: " + type + ", val: " + value + ", found: " + String(found));
                    }
                }
                if (command == "2") {  // это запрос значения переменной
                    SerialPrint("i", "MyLo_ra", "Request a variable value");
                }
            }

            mysensorBuf = deleteBeforeDelimiter(mysensorBuf, ";");
        }
    }

    void sensorType(int index, int& num, String& widget, String& descr) {
            
                descr = F("Unknown");
                widget = F("anydata");
                num = 1;

            }

};

class MyLo_raNode : public IoTItem {
   private:
    String id = "";
    int orange = 0;
    int red = 0;
    int offline = 0;
    int _minutesPassed = 0;
    String json = "{}";
    bool dataFromNode = false;
    // временное решение
    unsigned long currentMillis;
    unsigned long prevMillis;
    unsigned long difference;

   public:
    MyLo_raNode(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, F("id"), id);

        jsonRead(parameters, F("orange"), orange);
        jsonRead(parameters, F("red"), red);
        jsonRead(parameters, F("offline"), offline);

        dataFromNode = false;
        SerialPrint("i", "MyLo_ra", "Node initialized");

        jsonRead(parameters, "debug", _debug);
    }

    void setValue(const IoTValue& Value, bool genEvent = true) {
        value = Value;
        regEvent(value.valD, "MyLo_raNode", false, genEvent);
        _minutesPassed = 0;
        prevMillis = millis();
        dataFromNode = true;
        setNewWidgetAttributes();
    }

    void doByInterval() {
        _minutesPassed++;
        setNewWidgetAttributes();
    }

    void loop() {
        currentMillis = millis();
        difference = currentMillis - prevMillis;
        if (difference > 60000) {
            prevMillis = millis();
            this->doByInterval();
        }
    }

    // событие когда пользователь подключается приложением или веб интерфейсом к усройству
    void onMqttWsAppConnectEvent() { setNewWidgetAttributes(); }

    void setNewWidgetAttributes() {
        if (dataFromNode) {
            jsonWriteStr(json, F("info"), prettyMinutsTimeout(_minutesPassed));
            if (orange != 0 && red != 0 && offline != 0) {
                if (_minutesPassed < orange) {
                    jsonWriteStr(json, F("color"), "");
                }
                if (_minutesPassed >= orange && _minutesPassed < red) {
                    jsonWriteStr(json, F("color"), F("orange"));  // сделаем виджет оранжевым
                }
                if (_minutesPassed >= red && _minutesPassed < offline) {
                    jsonWriteStr(json, F("color"), F("red"));  // сделаем виджет красным
                }
                if (_minutesPassed >= offline) {
                    jsonWriteStr(json, F("info"), F("offline"));
                }
            }
        } else {
            jsonWriteStr(json, F("info"), F("awaiting"));
        }
        sendSubWidgetsValues(id, json);
    }

    ~MyLo_raNode(){};
};

void* getAPI_MyLo_raGate(String subtype, String param) {
    if (subtype == F("MyLo_raGate")) {
        return new MyLo_raGate(param);
    } else if (subtype == F("MyLo_raNode")) {
        return new MyLo_raNode(param);
    } else {
        return nullptr;
    }
}