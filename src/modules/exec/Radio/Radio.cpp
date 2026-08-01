/*
#include "Global.h"
#include "classes/IoTItem.h"
#include <SPI.h>
#include "vs1053_ext.h"

// Глобальные переменные для связи
String NewTrackTitle = "";
bool TrackTitleChanged = false;

// Библиотека сама автоматически вызовет функцию с этим именем!
void vs1053_showstreamtitle(const char *info) {
    if (info) {
        Serial.printf("Radio Title: %s\n", info);
        NewTrackTitle = String(info);
        TrackTitleChanged = true;
    }
}
class RadioVs1053 : public IoTItem {
private:
    int _cs, _dcs, _dreq;
    int _mosi, _miso, _sck;
    int _spiNum;
    
    int _volume = 70;
    String _url = "";
    bool _isPlaying = false;
    
    VS1053* player = nullptr;

public:
    RadioVs1053(String parameters) : IoTItem(parameters) {
        String pins;
        jsonRead(parameters, "pins", pins);
        _cs   = selectFromMarkerToMarker(pins, ",", 0).toInt();
        _dcs  = selectFromMarkerToMarker(pins, ",", 1).toInt();
        _dreq = selectFromMarkerToMarker(pins, ",", 2).toInt();

        if (_cs == 0)   { _cs = 2; }
        if (_dcs == 0)  { _dcs = 4; }
        if (_dreq == 0) { _dreq = 36; }
        
        _mosi   = 23; 
        _miso   = 19; 
        _sck    = 18; 
        _spiNum = 3; 

        jsonRead(parameters, "volume", _volume);
        jsonRead(parameters, "url", _url);

        SPI.begin(_sck, _miso, _mosi, -1); 

        player = new VS1053(_cs, _dcs, _dreq, _spiNum, _mosi, _miso, _sck);
        player->begin();
        player->setVolume(_volume);

        // Настраиваем библиотеку на наш колбэк
      //  player->showstreamtitle(vs1053_showstreamtitle);

        if (_url != "") {
            player->connecttohost(_url.c_str());
            _isPlaying = true;
        }
    }

    ~RadioVs1053() {
        if (player) {
            player->stop_mp3client();
            delete player;
        }
    }

    // Основной цикл
    void loop() override {
        if (_isPlaying && player) {
            player->loop();
            
            // Если колбэк поймал новое название трека — кидаем событие в систему
            if (TrackTitleChanged) {
                TrackTitleChanged = false;
                
                // Генерируем событие для топика "title". 
                // В веб-интерфейсе виджет текста (Text) должен быть подписан на ID_title
                regEvent(NewTrackTitle, "title"); 
            }
        }
    }

    void doByInterval() override {
        // Раз в интервал можно дублировать текущие параметры, чтобы UI не «забывал» их
        regEvent(String(_volume), "volume");
        regEvent(NewTrackTitle, "title");
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!player) return {};

        if (command == "play") {
            if (param.size() > 0) {
                _url = param[0].valS;
            }
            if (_url != "") {
                player->connecttohost(_url.c_str());
                _isPlaying = true;
                SerialPrint("I", F("RadioVs1053"), "Playing: " + _url);
            }
        }
        else if (command == "stop") {
            player->stop_mp3client();
            _isPlaying = false;
            NewTrackTitle = "Stopped";
            regEvent(NewTrackTitle, "title"); // Пишем в веб, что радио остановлено
            SerialPrint("I", F("RadioVs1053"), "Stopped");
        }
        else if (command == "volume") {
            if (param.size() > 0) {
                _volume = param[0].valD;
                if (_volume > 100) _volume = 100;
                if (_volume < 0) _volume = 0;
                
                player->setVolume(_volume);
                value.valD = _volume;
                regEvent(String(_volume), "volume");
            }
        }
        return {};
    }
};

// Фабрика
void* getAPI_Radio(String subtype, String param) {
    if (subtype == F("RadioVs1053")) {
        return new RadioVs1053(param);
    }
    return nullptr;
}
*/
#include "Global.h"
#include "classes/IoTItem.h"
#include <SPI.h>
#include "vs1053_ext.h"
#include "esp_task_wdt.h"

