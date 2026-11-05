#include "Global.h"
#include "classes/IoTUart.h"
#include "classes/IoTItem.h"

extern std::list<IoTItem*> IoTItems;

class DwinI : public IoTUart {
   private:
    uint8_t _headerBuf[260];
    int _headerIndex = 0;
    bool _debug = false;

    // Поиск элемента в системе по ID
    IoTItem* findItem(const String& id) {
        for (auto item : IoTItems) {
            if (item && item->getID() == id) {
                return item;
            }
        }
        return nullptr;
    }

    // Отправка пакета в UART с логированием в HEX (только при _debug == true)
    void sendToUart(const uint8_t* buf, size_t len) {
        if (!_myUART) return;
        
        _myUART->write(buf, len);

        if (_debug) {
            String hexOut = "";
            for (size_t i = 0; i < len; i++) {
                char b[4];
                sprintf(b, "%02X ", buf[i]);
                hexOut += String(b);
            }
            SerialPrint("I", "DwinI", "TX -> DWIN: " + hexOut);
        }
    }

public:
    DwinI(String parameters) : IoTUart(parameters) {
        jsonRead(parameters, "debug", _debug);
    }

    // Запись сырого 16-битного значения по адресу VP/SP
    void writeRaw16(uint16_t addr, uint16_t value) {
        if (!_myUART) return;

        uint8_t packet[8] = {
            0x5A, 0xA5, 0x05, 0x82,
            (uint8_t)((addr >> 8) & 0xFF),
            (uint8_t)(addr & 0xFF),
            (uint8_t)((value >> 8) & 0xFF),
            (uint8_t)(value & 0xFF)
        };

        sendToUart(packet, 8);

        if (_debug) {
            char b[64];
            sprintf(b, "writeRaw -> Addr: 0x%04X | Val: 0x%04X (%d)", addr, value, value);
            SerialPrint("I", "DwinI", String(b));
        }
    }

    // Отрисовка конкретного буфера истории в указанный канал
    void drawHistoryToChannel(uint8_t srcChan, uint8_t dstChan) {
        if (!_myUART) return;

        IoTItem* item = findItem("logbuf"); 
        if (!item) {
            if (_debug) SerialPrint("E", "DwinI", "RingBuffer [logbuf] not found!");
            return;
        }

        std::vector<IoTValue> paramsCount;
        IoTValue valChan;
        valChan.valD = srcChan;
        valChan.isDecimal = true;
        paramsCount.push_back(valChan);

        IoTValue resCount = item->execute("getCount", paramsCount);
        uint16_t count = (uint16_t)resCount.valD;

        if (_debug) {
            SerialPrint("I", "DwinI", "drawHistoryToChannel: srcBuf=" + String(srcChan) + " -> dstChan=" + String(dstChan) + " | Points: " + String(count));
        }

        if (count == 0) return;

        // 1. Очистка выбранного канала тренда
        uint8_t clearPacket[14] = {
            0x5A, 0xA5, 0x0B, 0x82, 
            0x03, 0x10,             
            0x5A, 0xA5, 0x01, 0x00, 
            dstChan, 0x02, 0x00, 0x00
        };
        sendToUart(clearPacket, 14);
        _myUART->flush();
        delay(30);

        // 2. Отправка массива точек порциями по 30 штук
        const uint8_t CHUNK_SIZE = 30;
        std::vector<IoTValue> paramsPoint(2);
        paramsPoint[0].valD = srcChan;
        paramsPoint[0].isDecimal = true;
        paramsPoint[1].isDecimal = true;

        for (uint16_t offset = 0; offset < count; offset += CHUNK_SIZE) {
            uint16_t pointsInChunk = min((uint16_t)CHUNK_SIZE, (uint16_t)(count - offset));
            uint16_t dataLen = 8 + (pointsInChunk * 2); 
            uint16_t packetLen = 4 + dataLen;

            uint8_t chunkPacket[packetLen];
            
            chunkPacket[0] = 0x5A;
            chunkPacket[1] = 0xA5;
            chunkPacket[2] = dataLen + 1;
            chunkPacket[3] = 0x82;
            chunkPacket[4] = 0x03;
            chunkPacket[5] = 0x10;
            chunkPacket[6] = 0x5A;
            chunkPacket[7] = 0xA5;
            chunkPacket[8] = 0x01;
            chunkPacket[9] = 0x00;
            chunkPacket[10] = dstChan;
            chunkPacket[11] = pointsInChunk;

            for (uint16_t p = 0; p < pointsInChunk; p++) {
                paramsPoint[1].valD = offset + p;
                IoTValue resPoint = item->execute("getPoint", paramsPoint);
                uint16_t val = (uint16_t)resPoint.valD;

                chunkPacket[12 + (p * 2)]     = (uint8_t)(val >> 8);
                chunkPacket[12 + (p * 2) + 1] = (uint8_t)(val & 0xFF);
            }

            sendToUart(chunkPacket, packetLen);
            _myUART->flush();
            delay(10); 
            yield();
        }
    }

