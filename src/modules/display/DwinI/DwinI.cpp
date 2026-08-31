#include "Global.h"
#include "classes/IoTUart.h"
#include "classes/IoTItem.h"

// Внешнее объявление списка всех запущенных элементов ядра
extern std::list<IoTItem*> IoTItems;

// Стандартные RGB565 цвета DWIN (соответствуют базовым цветам каналов)
static const uint16_t DWIN_DEF_COLORS[] = {
    0xFFE0, // 0 - Желтый
    0x07E0, // 1 - Зеленый
    0xF800, // 2 - Красный
    0x001F, // 3 - Синий
    0xF81F, // 4 - Фиолетовый
    0x07FF, // 5 - Голубой (Cyan)
    0xFDA0, // 6 - Оранжевый
    0xFFFF  // 7 - Белый
};

class DwinI : public IoTUart {
   private:
    uint8_t _headerBuf[260];    // Буфер для приема пакетов DWIN
    int _headerIndex = 0;

    // Универсальный метод поиска любого элемента системы по ID
    IoTItem* findItem(const String& id) {
        for (auto item : IoTItems) {
            if (item && item->getID() == id) {
                return item;
            }
        }
        return nullptr;
    }

public:
    DwinI(String parameters) : IoTUart(parameters) {
        // --- Чтение цветов из modinfo параметров ---
        String curveColors = "";
        jsonRead(parameters, "curveColors", curveColors); // Получаем "00, 10, 23..."

        if (curveColors.length() > 0) {
            // Разбиваем строку по запятым
            int i = 0;
            while (true) {
                String pair = selectFromMarkerToMarker(curveColors, ",", i);
                if (pair == "") break;
                pair.trim(); // "00", "13" и т.д.

                if (pair.length() >= 2) {
                    uint8_t chan = pair.substring(0, 1).toInt(); // Первая цифра - канал
                    uint8_t colIdx = pair.substring(1).toInt();  // Вторая цифра - индекс цвета

                    if (colIdx < 8) {
                        setCurveColor(chan, DWIN_DEF_COLORS[colIdx]);
                    }
                }
                i++;
            }
        }
    }

    // Метод прямой отправки RGB565 цвета в канал DWIN (через системный VP описателя трендов)
    void setCurveColor(uint8_t channel, uint16_t colorRGB565) {
        if (!_myUART) return;
        
        // В DWIN адрес цвета каналов графиков начинается с VP 0x0310 + offset
        // Пишем прямо в структуру Control массива графиков
        uint16_t colorAddr = 0x0312 + channel; 

        uint8_t packet[8] = {
            0x5A, 0xA5, 0x05, 0x82,
            highByte(colorAddr),
            lowByte(colorAddr),
            highByte(colorRGB565),
            lowByte(colorRGB565)
        };
        _myUART->write(packet, 8);
    }

