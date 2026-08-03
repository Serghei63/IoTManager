#include "Global.h"
#include "classes/IoTItem.h"
#include <WiFi.h>

// Основные компоненты ESP8266Audio
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioOutputI2S.h"
#include "esp_task_wdt.h"

// Декодеры всех поддерживаемых кодеков
#include "AudioGeneratorMP3.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioGeneratorFLAC.h"

// Глобальные переменные для метаданных Icecast
static String RadioTrackTitle = "";
static volatile bool RadioTitleChanged = false;

// Колбэк-функция перехвата названия песни из Icecast/Shoutcast потока
void MDCallback1334(void *cbData, const char *type, bool isUnicode, const char *beacon) {
    (void)cbData;
    (void)isUnicode; // Чтобы компилятор не ругался на неиспользуемую переменную
    
    if (strstr(type, "StreamTitle") != nullptr && beacon != nullptr) {
        String title = String(beacon);
        title.trim();
        
        // Очищаем от кавычек Icecast: 'Artist - Song Title'
        if (title.startsWith("'") && title.endsWith("'")) {
            title = title.substring(1, title.length() - 1);
        }

        if (title.length() > 0 && title != RadioTrackTitle) {
            RadioTrackTitle = title;
            RadioTitleChanged = true;
            Serial.printf("[UDA1334 Track] %s\n", RadioTrackTitle.c_str());
        }
    }
}

class Radio1334 : public IoTItem {
private:
    int _wclk = 25; // WSEL
    int _dout = 22; // DIN
    int _bclk = 26; // BCLK
    
    int _volume = 70; // 0..100%
    String _url = "";
    String _stationName = "Radio"; 
    
    // Компоненты ESP8266Audio
    AudioFileSourceHTTPStream* _file    = nullptr;
    AudioFileSourceBuffer*     _buff    = nullptr;
    AudioGenerator*            _decoder = nullptr; // Универсальный декодер (AAC/MP3/WAV/FLAC)
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

                Serial.printf("[UDA1334 Task] Connecting to: %s\n", instance->_url.c_str());
                
                instance->_file = new AudioFileSourceHTTPStream(instance->_url.c_str());
                
                // Перехват метаданных Icecast
                instance->_file->RegisterMetadataCB(MDCallback1334, NULL);

                // Буфер 20 КБ в RAM для защиты от иканий сети
                instance->_buff = new AudioFileSourceBuffer(instance->_file, 262144); // 256 KB buffer
                
                instance->_out = new AudioOutputI2S();
                // Порядок вызова SetPinout: (BCLK, WCLK, DOUT)
                instance->_out->SetPinout(instance->_bclk, instance->_wclk, instance->_dout);
                
                // Перевод громкости из 0..100% в диапазон 0.0 .. 1.0
                float gain = (float)instance->_volume / 100.0f;
                instance->_out->SetGain(gain);

                // Выбор кодека на основе ссылки URL
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
                    // По умолчанию считаем поток MP3
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
        
        // Порядок согласно шелкографии на плате: WSEL, DIN, BCLK
        _wclk = selectFromMarkerToMarker(pins, ",", 0).toInt();
        _dout = selectFromMarkerToMarker(pins, ",", 1).toInt();
        _bclk = selectFromMarkerToMarker(pins, ",", 2).toInt();

        // Дефолты если пины не переписаны в конфиге
        if (_wclk == 0) { _wclk = 25; }
        if (_dout == 0) { _dout = 22; }
        if (_bclk == 0) { _bclk = 26; }

        jsonRead(parameters, "volume", _volume);
        jsonRead(parameters, "url", _url);

        if (parameters.indexOf("\"name\"") != -1) {
            jsonRead(parameters, "name", _stationName);
        } else {
            _stationName = "I2S Radio";
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
            
            // Первичная инициализация событий
            regEvent(_stationName, "title");
            RadioTrackTitle = "Connecting...";
            regEvent(RadioTrackTitle, "track");
            regEvent(String(_volume), "volume");
        }

        // Событие track отправляется СТРОГО по факту обновления метаданных из потока
        if (RadioTitleChanged) {
            RadioTitleChanged = false;
            regEvent(RadioTrackTitle, "track");
        }

        IoTItem::loop();
    }

    void doByInterval() override {
        // Пусто: защищаем шину событий от лишнего спама
    }

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
                regEvent(_stationName, "title");
                RadioTrackTitle = "Loading...";
                regEvent(RadioTrackTitle, "track");
            }
        }
        else if (command == "stop") {
            _needsStop = true;
            _stationName = "Stopped";
            RadioTrackTitle = "";
            regEvent(_stationName, "title");
            regEvent(RadioTrackTitle, "track");
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