    // Обработчик сырого UART потока с DWIN
    void uartHandle() {
        if (!_myUART) return;

        while (_myUART->available()) {
            uint8_t incomingByte = _myUART->read();

            if (_debug) {
                Serial.printf("[DWIN RAW RX]: 0x%02X\n", incomingByte);
            }

            _headerBuf[_headerIndex] = incomingByte;

            // Проверка заголовков DWIN (0x5A 0xA5)
            if ((_headerIndex == 0 && _headerBuf[0] != 0x5A) || 
                (_headerIndex == 1 && _headerBuf[1] != 0xA5) || 
                (_headerIndex == 2 && _headerBuf[2] == 0) || 
                (_headerIndex == 3 && (_headerBuf[3] != 0x83 && _headerBuf[3] != 0x82))) {
                _headerIndex = 0;
                continue;
            }

            // Пакет полностью собран
            if (_headerIndex >= 2 && _headerIndex == (_headerBuf[2] + 2)) {
                if (_debug) {
                    String hexDump = "";
                    for (int i = 0; i <= _headerIndex; i++) {
                        char b[4];
                        sprintf(b, "%02X ", _headerBuf[i]);
                        hexDump += String(b);
                    }
                    SerialPrint("I", "DwinI", "RX <- DWIN: " + hexDump);
                }

                // Пропуск стандартного ответа записи 0x82 OK
                if (_headerBuf[3] == 0x82 && _headerBuf[4] == 0x4F && _headerBuf[5] == 0x4B) {
                    _headerIndex = 0;
                    return;
                }

                char buf[5];
                hex2string(_headerBuf + 4, 2, buf);
                String vpAddr = String(buf);

                // Событие переключения страницы (VP 0x0084)
                if (vpAddr == "0084") {
                    uint16_t page = (_headerBuf[7] << 8) | _headerBuf[8];
                    generateOrder("dwin_page", String(page));
                }
                // Нажатия на элементы / переменные
                else {
                    uint8_t dataStart = (_headerBuf[3] == 0x83) ? 7 : 6;
                    uint16_t rawVal = (_headerBuf[dataStart] << 8) | _headerBuf[dataStart + 1];
                    String valStr = String(rawVal);
                    String idTarget = "_" + vpAddr;

                    for (auto item : IoTItems) {
                        if (item && item->getID().endsWith(idTarget)) {
                            if (item->getValue() != valStr) {
                                generateOrder(item->getID(), valStr);
                            }
                            break;
                        }
                    }
                }
                
                _headerIndex = 0;
                return;
            }

            _headerIndex++;
            if (_headerIndex >= 260) _headerIndex = 0;
        }
    }

