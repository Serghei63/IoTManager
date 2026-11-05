#include "MqttClient.h"
#include "classes/IoTDiscovery.h"

#ifdef SD_ENABLE
#include <SdFat.h>
extern SdFat sd;
#endif

void mqttInit() {
    mqtt.setCallback(mqttCallback);
    ts.add(
        WIFI_MQTT_CONNECTION_CHECK, MQTT_RECONNECT_INTERVAL,
        [&](void*) {
            if (isNetworkActive()) {
                SerialPrint("i", F("WIFI"), "http://" + jsonReadStr(settingsFlashJson, F("ip")));
                wifiUptimeCalc();
                if (mqtt.connected()) {
                    SerialPrint("i", F("MQTT"), "OK");
                    mqttUptimeCalc();
                    handleMqttStatus(false);
                } else {
                    SerialPrint("E", F("MQTT"), F("✖ Connection lost"));
                    handleMqttStatus(false);
                    mqttUptime = 0;
                    mqttConnect();
                }
            } else {
                SerialPrint("E", F("WIFI"), F("✖ Lost WiFi connection"));
                ts.remove(WIFI_MQTT_CONNECTION_CHECK);
                wifiUptime = 0;
                startAPMode();
            }
        },
        nullptr, true);
}

void mqttLoop() {
    if (!isNetworkActive() || !mqtt.connected()) {
        return;
    }
    mqtt.loop();
}

