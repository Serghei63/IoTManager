#include "Global.h"
#include "classes/IoTItem.h"
#include <WiFi.h>

// Основные компоненты ESP8266Audio
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioOutputI2S.h"
#include "esp_task_wdt.h"

// Декодеры поддерживаемых кодеков
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioGeneratorFLAC.h"

class Radio1334 : public IoTItem {
private:
    // Новые пины по умолчанию: WSEL -> 18, DIN -> 19, BCLK -> 21
    int _wclk = 18; // WSEL / WS
    int _dout = 19; // DIN / DOUT
    int _bclk = 21; // BCLK / BCK
    
    int _volume = 70; // 0..100%
    String _url = "";
    String _stationName = "Radio"; 
    
    // Компоненты ESP8266Audio
    AudioFileSourceHTTPStream* _file    = nullptr;
    AudioFileSourceBuffer*     _buff    = nullptr;
    AudioGenerator*            _decoder = nullptr;
    AudioOutputI2S*            _out     = nullptr;
    
    TaskHandle_t _radioTaskHandle = nullptr;

public:
    bool _isPlaying = false;
    bool _needsConnect = false;
    bool _needsStop = false;
    bool _firstConnectAttempt = false;
    unsigned long _wifiReadyTime = 0;

    // Фоновый таск FreeRTOS на Core 0
    static void radioWorker(void* pvParameters) {
        Radio1334* instance = (Radio1334*)pvParameters;
        
        esp_task_wdt_delete(NULL); 
        vTaskDelay(pdMS_TO_TICKS(200)); 

        while (true) {
            // Подключение к новому потоку
            if (instance->_needsConnect) {
                instance->_needsConnect = false;
                instance->stopAudioPipeline(); // Чистим старый поток перед запуском

                vTaskDelay(pdMS_TO_TICKS(150)); // Небольшая пауза для дефрагментации Heap

                Serial.printf("[UDA1334 Task] Connecting to: %s\n", instance->_url.c_str());
                
                // Создаем чистый HTTP-поток без перехватчиков метаданных
                instance->_file = new AudioFileSourceHTTPStream(instance->_url.c_str());

                // Буфер 256 KB в RAM/PSRAM для защиты от заиканий
                instance->_buff = new AudioFileSourceBuffer(instance->_file, 262144);
                
                instance->_out = new AudioOutputI2S();
                
                // Порядок вызова SetPinout у библиотеки строго: (BCLK, WCLK, DOUT)
                instance->_out->SetPinout(instance->_bclk, instance->_wclk, instance->_dout);
                
                float gain = (float)instance->_volume / 100.0f;
                instance->_out->SetGain(gain);

                // Выбор кодека на основе URL
                String urlLower = instance->_url;
                urlLower.toLowerCase();

                if (urlLower.indexOf(".aac") != -1 || urlLower.indexOf("/aac") != -1 || urlLower.indexOf("format=aac") != -1) {
                    instance->_decoder = new AudioGeneratorAAC();
                    Serial.println("[UDA1334 Task] Selected codec: AAC");
                } 
                else if (urlLower.indexOf(".flac") != -1) {
                    instance->_decoder = new AudioGeneratorFLAC();
                    Serial.println("[UDA1334 Task] Selected codec: FLAC");
                } 
                else if (urlLower.indexOf(".wav") != -1) {
                    instance->_decoder = new AudioGeneratorWAV();
                    Serial.println("[UDA1334 Task] Selected codec: WAV");
                } 
                else {
                    instance->_decoder = new AudioGeneratorMP3();
                    Serial.println("[UDA1334 Task] Selected codec: MP3");
                }

                if (instance->_decoder->begin(instance->_buff, instance->_out)) {
                    instance->_isPlaying = true;
                    Serial.println("[UDA1334 Task] Playback started!");
                } else {
                    Serial.println("[UDA1334 Task] Decoder start failed!");
                    instance->stopAudioPipeline();
                }
            }

            // Остановка
            if (instance->_needsStop) {
                instance->_needsStop = false;
                instance->stopAudioPipeline();
            }

            // Цикл декодирования фреймов
            if (instance->_isPlaying && instance->_decoder) {
                if (instance->_decoder->isRunning()) {
                    if (!instance->_decoder->loop()) {
                        instance->_decoder->stop();
                        instance->_isPlaying = false;
                    }
                } else {
                    instance->_isPlaying = false;
                }
                vTaskDelay(1);
            } else {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
        }
    }

    void stopAudioPipeline() {
        _isPlaying = false;
        if (_decoder) {
            if (_decoder->isRunning()) _decoder->stop();
            delete _decoder;
            _decoder = nullptr;
        }
        if (_out) {
            _out->stop();
            delete _out;
            _out = nullptr;
        }
        if (_buff) {
            delete _buff;
            _buff = nullptr;
        }
        if (_file) {
            _file->close();
            delete _file;
            _file = nullptr;
        }
    }

    Radio1334(String parameters) : IoTItem(parameters) {
        String pins;
        jsonRead(parameters, "pins", pins);
        
        // Согласно твоему порядку в modinfo: WSEL (0), DIN (1), BCLK (2)
        _wclk = selectFromMarkerToMarker(pins, ",", 0).toInt();
        _dout = selectFromMarkerToMarker(pins, ",", 1).toInt();
        _bclk = selectFromMarkerToMarker(pins, ",", 2).toInt();

        // Устанавливаем новые дефолтные значения, если пины не переписаны в конфиге
        if (_wclk == 0) { _wclk = 18; }
        if (_dout == 0) { _dout = 19; }
        if (_bclk == 0) { _bclk = 21; }

        jsonRead(parameters, "volume", _volume);
        jsonRead(parameters, "url", _url);

        String stationTmp = "";
        if (jsonRead(parameters, "station", stationTmp)) {
            if (stationTmp != "") _stationName = stationTmp;
        } else if (jsonRead(parameters, "name", stationTmp)) {
            _stationName = stationTmp;
        }

        if (_url != "") {
            _firstConnectAttempt = true;
        }

        xTaskCreatePinnedToCore(
            Radio1334::radioWorker,
            "UdaRadioTask",
            8192,
            this,
            12,
            &_radioTaskHandle,
            0
        );
    }

    ~Radio1334() {
        _needsStop = true;
        vTaskDelay(pdMS_TO_TICKS(100));
        if (_radioTaskHandle != nullptr) {
            vTaskDelete(_radioTaskHandle);
            _radioTaskHandle = nullptr;
        }
        stopAudioPipeline();
    }

    void loop() override {
        // Автостарт потока при первом подключении к сети
        if (_firstConnectAttempt && WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0) {
            _firstConnectAttempt = false;
            _wifiReadyTime = millis();
        }

        if (_wifiReadyTime > 0 && (millis() - _wifiReadyTime > 5000)) {
            _wifiReadyTime = 0;
            _needsConnect = true;
            
            // Отправляем название станции и громкость при автостарте
            regEvent(_stationName, "title");
            regEvent(_stationName, "track");
            regEvent(String(_volume), "volume");
        }

        IoTItem::loop();
    }

    void doByInterval() override {}

    // Управление из UI веб-интерфейса
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

    // Управление из скриптов и сценариев
    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "play") {
            if (param.size() > 0) { _url = param[0].valS; }
            if (param.size() > 1) { _stationName = param[1].valS; }

            if (WiFi.status() != WL_CONNECTED) {
                SerialPrint("E", F("Radio1334"), "Connect failed: Wi-Fi not ready!");
                return {};
            }

            if (_url != "") {
                _needsConnect = true;
                // Сразу устанавливаем понятное имя станции во все текстовые виджеты
                regEvent(_stationName, "title");
                regEvent(_stationName, "track");
            }
        }
        else if (command == "stop") {
            _needsStop = true;
            _stationName = "Stopped";
            regEvent(_stationName, "title");
            regEvent("", "track");
        }
        else if (command == "volume") {
            if (param.size() > 0) {
                int newVol = (int)param[0].valD;
                if (newVol > 100) newVol = 100;
                if (newVol < 0) newVol = 0;

                if (_volume != newVol) {
                    _volume = newVol;
                    if (_out) {
                        float gain = (float)_volume / 100.0f;
                        _out->SetGain(gain);
                    }
                    regEvent(String(_volume), "volume");
                }
            }
        }
        return {};
    }
};

// Регистратор для ядра IoTmanager
void* getAPI_Radio1334(String subtype, String param) {
    if (subtype == F("Radio1334")) {
        return new Radio1334(param);
    }
    return nullptr;
}
/*
myradio.play("http://stream.com/rock.mp3", "Рок ФМ") — включит поток и в текстовом виджете сразу загорится «Рок ФМ».

myradio.play("http://stream.com/jazz.mp3", "Радио Джаз") — переключит на джаз с соответствующей надписью.

myradio.stop() — остановит воспроизведение, а на экране появится «Stopped».


*/