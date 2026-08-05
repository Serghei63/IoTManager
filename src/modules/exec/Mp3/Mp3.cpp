/*
#include "Global.h"
#include "classes/IoTItem.h"

#include "SoftwareSerial.h"               // Подключаем библиотеку SoftwareSerial
#include "DFRobotDFPlayerMini.h"            // Подключаем библиотеку DFPlayerMini_Fast


class Mp3 : public IoTItem {
   private:
    SoftwareSerial* mySerial;
    DFRobotDFPlayerMini* myMP3;

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

        if (mySerial) {
            mySerial->begin(9600);
            myMP3 = new DFRobotDFPlayerMini();
        }
        if (myMP3) {
            myMP3->begin(*mySerial);
            myMP3->volume(volumetmp);
        } 

        value.isDecimal = false;    // значение объекта всегда будет строка
    }

    void doByInterval() {
        if (myMP3 && myMP3->available()) {
            switch (myMP3->readType()) {
                case TimeOut:
                value.valS = F("Time Out!");
                break;
                case WrongStack:
                value.valS = F("Stack Wrong!");
                break;
                case DFPlayerCardInserted:
                value.valS = F("Card Inserted!");
                break;
                case DFPlayerCardRemoved:
                value.valS = F("Card Removed!");
                break;
                case DFPlayerCardOnline:
                value.valS = F("Card Online!");
                break;
                case DFPlayerPlayFinished:
                value.valS = F("Play Finished!");
                break;
                case DFPlayerError:
                    switch (myMP3->read()) {
                        case Busy:
                        value.valS = F("Card not found");
                        break;
                        case Sleeping:
                        value.valS = F("Sleeping");
                        break;
                        case SerialWrongStack:
                        value.valS = F("Get Wrong Stack");
                        break;
                        case CheckSumNotMatch:
                        value.valS = F("Check Sum Not Match");
                        break;
                        case FileIndexOut:
                        value.valS = F("File Index Out of Bound");
                        break;
                        case FileMismatch:
                        value.valS = F("Cannot Find File");
                        break;
                        case Advertise:
                        value.valS = F("In Advertise");
                        break;
                        default:
                        break;
                    }
                break;
                default:
                break;
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) {
        // реакция на вызов команды модуля из сценария
        // String command - имя команды после ID. (ID.Команда())
        // param - вектор ("массив") значений параметров переданных вместе с командой: ID.Команда("пар1", 22, 33) -> param[0].ValS = "пар1", param[1].ValD = 22

        if (myMP3) {
            if (command == "enableLoop") { 
                myMP3->enableLoop();
            } else if (command == "disableLoop") { 
                myMP3->disableLoop();
            } else if (command == "randomAll") { 
                myMP3->randomAll();
            } else if (command == "stop") { 
                myMP3->stop();
            } else if (command == "volume") { 
                if (param.size()) {
                    myMP3->volume(param[0].valD);
                }
            } else if (command == "playFolder") { 
                if (param.size()) {
                    myMP3->playFolder(param[0].valD, param[1].valD);    // (folderNum, fileNum)
                }
            } else if (command == "play") { 
                myMP3->play(1);  //Play the first mp3
            } else if (command == "next") { 
                myMP3->next();  //Play next mp3
            } else if (command == "previous") { 
                myMP3->previous();  //Play previous mp3
            }
        }

        return {};  // команда поддерживает возвращаемое значения. Т.е. по итогу выполнения команды или общения с внешней системой, можно вернуть значение в сценарий для дальнейшей обработки
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
*/
#include "Global.h"
#include "classes/IoTItem.h"

#include "DFRobotDFPlayerMini.h"            // Подключаем библиотеку DFPlayerMini

class Mp3 : public IoTItem {
   private:
    HardwareSerial* mySerial = nullptr;
    DFRobotDFPlayerMini* myMP3 = nullptr;
    bool isInitialised = false;