    // =========================================================================
    // 1. ВЫГРУЗКА ИСТОРИИ ИЗ RINGBUFFER (Универсальная)
    // =========================================================================
    void sendHistoryFromBuffer(uint8_t srcChan, uint8_t dstChan) {
        if (!_myUART) return;

        IoTItem* item = findItem("logbuf"); 
        if (!item) {
            SerialPrint("E", "DwinI", "RingBuffer [logbuf] not found!");
            return;
        }

        // 1. Формируем параметр: номер канала (0, 1, 2...)
        std::vector<IoTValue> paramsCount;
        IoTValue valChan;
        valChan.valD = srcChan;
        valChan.isDecimal = true;
        paramsCount.push_back(valChan);

        // Запрашиваем количество точек
        IoTValue resCount = item->execute("getCount", paramsCount);
        uint8_t count = (uint8_t)resCount.valD;

        SerialPrint("I", "DwinI", "drawHistory: srcChan=" + String(srcChan) + " -> dstChan=" + String(dstChan) + " | Points: " + String(count));

        if (count == 0) return;

        // --- ШЛИМ КОМАНДУ ОЧИСТКИ КАНАЛА НА ЭКРАНЕ DWIN ---
        // Третий байт с конца (0x02) говорит дисплею: "Очистить буфер этого канала"
        uint8_t clearPacket[14] = {
            0x5A, 0xA5, 0x0B, 0x82, 
            0x03, 0x10,             
            0x5A, 0xA5, 0x01, 0x00, 
            dstChan,                // Какой канал стираем (0..7)
            0x02,                   // 0x02 = Clear Channel Command
            0x00, 0x00
        };
        _myUART->write(clearPacket, 14);
        delay(5); // Небольшая пауза, чтобы экран успел сбросить буфер

        // 2. В цикле забираем точки и рисуем свежую историю
        for (uint8_t i = 0; i < count; i++) {
            std::vector<IoTValue> paramsPoint;
            
            IoTValue pChan, pIdx;
            pChan.valD = srcChan;
            pChan.isDecimal = true;

            pIdx.valD = i;
            pIdx.isDecimal = true;

            paramsPoint.push_back(pChan);
            paramsPoint.push_back(pIdx);

            IoTValue resPoint = item->execute("getPoint", paramsPoint);
            uint16_t val = (uint16_t)resPoint.valD;

            // Отправляем точку в DWIN
            uint8_t packet[14] = {
                0x5A, 0xA5, 0x0B, 0x82, 
                0x03, 0x10,             
                0x5A, 0xA5, 0x01, 0x00, 
                dstChan,                
                0x01,                   // 0x01 = Add Point Command
                (uint8_t)(val >> 8),    
                (uint8_t)(val & 0xFF)   
            };

            _myUART->write(packet, 14);
            delay(2);
        }
    }

