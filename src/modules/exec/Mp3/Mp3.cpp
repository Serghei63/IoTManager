#include "Global.h"
#include "classes/IoTItem.h"

#include "SoftwareSerial.h"               // Подключаем библиотеку SoftwareSerial
#include "DFRobotDFPlayerMini.h"           // Подключаем библиотеку DFPlayerMini

class Mp3 : public IoTItem {
   private:
    SoftwareSerial* mySerial;
    DFRobotDFPlayerMini* myMP3;
    
    int _currentTrack;
    int _lastState;

   public:
    Mp3(String parameters): IoTItem(parameters) {
        String tmpstr;
        int volumetmp;
        jsonRead(parameters, "pins", tmpstr);
        int pinRx = selectFromMarkerToMarker(tmpstr, ",", 0).toInt();  
        int pinTx = selectFromMarkerToMarker(tmpstr, ",", 1).toInt();  
        mySerial = new SoftwareSerial(pinRx, pinTx);
        pinMode(pinRx, INPUT);
        pinMode(pinTx, OUTPUT);

        jsonRead(parameters, "volume", volumetmp);

        _currentTrack = -1;
        _lastState = -1;

        if (mySerial) {
            mySerial->begin(9600);
            myMP3 = new DFRobotDFPlayerMini();
        }
        if (myMP3) {
            myMP3->begin(*mySerial);
            myMP3->volume(volumetmp);
            SerialPrint("I", "MP3", "MP3 module initialized, RX: " + String(pinRx) + ", TX: " + String(pinTx));
        } 

        value.isDecimal = false;    // значение объекта всегда будет строка
    }

    void doByInterval() override {

if (!myMP3) return;

    // 1. Асинхронные события от плеера (когда чип сам шлет информацию)
    if (myMP3->available()) {
        switch (myMP3->readType()) {
            case DFPlayerPlayFinished:
                value.valS = F("Play Finished!");
                regEvent(value.valS, F("Mp3"));
                _lastState = -1; 
                return; // Сразу выходим, давая веб-интерфейсу отобразить окончание
            case DFPlayerError:
                value.valS = F("Error!");
                regEvent(value.valS, F("Mp3"));
                return;
            default:
                break;
        }
    }

    // Переменные для поочередного опроса
    static byte queryStep = 0;
    static int tempState = -1;
    static int tempTrack = -1;

    queryStep++;

    // Шаг 1: Опрашиваем ТОЛЬКО статус
    if (queryStep == 2 || _lastState == -1) {
        int stateResult = myMP3->readState();
        if (stateResult != -1) { // Игнорируем ошибки таймаута UART
            tempState = stateResult;
        }
    }
    // Шаг 2: В следующий интервал опрашиваем ТОЛЬКО номер трека
    else if (queryStep >= 4) {
        queryStep = 0; // Сброс счетчика шагов
        int trackResult = myMP3->readCurrentFileNumber();
        if (trackResult != -1) { // Игнорируем ошибки таймаута UART
            tempTrack = trackResult;
        }
    }

    // Обработка результатов, если они валидны и изменились
    if ((tempState != _lastState || tempTrack != _currentTrack) && tempState != -1) {
        _lastState = tempState;
        _currentTrack = tempTrack;

        String statusStr = "";

        if (_lastState == 1 || _lastState == 513) {
            statusStr = "Playing #" + String(_currentTrack > 0 ? _currentTrack : 1);
        } 
        else if (_lastState == 2 || _lastState == 514) {
            statusStr = "Paused #" + String(_currentTrack > 0 ? _currentTrack : 1);
        }
        else if (_lastState == 0 || _lastState == 512) {
            statusStr = F("Stopped");
        }
        else {
            // Если статус неизвестен, но это не -1, и у нас висит финиш — не трогаем
            if (value.valS == F("Play Finished!")) return;
            statusStr = "Status: " + String(_lastState);
        }

        value.valS = statusStr;
        regEvent(value.valS, F("Mp3"));
    }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!myMP3) {
            SerialPrint("E", "MP3", "Error: myMP3 is null!");
            return {};
        }

        // Заставим doByInterval на следующем цикле принудительно обновить статус в веб
        _lastState = -1; 

        // 1. ИГРАТЬ ВСЁ С НАЧАЛА ПО ОЧЕРЕДИ
        if (command == F("playAll")) {
            myMP3->play(1);
            myMP3->enableLoopAll();
            SerialPrint("I", "MP3", "playAll() executed");
        } 
        // 2. ПОЛНЫЙ СТОП
        else if (command == F("stop")) { 
            myMP3->stop();
            SerialPrint("I", "MP3", "stop() executed");
        } 
        // 3. ПАУЗА
        else if (command == F("pause")) { 
            myMP3->pause();
            SerialPrint("I", "MP3", "pause() executed");
        } 
        // 4. ПРОДОЛЖЕНИЕ ИГРЫ
        else if (command == F("continue")) { 
            myMP3->start();
            SerialPrint("I", "MP3", "continue() executed");
        } 
        // 5. СЛУЧАЙНЫЙ ВЫБОР ВСЕЙ КАРТЫ
        else if (command == F("randomAll")) { 
            myMP3->randomAll();
            SerialPrint("I", "MP3", "randomAll() executed");
        } 
        // 6. ИГРАТЬ КОНКРЕТНУЮ ПАПКУ И ФАЙЛ
        else if (command == F("playFolder")) { 
            if (param.size() >= 2) {
                int folder = param[0].valD;
                int file = param[1].valD;
                myMP3->playFolder(folder, file);
                SerialPrint("I", "MP3", "playFolder() Folder: " + String(folder) + ", File: " + String(file));
            }
        } 
        // 7. ИЗМЕНЕНИЕ ГРОМКОСТИ
        else if (command == F("volume")) { 
            if (param.size() >= 1) {
                int vol = param[0].valD;
                myMP3->volume(vol);
                SerialPrint("I", "MP3", "volume() Set to: " + String(vol));
            }
        } 
        // 8. СЛЕДУЮЩИЙ И ПРЕДЫДУЩИЙ
        else if (command == F("next")) { 
            myMP3->next();  
        } 
        else if (command == F("previous")) { 
            myMP3->previous();  
        }

        return {};  
    }

    ~Mp3() {};
};

void* getAPI_Mp3(String subtype, String param) {
    if (subtype == F("Mp3")) {
        return new Mp3(param);
    } else {
        return nullptr;
    }
}