// Глобальные переменные метаданных
String RadioTrackTitle = "";
bool RadioTitleChanged = false;

// Функция-колбэк для получения метаданных (stream title) из библиотеки vs1053_ext
void vs1053_showstreamtitle(const char *info) {
    if (info == nullptr || strlen(info) == 0) {
        return;
    }
    
    String streamData = String(info);
    streamData.trim();
    if (streamData.length() == 0) {
        return;
    }

    Serial.printf("[VS1053 Callback] Meta received: %s\n", streamData.c_str());
    RadioTrackTitle = streamData;
    RadioTitleChanged = true;
}

class RadioVs1053 : public IoTItem {
private:
    int _cs, _dcs, _dreq;
    int _mosi, _miso, _sck;
    int _spiNum;
    
    int _volume = 70;
    String _url = "";
    String _stationName = "Radio"; 
    
    VS1053* player = nullptr;
    TaskHandle_t _radioTaskHandle = nullptr; 

public:
    bool _isPlaying = false;
    bool _firstConnectAttempt = false; 
    bool _needsConnect = false;
    unsigned long _wifiReadyTime = 0;

    static void radioWorker(void* pvParameters) {
        RadioVs1053* instance = (RadioVs1053*)pvParameters;
        
        // Отключаем WDT для этого таска, так как мы сами контролируем кванты времени
        esp_task_wdt_delete(NULL); 
        vTaskDelay(pdMS_TO_TICKS(200)); 

        unsigned long lastWdtReset = 0;

        while (true) {
            if (instance->_needsConnect) {
                instance->_needsConnect = false;
                instance->_isPlaying = true;
                Serial.printf("[Radio Task] Connecting on Core 0 to: %s\n", instance->_url.c_str());
                instance->player->connecttohost(instance->_url.c_str());
            }

            if (instance->_isPlaying && instance->player) {
                // Выполняем подкачку данных в декодер VS1053
                instance->player->loop();
                
                // Каждые 100 мс передаем 1 тик системе (1 мс) для планировщика FreeRTOS
                if (millis() - lastWdtReset > 100) {
                    lastWdtReset = millis();
                    vTaskDelay(1); 
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(50)); 
            }
        }
    }

    RadioVs1053(String parameters) : IoTItem(parameters) {
        String pins;
        jsonRead(parameters, "pins", pins);
        _cs   = selectFromMarkerToMarker(pins, ",", 0).toInt();
        _dcs  = selectFromMarkerToMarker(pins, ",", 1).toInt();
        _dreq = selectFromMarkerToMarker(pins, ",", 2).toInt();

        // Значения по умолчанию для HSPI/VS1053 при пустых параметрах
        if (_cs == 0)   { _cs = 2; }
        if (_dcs == 0)  { _dcs = 4; }
        if (_dreq == 0) { _dreq = 36; }
        
        _mosi   = 23; 
        _miso   = 19; 
        _sck    = 18; 
        _spiNum = 3; // VSPI / SPI3

        jsonRead(parameters, "volume", _volume);
        jsonRead(parameters, "url", _url);
        
        if (parameters.indexOf("\"name\"") != -1) {
            jsonRead(parameters, "name", _stationName);
        } else {
            _stationName = "Internet Radio"; 
        }

        Serial.println("[Radio] Constructor started...");

        // Создаем экземпляр декодера VS1053
        player = new VS1053(_cs, _dcs, _dreq, _spiNum, _mosi, _miso, _sck);
        player->begin();
        player->setVolume(_volume);

        if (_url != "") {
            _firstConnectAttempt = true; 
        }

        // Запускаем фоновый таск обработки радио на Ядре 0
        xTaskCreatePinnedToCore(
            RadioVs1053::radioWorker,
            "RadioTask",
            8192,
            this,
            12, 
            &_radioTaskHandle,
            0
        );
        Serial.println("[Radio] Constructor finished successfully!");
    }