    // =========================================================================
    // 2. ПРИЕМ И ПАРСИНГ ДАННЫХ ИЗ UART (События с тачскрина DWIN)
    // =========================================================================
    void uartHandle() {
        if (!_myUART) return;
        
        while (_myUART->available()) {
            _headerBuf[_headerIndex] = _myUART->read();

            // Ищем заголовок кадра DWIN: 5A A5 [Length] 82 ...
            if ((_headerIndex == 0 && _headerBuf[0] != 0x5A) || 
                (_headerIndex == 1 && _headerBuf[1] != 0xA5) || 
                (_headerIndex == 2 && _headerBuf[2] == 0) || 
                (_headerIndex == 3 && _headerBuf[3] != 0x82)) {
                _headerIndex = 0;
                continue;
            }

            // Пакет получен полностью
            if (_headerIndex == _headerBuf[2] + 2) {
                char buf[5];
                hex2string(_headerBuf + 4, 2, buf);
                String vpAddr = String(buf);

                 // --- А) СМЕНА СТРАНИЦЫ (VP 0x0084) ---
                if (vpAddr == "0084") {
                    uint16_t page = (_headerBuf[7] << 8) | _headerBuf[8];
                    
                    // Просто пробрасываем номер страницы в систему как событие!
                    generateOrder("dwin_page", String(page));
                }
                // --- Б) КНОПКИ И ПЕРЕМЕННЫЕ С ЭКРАНА ---
                else {
                    String valStr = String((_headerBuf[7] << 8) | _headerBuf[8]);
                    String id = "_" + vpAddr;
                    
                    // Поиск элемента с VP-адресом
                    for (auto item : IoTItems) {
                        if (item && item->getID().endsWith(id)) {
                            generateOrder(item->getID(), valStr);
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

    // =========================================================================
    // 3. ОТПРАВКА ОБНОВЛЕНИЙ ИЗ СИСТЕМЫ НА ЭКРАН (Реакция на события)
    // =========================================================================
    void onRegEvent(IoTItem* eventItem) {
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

        // --- Вывод 16-битного INT (VP_xxxx или VP_xxxxi) ---
        if (typeOfVP == 'i') {
            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write(0x05);   
            _myUART->write(0x82);   
            uartPrintHex(VP);       
            
            int val = eventItem->value.isDecimal ? (int)eventItem->value.valD : atoi(eventItem->value.valS.c_str());
            _myUART->write(highByte(val));
            _myUART->write(lowByte(val));
        }
        // --- Вывод строки UTF-16 (VP_xxxxs) ---
        else if (typeOfVP == 's') {
            if (eventItem->value.isDecimal) {
                eventItem->value.valS = eventItem->getValue();
            }

            int u16counter = 0;
            const char* valSptr = eventItem->value.valS.c_str();
            for (size_t i = 0; i < eventItem->value.valS.length(); i++) {
                if ((uint8_t)valSptr[i] > 200) u16counter++;
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
        // --- Вывод Float IEEE 754 (VP_xxxxf) ---
        else if (typeOfVP == 'f') {
            _myUART->write(0x5A);
            _myUART->write(0xA5);
            _myUART->write(0x07);   
            _myUART->write(0x82);   
            uartPrintHex(VP);       
            
            float valFloat = eventItem->value.isDecimal ? (float)eventItem->value.valD : atof(eventItem->value.valS.c_str());
            byte hex[4];
            memcpy(hex, &valFloat, 4);

            _myUART->write(hex[3]);
            _myUART->write(hex[2]);
            _myUART->write(hex[1]);
            _myUART->write(hex[0]);
        }
        // --- Смена страницы по событию (VP_xxxxr) --- 
        else if (typeOfVP == 'r') {
            int page = eventItem->value.isDecimal ? (int)eventItem->value.valD : atoi(eventItem->value.valS.c_str());
            
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                highByte(page), 
                lowByte(page)
            };
            _myUART->write(packet, 10);
        }
    }

    void loop() {
        IoTItem::loop();
    }

IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!_myUART) return {};

        // 1. Смена цвета канала: dwin.setColor(channel, colorIndex_or_RGB565)
        if (command == "setColor" && !param.empty()) {
            uint8_t channel = (uint8_t)param[0].valD;
            uint16_t colorVal = param.size() > 1 ? (uint16_t)param[1].valD : 0;

            // Если передали индекс от 0 до 7 — берем готовый цвет из палитры
            if (colorVal < 8) {
                colorVal = DWIN_DEF_COLORS[colorVal];
            }
            // Иначе считаем, что передали сразу готовый RGB565 цвет (например 0xFFE0)

            setCurveColor(channel, colorVal);
        }

        // 2. Команда смены страницы: dwin.setPage(N)
        else if (command == "setPage" && !param.empty()) {
            uint16_t page = (uint16_t)param[0].valD;
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                highByte(page), 
                lowByte(page)
            };
            _myUART->write(packet, 10);
        }
        // 3. Старые команды: dwin.scr0() ... dwin.scr9()
        else if (command.startsWith("scr")) {
            uint16_t page = command.substring(3).toInt();
            uint8_t packet[10] = {
                0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, 
                0x00, 
                (uint8_t)page
            };
            _myUART->write(packet, 10);
        }
        // 4. Выгрузка истории
        else if (command == "drawHistory") {
            // Забираем номер канала из первого параметра
            uint8_t src = param.size() > 0 ? (uint8_t)param[0].valD : 0;
            // Забираем приемный канал на DWIN из второго параметра
            uint8_t dst = param.size() > 1 ? (uint8_t)param[1].valD : 0;
            
            sendHistoryFromBuffer(src, dst);
        }
        // 5. Добавление одиночной точки
        else if (command == "addPoint") {
            uint8_t channel = param.size() > 0 ? (uint8_t)param[0].valD : 0;
            uint16_t val = param.size() > 1 ? (uint16_t)param[1].valD : 150;

            uint8_t packet[14] = {
                0x5A, 0xA5, 0x0B, 0x82, 
                0x03, 0x10,             
                0x5A, 0xA5, 0x01, 0x00, 
                channel,                
                0x01,                   
                (uint8_t)(val >> 8),    
                (uint8_t)(val & 0xFF)   
            };
            _myUART->write(packet, 14);
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
