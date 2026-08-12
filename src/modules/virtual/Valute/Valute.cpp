/*
#include "Global.h"
#include "classes/IoTItem.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

class Kurs : public IoTItem {
private:
    String _param;
    unsigned long _interval;
    unsigned long _prevMillis = 0;
    String _currentValue = "0.00";

    // Локальная структура для хранения результатов парсинга
    struct Rates {
        String usd = "0.00";
        String eur = "0.00";
        String gbp = "0.00";
        String date = "";
        String time = "";
    } _rates;

public:
    Kurs(String parameters) : IoTItem(parameters) {
        _param = jsonReadStr(parameters, "param");
        
        long intervalHours = 1;
        jsonRead(parameters, F("int"), intervalHours);
        if (intervalHours < 1) intervalHours = 1;
        
        // Переводим интервал проверки в миллисекунды
        _interval = intervalHours * 1000 * 60 * 60; 
        
        // Инициализируем таймер так, чтобы модуль сходил в сеть сразу при старте
        _prevMillis = millis() - _interval;
    }

    void getKurs() {
        if (WiFi.status() != WL_CONNECTED) {
            SerialPrint("E", F("Kurs"), "Wi-Fi not connected");
            return;
        }

        WiFiClient client;
        HTTPClient https;

        Serial.println("[Kurs] Requesting data from MOEX...");
        https.begin(client, "http://iss.moex.com/iss/statistics/engines/futures/markets/indicativerates/securities.json");
        
        int httpResponseCode = https.GET();
        
        if (httpResponseCode == HTTP_CODE_OK) {
            String payload = https.getString();
            
            // Динамический буфер для входящего JSON (выделяется в Heap и сам очищается)
            DynamicJsonDocument doc(16384); 
            
            // Настраиваем гибкий фильтр: забираем весь массив data, но не ограничиваем его размер
            DynamicJsonDocument filter(256);
            filter["securities"]["data"] = true;

            DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

            if (error) {
                Serial.printf("[Kurs] deserializeJson() failed: %s\n", error.c_str());
            } else { 
                JsonArray dataArray = doc["securities"]["data"];
                
                // Проходим по всему массиву и ищем валюты по их текстовому имени, а не по индексам
                for (JsonArray row : dataArray) {
                    String pairName = row[2].as<String>(); // Например, "USD/RUB"
                    String price = row[3].as<String>();
                    
                    if (pairName == "USD/RUB") {
                        _rates.usd = price;
                        _rates.date = row[0].as<String>();
                        _rates.time = row[1].as<String>();
                    } else if (pairName == "EUR/RUB") {
                        _rates.eur = price;
                    } else if (pairName == "GBP/RUB") {
                        _rates.gbp = price;
                    }
                }
                Serial.printf("[Kurs] Parsed successfully. USD: %s, EUR: %s\n", _rates.usd.c_str(), _rates.eur.c_str());
            }
        } else {
            Serial.printf("[Kurs] HTTP Error code: %d\n", httpResponseCode);
        }
        
        https.end();
    }

    void doByInterval() override {
        // Запрашиваем данные с биржи
        getKurs();

        // Распределяем данные в зависимости от того, какой топик/параметр привязан к этому экземпляру класса
        if (_param == "USD")  _currentValue = _rates.usd;
        else if (_param == "EUR")  _currentValue = _rates.eur;
        else if (_param == "GBP")  _currentValue = _rates.gbp;
        else if (_param == "Data") _currentValue = _rates.date;
        else if (_param == "Time") _currentValue = _rates.time;

        // Отправляем в ядро IoTmanager
        value.valS = _currentValue;
        regEvent(value.valS, "Kurs");
    }
    
    void loop() override {
        // Правильная неблокирующая проверка интервала
        if (millis() - _prevMillis >= _interval) {
            _prevMillis = millis();
            this->doByInterval();
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "get") {
            this->doByInterval();
        }
        return {};
    }

    ~Kurs() {}
};

void *getAPI_Valute(String subtype, String param) {
    if (subtype == F("Kurs")) {
        return new Kurs(param);
    }
    return nullptr;
}
*/
#include "Global.h"
#include "classes/IoTItem.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <list> // Подключаем std::list

// Глобальный документ для хранения ответа
DynamicJsonDocument KursDoc(16384); 
bool isKursLoading = false; 

// ИСПРАВЛЕНО: Объявляем правильный тип внешнего списка (std::list)
extern std::list<IoTItem*> IoTItems;