boolean mqttConnect() {
    getMqttData();
    bool res = false;
    if (mqttServer == "") {
        SerialPrint("E", "MQTT", F("mqttServer empty"));
        handleMqttStatus(true, 6);
        return res;
    }
    SerialPrint("i", "MQTT", "connection started");
    SerialPrint("i", "MQTT", "broker " + mqttServer + ":" + String(mqttPort, DEC));
    SerialPrint("i", "MQTT", "topic " + mqttRootDevice);

    mqtt.setServer(mqttServer.c_str(), mqttPort);

    if (!mqtt.connected()) {
        bool connected = false;
        if (mqttUser != "" && mqttPass != "") {
            if (HOMEdDiscovery) {
                connected = mqtt.connect(chipId.c_str(), mqttUser.c_str(), mqttPass.c_str(), (HOMEdDiscovery->HOMEdTopic + "/device/custom/" + nameId).c_str(), 1, true, "{\"status\":\"offline\"}");
            } else {
                connected = mqtt.connect(chipId.c_str(), mqttUser.c_str(), mqttPass.c_str(), (mqttRootDevice + "/state").c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            SerialPrint("i", F("MQTT"), F("Go to connection with login and password"));
        } else if (mqttUser == "" && mqttPass == "") {
            if (HOMEdDiscovery) {
                connected = mqtt.connect(chipId.c_str(), (HOMEdDiscovery->HOMEdTopic + "/device/custom/" + nameId).c_str(), 1, true, "{\"status\":\"offline\"}");
            } else {
                connected = mqtt.connect(chipId.c_str(), (mqttRootDevice + "/state").c_str(), 1, true, "{\"status\":\"offline\"}");
            }
            SerialPrint("i", F("MQTT"), F("Go to connection without login and password"));
        } else {
            SerialPrint("E", F("MQTT"), F("✖ Login or password missed"));
            handleMqttStatus(true, 7);
            return res;
        }

        if (mqtt.state() == 0) {
            SerialPrint("i", F("MQTT"), F("✔ connected"));
            handleMqttStatus(true);
            mqttSubscribe();
            res = true;
        } else {
            SerialPrint("E", F("MQTT"), "🡆 Could't connect, retry in " + String(MQTT_RECONNECT_INTERVAL / 1000) + "s");
            handleMqttStatus(true);
        }
    }
    return res;
}

void mqttDisconnect() {
    SerialPrint("i", F("MQTT"), F("disconnected"));
    mqtt.disconnect();
}

void mqttReconnect() {
    mqttDisconnect();
    mqttConnect();
}

bool mqttIsConnect(){
    return mqtt.connected();
}

void getMqttData() {
    mqttServer = jsonReadStr(settingsFlashJson, F("mqttServer"));
    mqttPort = jsonReadInt(settingsFlashJson, F("mqttPort"));
    mqttUser = jsonReadStr(settingsFlashJson, F("mqttUser"));
    mqttPass = jsonReadStr(settingsFlashJson, F("mqttPass"));
    mqttPrefix = jsonReadStr(settingsFlashJson, F("mqttPrefix"));
    if (jsonReadInt(settingsFlashJson, F("HOMEd_names"))){
        nameId = jsonReadStr(settingsFlashJson, F("name"));
    } else {
        nameId = getChipId();
    }
    mqttRootDevice = mqttPrefix + "/" + chipId;
}

void mqttSubscribe() {
    SerialPrint("i", F("MQTT"), F("subscribed"));
    SerialPrint("i", F("MQTT"), mqttRootDevice);
    mqtt.subscribe(mqttPrefix.c_str());
    mqtt.subscribe((mqttRootDevice + "/+/control").c_str());
    mqtt.subscribe((mqttRootDevice + "/update").c_str());

    if (jsonReadBool(settingsFlashJson, "mqttin")) {
        mqtt.subscribe((mqttPrefix + "/+/+/event/#").c_str());
        mqtt.subscribe((mqttPrefix + "/+/+/order/#").c_str());
        mqtt.subscribe((mqttPrefix + "/+/+/info").c_str());
    }
    for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        if ((*it)->getSubtype() == "ExternalMQTT") {
            String tmps = (*it)->getMqttExterSub();
            if (tmps != ""){
                mqtt.subscribe(tmps.c_str());
                SerialPrint("i", F("MQTT"), ("subscribed external " + tmps).c_str());
            }
        }
    }
    if(HOMEdDiscovery)
        HOMEdDiscovery->mqttSubscribeDiscovery();
    if(HADiscovery)
        HADiscovery->mqttSubscribeDiscovery();

    if(HOMEdDiscovery || HADiscovery) {
        for (std::list<IoTItem *>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            if ((*it)->iAmLocal) {
                publishStatusMqtt((*it)->getID(), (*it)->getValue());
                (*it)->onMqttWsAppConnectEvent();
            }
        }
    }
}

void mqttSubscribeExternal(String topic, bool usePrefix) {
    String _sb_topic = topic;
    if (usePrefix) {
        _sb_topic = mqttPrefix + "/" + topic;
    }
    mqtt.subscribe(_sb_topic.c_str());
    SerialPrint("i", F("MQTT"), ("subscribed external " + _sb_topic).c_str());
}

void mqttCallback(char* topic, uint8_t* payload, size_t length) {
    String topicStr = String(topic);
    String payloadStr;
    payloadStr.reserve(length + 1);
    for (size_t i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
        (*it)->onMqttRecive(topicStr, payloadStr);
    }

    if (payloadStr.startsWith("HELLO")) {
        SerialPrint("i", F("MQTT"), F("Full update"));
        publishWidgets();

        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            if ((*it)->iAmLocal) {
                publishStatusMqtt((*it)->getID(), (*it)->getValue());
                (*it)->onMqttWsAppConnectEvent();
            }
        }

        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            String subtype = (*it)->getSubtype();
            if (subtype == "Loging" || subtype == "LogingDaily" || subtype == "LogingHourly" || subtype == "SDLoging" || subtype == "Lognew2") {
                (*it)->setPublishDestination(TO_MQTT);
                (*it)->publishValue();
            }
        }

#ifdef GATE_MODE
        publishTimes();
#endif
#ifdef EnableLogging
        choose_log_date_and_send();
#endif
    } else if (topicStr.indexOf(F("control")) != -1) {
        String key = selectFromMarkerToMarker(topicStr, "/", 3);
        String valueIfJson = "";
        if (!jsonRead(payloadStr, F("status"), valueIfJson, false))
            generateOrder(key, payloadStr);
        else
            generateOrder(key, valueIfJson);

        SerialPrint("i", F("=>MQTT"), "Msg from iotmanager app: " + key + " " + payloadStr);
    } else if (topicStr.indexOf("event") != -1) {
        if (!jsonReadBool(settingsFlashJson, "mqttin")) return;
        if (topicStr.indexOf(chipId) == -1) {
            String devId = selectFromMarkerToMarker(topicStr, "/", 2);
            String id = selectFromMarkerToMarker(topicStr, "/", 3);
            analyzeMsgFromNet(payloadStr, id);
        }
    } else if (topicStr.indexOf("order") != -1) {
        if (!jsonReadBool(settingsFlashJson, "mqttin")) return;
        String devId = selectFromMarkerToMarker(topicStr, "/", 2);
        String id = selectFromMarkerToMarker(topicStr, "/", 3);
        generateOrder(id, payloadStr);
        SerialPrint("i", F("=>MQTT"), "Received direct order " + id + " " + payloadStr);
    }
}

boolean publish(const String& topic, const String& data) {
    if (mqtt.beginPublish(topic.c_str(), data.length(), false)) {
        mqtt.print(data);
        return mqtt.endPublish();
    }
    return false;
}

boolean publishData(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic;
    if (!publish(path, data)) {
        SerialPrint("E", F("MQTT"), F("on publish data"));
        return false;
    }
    return true;
}