    ~RadioVs1053() {
        _isPlaying = false;
        _needsConnect = false;

        if (_radioTaskHandle != nullptr) {
            vTaskDelete(_radioTaskHandle);
            _radioTaskHandle = nullptr;
        }

        if (player) {
            delete player;
            player = nullptr;
        }
    }

    void loop() override {
        // Ожидание стабильного подключения к Wi-Fi перед первым запуском
        if (_firstConnectAttempt && WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0) {
            _firstConnectAttempt = false;
            _wifiReadyTime = millis(); 
            Serial.println("[Radio] Network ready! Waiting 5s for IoTmanager to settle down...");
        }

        if (_wifiReadyTime > 0 && (millis() - _wifiReadyTime > 5000)) {
            _wifiReadyTime = 0; 
            Serial.println("[Radio] Timeout passed. Signaling Task to connect...");
            _needsConnect = true; 
            regEvent(_stationName, "title");
        }

        // Если прилетел мета-заголовок трека из колбэка
        if (RadioTitleChanged) {
            RadioTitleChanged = false;
            regEvent(RadioTrackTitle, "title");
        }

        IoTItem::loop();
    }

    void doByInterval() override {
        regEvent(String(_volume), "volume");
        regEvent(_stationName, "title"); 
    }

    // Обработка кнопок из веб-интерфейса
void onModuleOrder(String &key, String &value) override {
        std::vector<IoTValue> emptyParams;
        
        if (key == "play") {
            execute("play", emptyParams);
        } else if (key == "stop") {
            execute("stop", emptyParams);
        } else if (key == "volume") {
            std::vector<IoTValue> params;
            IoTValue val;
            val.valD = value.toInt();
            params.push_back(val);
            execute("volume", params);
        }
    }
    // Обработка команд из сценариев IoTmanager
    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!player) return {};

        if (command == "play") {
            if (param.size() > 0) { _url = param[0].valS; }
            if (param.size() > 1) { _stationName = param[1].valS; } 
            else { _stationName = "Internet Radio"; }

            if (WiFi.status() != WL_CONNECTED) {
                SerialPrint("E", F("RadioVs1053"), "Connect failed: Wi-Fi not ready!");
                return {};
            }

            if (_url != "") {
                _needsConnect = true; 
                regEvent(_stationName, "title"); 
                SerialPrint("I", F("RadioVs1053"), "Play requested for: " + _stationName);
            }
        }
        else if (command == "stop") {
            _isPlaying = false;
            _needsConnect = false;
            
            // Вместо player->stopSong(); вызываем публичный метод остановки:
            player->stop_mp3client();
            
            _stationName = "Stopped";
            regEvent(_stationName, "title"); 
            SerialPrint("I", F("RadioVs1053"), "Stopped");
        }
        else if (command == "volume") {
            if (param.size() > 0) {
                _volume = (int)param[0].valD; 
                if (_volume > 100) _volume = 100;
                if (_volume < 0) _volume = 0;
                
                player->setVolume(_volume);
                value.valD = _volume; 
                regEvent(String(_volume), "volume");
            }
        }
        return {};
    }
};

// =========================================================================
//              Функция связи модуля с ядром IoTmanager
// =========================================================================
void* getAPI_Radio(String subtype, String param) {
    if (subtype == F("RadioVs1053") || subtype == F("Radio")) {
        return new RadioVs1053(param);
    }
    return nullptr;
}
/*
myradio.play("http://stream.com/rock.mp3", "Рок ФМ") — включит поток и в текстовом виджете сразу загорится «Рок ФМ».

myradio.play("http://stream.com/jazz.mp3", "Радио Джаз") — переключит на джаз с соответствующей надписью.

myradio.stop() — остановит воспроизведение, а на экране появится «Stopped».


*/