class Kurs : public IoTItem {
private:
    String _param;
    bool _debug = true;
    bool _isFirstRun = true;

static void tGetKurs(void *pvParameters) {
        Kurs *instance = (Kurs *)pvParameters;
        
        if (WiFi.status() == WL_CONNECTED) {
            WiFiClient client;
            HTTPClient http;
            String payload;
            String ret;

            if (instance->_debug) Serial.println(F("[Kurs Task] --- Sending GET to MOEX ---"));
            
            http.setTimeout(4000); 
            http.begin(client, "http://iss.moex.com/iss/statistics/engines/futures/markets/indicativerates/securities.json");
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            
            int httpCode = http.GET();

            if (httpCode > 0) {
                ret = String(httpCode);
                if (httpCode == HTTP_CODE_OK) {
                    payload = http.getString();
                    
                    KursDoc.clear(); 
                    DeserializationError error = deserializeJson(KursDoc, payload);
                    if (error && instance->_debug) {
                        Serial.printf("[Kurs Error] deserializeJson failed: %s\n", error.c_str());
                    }
                }
            } else {
                ret = http.errorToString(httpCode).c_str();
            }
            http.end();
        }

        // Данные в KursDoc обновились. Мгновенно обновляем все объекты в системе
        for (auto item : IoTItems) {
            if (item && item->getSubtype() == "Kurs") {
                // Каждый объект сам вытащит свой параметр (USD, EUR, Data, Time)
                ((Kurs*)item)->parseJsonData();
            }
        }

        isKursLoading = false; 
        vTaskDelete(NULL);    
    }

public:
    Kurs(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "param", _param);
        
        long interval;
        jsonRead(parameters, F("int"), interval); 
        
        if (interval < 1) interval = 1;
        setInterval(interval * 60 * 60); 
    }

    void startBackgroundRequest() {
        if (isKursLoading) return; 
        isKursLoading = true;

        xTaskCreatePinnedToCore(
            tGetKurs,             
            "tGetKurs",           
            8192,                 
            this,                 
            1,                    
            NULL,                 
            0                     
        );
    }

void doByInterval() override {
        if (_debug) Serial.printf("[Kurs] doByInterval() triggered for: %s\n", _param.c_str());
        
        // Любой объект (USD, EUR, Data), у которого подошел час, 
        // инициирует фоновый запрос к Мосбирже, если он еще не качается
        startBackgroundRequest();
    }

    void parseJsonData() {

        if (KursDoc["securities"]["data"].is<JsonArray>()) {
             JsonArray securities_data = KursDoc["securities"]["data"];
             String foundValue = "";

             for (JsonArray row : securities_data) {
                  String pairName = row[2].as<String>(); 
                      pairName.toUpperCase();
        
                 // Получаем тип клиринга: "tc" или "mc"
             String clearing = row[4].as<String>(); 

        if (_param == "USD" && pairName.indexOf("USD") != -1) {
            foundValue = row[3].as<String>();
            if (clearing == "mc") break; // Выходим только если нашли вечерний клиринг
        }
        else if (_param == "EUR" && pairName.indexOf("EUR") != -1) {
            foundValue = row[3].as<String>();
            if (clearing == "mc") break;
        }
        else if (_param == "GBP" && pairName.indexOf("GBP") != -1) {
            foundValue = row[3].as<String>();
            if (clearing == "mc") break;
        }
        // Для даты и времени привязываемся к USD. 
        // Убираем break, чтобы цикл дошел до конца и сохранил последнюю (вечернюю) метку
        else if (_param == "Data" && pairName.indexOf("USD") != -1) {
            foundValue = row[0].as<String>();
        }
        else if (_param == "Time" && pairName.indexOf("USD") != -1) {
            foundValue = row[1].as<String>();
        }
    }

            if (foundValue.length() > 0) {
                value.valS = foundValue;
                if (_debug) {
                    Serial.printf("[Kurs Debug] Parameter '%s' successfully set to: %s\n", _param.c_str(), value.valS.c_str());
                }
                regEvent(value.valS, "Kurs");
            }
        }
    }

    void loop() override {
        if (_isFirstRun) {
            _isFirstRun = false;
            if (_param == "USD") {
                if (_debug) Serial.println(F("[Kurs] Safe async first run from loop()..."));
                startBackgroundRequest();
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "get") {
            if (_param == "USD") startBackgroundRequest();
        }
        return {};
    }

    ~Kurs() {}
};

void *getAPI_Valute(String subtype, String param) {
    if (subtype == F("Kurs")) {
        return new Kurs(param);
    }
    return nullptr;
}