    void onRegEvent(IoTItem* eventItem) override {
        if (!_myUART || !eventItem) return; 

        String printStr = eventItem->getID();
        int indexOf_ = printStr.indexOf("_");
        if (indexOf_ == -1 || indexOf_ == 0) return;

        uint8_t sizeOfVPPart = printStr.length() - indexOf_ - 1;
        if (sizeOfVPPart < 4) return;
        
        char typeOfVP = (sizeOfVPPart == 5) ? printStr.charAt(indexOf_ + 5) : 0;
        String VP = printStr.substring(indexOf_ + 1, indexOf_ + 5);

        if (typeOfVP == 0) {
            typeOfVP = eventItem->value.isDecimal ? 'i' : 's';
        }

        if (typeOfVP == 'i') {
            int val = eventItem->value.isDecimal ? (int)eventItem->value.valD : atoi(eventItem->value.valS.c_str());
            uint8_t packet[8] = {
                0x5A, 0xA5, 0x05, 0x82,
                (uint8_t)strtol(VP.substring(0, 2).c_str(), NULL, 16),
                (uint8_t)strtol(VP.substring(2, 4).c_str(), NULL, 16),
                highByte(val),
                lowByte(val)
            };
            sendToUart(packet, 8);
        }
        else if (typeOfVP == 's') {
            if (eventItem->value.isDecimal) {
                eventItem->value.valS = eventItem->getValue();
            }

            int u16counter = 0;
            const char* valSptr = eventItem->value.valS.c_str();
            for (size_t i = 0; i < eventItem->value.valS.length(); i++) {
                if ((uint16_t)valSptr[i] > 200) u16counter++;
            }

            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write((eventItem->value.valS.length() - u16counter) * 2 + 5);
            _myUART->write(0x82);   
            uartPrintHex(VP);   
            uartPrintStrInUTF16(eventItem->value.valS.c_str(), eventItem->value.valS.length());
            _myUART->write(0xFF);   
            _myUART->write(0xFF);
        }
        else if (typeOfVP == 'f') {
            float valFloat = eventItem->value.isDecimal ? (float)eventItem->value.valD : atof(eventItem->value.valS.c_str());
            byte hex[4];
            memcpy(hex, &valFloat, 4);

            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82,
                (uint8_t)strtol(VP.substring(0, 2).c_str(), NULL, 16),
                (uint8_t)strtol(VP.substring(2, 4).c_str(), NULL, 16),
                hex[3], hex[2], hex[1], hex[0]
            };
            sendToUart(packet, 10);
        }
        else if (typeOfVP == 'r') {
            int page = eventItem->value.isDecimal ? (int)eventItem->value.valD : atoi(eventItem->value.valS.c_str());
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                highByte(page), 
                lowByte(page)
            };
            sendToUart(packet, 10);
        }
    }

    void loop() override {
        IoTItem::loop();
        uartHandle();
    }

    // Исполнение команд сценариев
    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!_myUART) return {};

        if (_debug) {
            SerialPrint("I", "DwinI", "EXECUTE: " + command + " (Params: " + String(param.size()) + ")");
        }

        if (command == "writeRaw") {
            if (param.size() >= 2) {
                uint16_t addr = (uint16_t)param[0].valD;
                uint16_t val  = (uint16_t)param[1].valD;

                if (!param[0].isDecimal && param[0].valS.length() > 0) {
                    addr = (uint16_t)strtol(param[0].valS.c_str(), NULL, 0);
                }

                writeRaw16(addr, val);
            }
        }
        else if (command == "setPage" && !param.empty()) {
            uint16_t page = (uint16_t)param[0].valD;
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                (uint8_t)(page >> 8),   
                (uint8_t)(page & 0xFF)  
            };
            sendToUart(packet, 10);
        }
        else if (command.startsWith("scr")) {
            uint16_t page = command.substring(3).toInt();
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                0x00, 
                (uint8_t)page
            };
            sendToUart(packet, 10);
        }
        else if (command == "drawHistoryToChannel") {
            uint8_t src = 0;
            uint8_t dst = 0;

            if (param.size() > 0) {
                src = param[0].isDecimal ? (uint8_t)param[0].valD : (uint8_t)param[0].valS.toInt();
            }
            if (param.size() > 1) {
                dst = param[1].isDecimal ? (uint8_t)param[1].valD : (uint8_t)param[1].valS.toInt();
            }

            drawHistoryToChannel(src, dst);
        }
        else if (command == "addPoint") {
            if (param.size() >= 2) {
                uint8_t targetChannel = param[0].isDecimal ? (uint8_t)param[0].valD : (uint8_t)param[0].valS.toInt();
                uint16_t val = param[1].isDecimal ? (uint16_t)param[1].valD : (uint16_t)param[1].valS.toInt();

                targetChannel &= 0x07;

                uint8_t packet[14] = {
                    0x5A, 0xA5, 0x0B, 0x82, 
                    0x03, 0x10,             
                    0x5A, 0xA5, 0x01, 0x00, 
                    targetChannel,          
                    0x01,                   
                    (uint8_t)(val >> 8),    
                    (uint8_t)(val & 0xFF)   
                };
                
                sendToUart(packet, 14);
            }
        }

        return {};
    }

    ~DwinI() {};
};

void *getAPI_DwinI(String subtype, String param) {
    if (subtype == F("DwinI")) {
        return new DwinI(param);
    }
    return nullptr;
}