boolean publishChartMqtt(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic + "/status";
    if (!publish(path, data)) {
        SerialPrint("E", F("MQTT"), F("on publish chart"));
        return false;
    }
    return true;
}

boolean publishStatusMqtt(const String& topic, const String& data) {
    if (HOMEdDiscovery) {
        HOMEdDiscovery->publishStatusHOMEd(topic, data);
    }
    String path = mqttRootDevice + "/" + topic + "/status";
    String json = "{}";
    jsonWriteStr(json, "status", data);
    return mqtt.publish(path.c_str(), json.c_str(), false);
}

boolean publishJsonMqtt(const String& topic, const String& json) {
    String path = mqttRootDevice + "/" + topic + "/status";
    return mqtt.publish(path.c_str(), json.c_str(), false);
}

boolean publishEvent(const String& topic, const String& data) {
    String path = mqttRootDevice + "/" + topic + "/event";
    return mqtt.publish(path.c_str(), data.c_str(), false);
}

void publishWidgets() {
    auto file = seekFile("layout.json");
    if (!file) {
        SerialPrint("E", F("MQTT"), F("no file layout.json"));
        return;
    }
    size_t size = file.size();
    DynamicJsonDocument doc(size * 2);
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        SerialPrint("E", F("MQTT"), error.f_str());
        jsonWriteInt(errorsHeapJson, F("jse3"), 1);
    }
    JsonArray arr = doc.as<JsonArray>();
    for (JsonVariant value : arr) {
        publishData("config", value.as<String>());
    }
    file.close();
}

// ------------------ MQTT & Chart Functions ------------------

bool publishChartFileToMqtt(String path, String id, int maxCount, String seriesArray, String typeChart) {
    String oneSingleJson = "";
    bool fileReadSuccess = false;

#ifdef SD_ENABLE
    if (path.startsWith("/lgsd/")) {
        FsFile configFile = sd.open(path.c_str(), O_RDONLY);
        if (configFile) {
            oneSingleJson.reserve(configFile.fileSize());
            while (configFile.available()) {
                oneSingleJson += (char)configFile.read();
            }
            configFile.close();
            fileReadSuccess = true;
        } else {
            SerialPrint("E", F("SDLoging"), path + " file reading error (SD), file not found");
            return false;
        }
    }
#endif

    if (!fileReadSuccess) {
        File configFile = FileFS.open(path, "r");
        if (!configFile) {
            SerialPrint("E", F("Loging"), path + " file reading error (FS), json not created, return");
            return false;
        }
        oneSingleJson = configFile.readString();
        configFile.close();
    }

    if (oneSingleJson.length() == 0) {
        SerialPrint("E", F("Loging"), path + " file is empty");
        return false;
    }

    if (oneSingleJson.endsWith(",")) {
        oneSingleJson.remove(oneSingleJson.length() - 1);
    }

    String topic = mqttRootDevice + "/" + id + "/status";
    String chartTypeVal = (typeChart.length() > 0) ? typeChart : "line";

    String json = "{";
    json += "\"maxCount\":" + String(maxCount) + ",";
    json += "\"topic\":\"" + topic + "\",";
    json += "\"typeChart\":\"" + chartTypeVal + "\",";
    if (seriesArray.length() > 0) {
        json += "\"series\":" + seriesArray + ",";
    }
    json += "\"status\":[" + oneSingleJson + "]";
    json += "}";

    json.replace("},]}", "}]}");

    publishChartMqtt(id, json);
    return true;
}

void handleMqttStatus(bool send, int state) {
    if (state == -1) {
        state = mqtt.state();
    }
    String stateStr = getStateStr(state);
    jsonWriteStr_(errorsHeapJson, F("mqtt"), stateStr);
    if (!send) sendStringToWs("errors", errorsHeapJson, -1);
}

const String getStateStr(int e) {
    switch (e) {
        case -4: return F("e1");
        case -3: return F("e2");
        case -2: return F("e3");
        case -1: return F("e4");
        case 0:  return F("e5");
        case 1:  return F("e6");
        case 2:  return F("e7");
        case 3:  return F("e8");
        case 4:  return F("e9");
        case 5:  return F("e10");
        case 6:  return F("e11");
        case 7:  return F("e12");
        case 8:  return F("e13");
        default: return F("unk");
    }
}

void mqttUptimeCalc() {
    static unsigned int prevMillis;
    mqttUptime = mqttUptime + (millis() - prevMillis);
    prevMillis = millis();
}

void wifiUptimeCalc() {
    static unsigned int prevMillis;
    wifiUptime = wifiUptime + (millis() - prevMillis);
    prevMillis = millis();
}