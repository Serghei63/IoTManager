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
            statusStr = "Playing N" + String(_currentTrack > 0 ? _currentTrack : 1);
        } 
        else if (_lastState == 2 || _lastState == 514) {
            statusStr = "Paused N" + String(_currentTrack > 0 ? _currentTrack : 1);
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

        // Принудительно обновим статус в веб-интерфейсе на следующем цикле
        _lastState = -1; 

        // === ГРУППА 1: УПРАВЛЕНИЕ ВОСПРОИЗВЕДЕНИЕМ ===
        if (command == F("playAll")) {
            myMP3->play(1);
            delay(40);
            myMP3->enableLoopAll();
            SerialPrint("I", "MP3", "playAll() executed");
        } 
        else if (command == F("stop")) { 
            myMP3->stop();
            SerialPrint("I", "MP3", "stop() executed");
        } 
        else if (command == F("pause")) { 
            myMP3->pause();
            SerialPrint("I", "MP3", "pause() executed");
        } 
        else if (command == F("continue") || command == F("start")) { 
            myMP3->start();
            SerialPrint("I", "MP3", "start() executed");
        } 
        else if (command == F("next")) { 
            myMP3->next();  
            SerialPrint("I", "MP3", "next() executed");
        } 
        else if (command == F("previous")) { 
            myMP3->previous();  
            SerialPrint("I", "MP3", "previous() executed");
        }
        else if (command == F("randomAll")) { 
            myMP3->randomAll();
            SerialPrint("I", "MP3", "randomAll() executed");
        } 
        else if (command == F("reset")) {
            myMP3->reset();
            SerialPrint("I", "MP3", "reset() triggered");
        }
        else if (command == F("sleep")) {
            myMP3->sleep();
            SerialPrint("I", "MP3", "sleep() triggered");
        }

        // === ГРУППА 2: НАСТРОЙКА ЗВУКА И ЖЕЛЕЗА ===
        else if (command == F("volume")) { 
            if (param.size() >= 1) {
                int vol = param[0].valD;
                if (vol < 0) vol = 0;
                if (vol > 30) vol = 30; // Аппаратный максимум DFPlayer
                delay(10);
                myMP3->volume(vol);
                SerialPrint("I", "MP3", "volume() Set to: " + String(vol));
            }
        } 
        else if (command == F("volumeUp")) {
            myMP3->volumeUp();
            SerialPrint("I", "MP3", "volumeUp() executed");
        }
        else if (command == F("volumeDown")) {
            myMP3->volumeDown();
            SerialPrint("I", "MP3", "volumeDown() executed");
        }
        else if (command == F("EQ")) {
            if (param.size() >= 1) {
                int eq = param[0].valD; // 0 - Normal, 1 - Pop, 2 - Rock, 3 - Jazz, 4 - Classic, 5 - Bass
                myMP3->EQ(eq);
                SerialPrint("I", "MP3", "EQ set to: " + String(eq));
            }
        }

        // === ГРУППА 3: САМАЯ ГЛАВНАЯ (РАБОТА С ПАПКАМИ И ИНТЕРВАЛАМИ) ===
        // Выбор конкретной папки и файла (Папки должны называться "01", "02" и т.д.)
        else if (command == F("playFolder")) { 
            if (param.size() >= 2) {
                int folder = param[0].valD;
                int file = param[1].valD;
                myMP3->playFolder(folder, file);
                SerialPrint("I", "MP3", "playFolder() Folder: " + String(folder) + ", File: " + String(file));
            }
        } 
        // Запуск папки по кругу
        else if (command == F("loopFolder")) {
            if (param.size() >= 1) {
                int folder = param[0].valD;
                myMP3->loopFolder(folder);
                SerialPrint("I", "MP3", "loopFolder() Folder: " + String(folder));
            }
        }
        // Воспроизведение файла из специальной папки "MP3" в корне флешки
        else if (command == F("playMp3Folder")) {
            if (param.size() >= 1) {
                int file = param[0].valD;
                myMP3->playMp3Folder(file);
                SerialPrint("I", "MP3", "playMp3Folder() File: " + String(file));
            }
        }
        // Идеально для уведомлений! Прерывает текущую песню, играет файл из папки "ADVERT", а потом продолжает песню с того же места!
        else if (command == F("advertise")) {
            if (param.size() >= 1) {
                int file = param[0].valD;
                myMP3->advertise(file);
                SerialPrint("I", "MP3", "advertise() Notification file: " + String(file));
            }
        }
        else if (command == F("stopAdvertise")) {
            myMP3->stopAdvertise();
            SerialPrint("I", "MP3", "stopAdvertise() executed");
        }
        // Режим зацикливания одного конкретного файла
        else if (command == F("loop")) {
            if (param.size() >= 1) {
                int file = param[0].valD;
                myMP3->loop(file);
                SerialPrint("I", "MP3", "loop() single file: " + String(file));
            }
        }

        // === ГРУППА 4: ИНФОРМАЦИОННЫЕ КОМАНДЫ (ЧТЕНИЕ ИЗ ПЛЕЕРА) ===
        // Чтение параметров возвращает значение через IoTValue
        else if (command == F("readVolume")) {
            int currentVol = myMP3->readVolume();
            SerialPrint("I", "MP3", "Read volume: " + String(currentVol));
            return {};
        }
        else if (command == F("readState")) {
            int state = myMP3->readState(); // 512 - стоп, 513 - играет, 514 - пауза
            return {};
        }
        else if (command == F("readFileCountsInFolder")) {
            if (param.size() >= 1) {
                int folder = param[0].valD;
                int counts = myMP3->readFileCountsInFolder(folder);
                return {};
            }
        }
        else if (command == F("readFileCounts")) {
            int counts = myMP3->readFileCounts();
            return {};
        }
        else if (command == F("readCurrentFileNumber")) {
            int num = myMP3->readCurrentFileNumber();
            return {};
        }

        return {};  
    }
/*
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
*/
    ~Mp3() {};
};

void* getAPI_Mp3(String subtype, String param) {
    if (subtype == F("Mp3")) {
        return new Mp3(param);
    } else {
        return nullptr;
    }
}