   public:
    Mp3(String parameters): IoTItem(parameters) {
        String tmpstr;
        int volumetmp = 15;
        
        jsonRead(parameters, "pins", tmpstr);
        int pinRx = selectFromMarkerToMarker(tmpstr, ",", 0).toInt();  
        int pinTx = selectFromMarkerToMarker(tmpstr, ",", 1).toInt();  
        
        jsonRead(parameters, "volume", volumetmp);

        // Используем HardwareSerial2 (на ESP32 можно переназначить на любые свободные пины)
        mySerial = new HardwareSerial(2);
        myMP3 = new DFRobotDFPlayerMini();

        if (mySerial && myMP3) {
            // Baudrate, Config, RX, TX
            mySerial->begin(9600, SERIAL_8N1, pinRx, pinTx);
            
            // Третий параметр false - отключаем долгие задержки при старте (isACK = false)
            if (myMP3->begin(*mySerial, true, false)) {
                myMP3->volume(volumetmp);
                isInitialised = true;
            } else {
                Serial.println(F("[MP3] DFPlayer Mini not responding!"));
            }
        }

        value.isDecimal = false;    // значение объекта всегда строка
    }

    void doByInterval() {
        if (!isInitialised || !myMP3) return;

        if (myMP3->available()) {
            uint8_t type = myMP3->readType();
            
            switch (type) {
                case TimeOut:
                    value.valS = F("Time Out!");
                    break;
                case WrongStack:
                    value.valS = F("Stack Wrong!");
                    break;
                case DFPlayerCardInserted:
                    value.valS = F("Card Inserted!");
                    break;
                case DFPlayerCardRemoved:
                    value.valS = F("Card Removed!");
                    break;
                case DFPlayerCardOnline:
                    value.valS = F("Card Online!");
                    break;
                case DFPlayerPlayFinished:
                    value.valS = F("Play Finished!");
                    regEvent(value.valS, F("Mp3")); // Отправляем событие в сценарий
                    break;
                case DFPlayerError:
                    switch (myMP3->read()) {
                        case Busy:
                            value.valS = F("Card not found");
                            break;
                        case Sleeping:
                            value.valS = F("Sleeping");
                            break;
                        case SerialWrongStack:
                            value.valS = F("Get Wrong Stack");
                            break;
                        case CheckSumNotMatch:
                            value.valS = F("Check Sum Not Match");
                            break;
                        case FileIndexOut:
                            value.valS = F("File Index Out of Bound");
                            break;
                        case FileMismatch:
                            value.valS = F("Cannot Find File");
                            break;
                        case Advertise:
                            value.valS = F("In Advertise");
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) {
        if (!myMP3) return {};

        if (command == F("enableLoop")) { 
            myMP3->enableLoop();
        } else if (command == F("disableLoop")) { 
            myMP3->disableLoop();
        } else if (command == F("randomAll")) { 
            myMP3->randomAll();
        } else if (command == F("stop")) { 
            myMP3->stop();
        } else if (command == F("volume")) { 
            if (param.size() >= 1) {
                myMP3->volume(param[0].valD);
            }
        } else if (command == F("playFolder")) { 
            if (param.size() >= 2) {
                myMP3->playFolder(param[0].valD, param[1].valD);    // (folderNum, fileNum)
            } else if (param.size() == 1) {
                myMP3->playFolder(1, param[0].valD);                // По умолчанию папка 01
            }
        } else if (command == F("play")) { 
            if (param.size() >= 1) {
                myMP3->play(param[0].valD);                         // Сыграть трек N
            } else {
                myMP3->start();                                    // Возобновить воспроизведение
            }
        } else if (command == F("next")) { 
            myMP3->next();
        } else if (command == F("previous")) { 
            myMP3->previous();
        }

        return {};
    }

    ~Mp3() {
        if (myMP3) delete myMP3;
        if (mySerial) delete mySerial;
    }
};

void* getAPI_Mp3(String subtype, String param) {
    if (subtype == F("Mp3")) {
        return new Mp3(param);
    } else {
        return nullptr;
    }
}