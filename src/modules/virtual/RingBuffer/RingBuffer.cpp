#include "Global.h" //[cite: 1]
#include "classes/IoTItem.h" //[cite: 1]

class RingBuffer : public IoTItem { //[cite: 1]
   private:
    uint16_t** _buffer = nullptr; // Динамический двухмерный массив [channels][size][cite: 1]
    uint16_t* _head = nullptr;   // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
    uint16_t* _count = nullptr;  // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
    
    uint8_t _maxChannels = 0; //[cite: 1]
    uint16_t _bufferSize = 100; //[cite: 1]
    int _debug = 0; // Для jsonRead(int&)[cite: 1]
    
    std::vector<String> _targetIDs; // Список ID логируемых элементов[cite: 1]

   public:
    RingBuffer(String parameters) : IoTItem(parameters) { //[cite: 1]
        // 1. Парсим размер буфера[cite: 1]
        String sizeStr; //[cite: 1]
        jsonRead(parameters, "size", sizeStr); //[cite: 1]
        if (sizeStr.length()) _bufferSize = sizeStr.toInt(); //[cite: 1]

        // 2. Парсим отладку (0/1)[cite: 1]
        jsonRead(parameters, "debug", _debug); //[cite: 1]

        // 3. Парсим список привязанных элементов[cite: 1]
        String targetsStr; //[cite: 1]
        jsonRead(parameters, "targets", targetsStr); //[cite: 1]
        
        if (targetsStr.length()) { //[cite: 1]
            int prev = 0; //[cite: 1]
            int pos = 0; //[cite: 1]
            while ((pos = targetsStr.indexOf(',', prev)) != -1) { //[cite: 1]
                _targetIDs.push_back(targetsStr.substring(prev, pos)); //[cite: 1]
                prev = pos + 1; //[cite: 1]
            }
            _targetIDs.push_back(targetsStr.substring(prev)); //[cite: 1]
        }

        _maxChannels = _targetIDs.size(); //[cite: 1]
        if (_maxChannels == 0) _maxChannels = 4; //[cite: 1]

        // 4. Выделяем динамическую память под буферы[cite: 1]
        _buffer = new uint16_t*[_maxChannels]; //[cite: 1]
        _head = new uint16_t[_maxChannels]();   // 👈 ИСПРАВЛЕНО: uint16_t
        _count = new uint16_t[_maxChannels]();  // 👈 ИСПРАВЛЕНО: uint16_t

        for (uint8_t i = 0; i < _maxChannels; i++) { //[cite: 1]
            _buffer[i] = new uint16_t[_bufferSize](); //[cite: 1]
        }
    }

    // Добавление точки в конкретный канал[cite: 1]
    void push(uint8_t ch, uint16_t val) { //[cite: 1]
        if (ch >= _maxChannels || !_buffer) return; //[cite: 1]

        _buffer[ch][_head[ch]] = val; //[cite: 1]
        _head[ch] = (_head[ch] + 1) % _bufferSize; //[cite: 1]
        if (_count[ch] < _bufferSize) _count[ch]++; //[cite: 1]

        if (_debug) { //[cite: 1]
            String chName = (ch < _targetIDs.size()) ? _targetIDs[ch] : String(ch); //[cite: 1]
            SerialPrint("I", "RingBuffer", "Add val: " + String(val) + " -> " + chName + " (Count: " + String(_count[ch]) + ")"); //[cite: 1]
        }
    }

    IoTItem* findItem(const String& id) { //[cite: 1]
        for (auto item : IoTItems) { //[cite: 1]
            if (item && item->getID() == id) { //[cite: 1]
                return item; //[cite: 1]
            }
        }
        return nullptr; //[cite: 1]
    }

    void doByInterval() override { //[cite: 1]
        for (uint8_t i = 0; i < _targetIDs.size(); i++) { //[cite: 1]
            IoTItem* item = findItem(_targetIDs[i]); //[cite: 1]
            if (item) { //[cite: 1]
                uint16_t val = (uint16_t)item->value.valD; //[cite: 1]
                push(i, val); //[cite: 1]
            }
        }
    }

    void onRegEvent(IoTItem* eventItem) override { //[cite: 1]
        if (!eventItem) return; //[cite: 1]
        
        String eventID = eventItem->getID(); //[cite: 1]
        for (uint8_t i = 0; i < _targetIDs.size(); i++) { //[cite: 1]
            if (_targetIDs[i] == eventID) { //[cite: 1]
                push(i, (uint16_t)eventItem->value.valD); //[cite: 1]
                break; //[cite: 1]
            }
        }
    }

    // 👈 ИСПРАВЛЕНО: index теперь uint16_t вместо uint8_t
    uint16_t getPoint(uint8_t ch, uint16_t index) {
        if (ch >= _maxChannels || index >= _count[ch]) return 0; //[cite: 1]
        uint16_t startIndex = (_count[ch] < _bufferSize) ? 0 : _head[ch]; // 👈 ИСПРАВЛЕНО: uint16_t
        uint16_t realIdx = (startIndex + index) % _bufferSize;            // 👈 ИСПРАВЛЕНО: uint16_t
        return _buffer[ch][realIdx]; //[cite: 1]
    }

    // 👈 ИСПРАВЛЕНО: Возвращает uint16_t вместо uint8_t
    uint16_t getCount(uint8_t ch) {
        return (ch < _maxChannels) ? _count[ch] : 0; //[cite: 1]
    }

    uint8_t getMaxChannels() { //[cite: 1]
        return _maxChannels; //[cite: 1]
    }

    void setValue(const IoTValue& Value, bool isTrigger = true) override { //[cite: 1]
        String cmd = Value.valS; //[cite: 1]
        
        if (cmd == F("print")) { //[cite: 1]
            printBuffer(); //[cite: 1]
        }
        else if (cmd == F("save")) { //[cite: 1]
            doByInterval(); //[cite: 1]
        }
        else {
            IoTItem::setValue(Value, isTrigger); //[cite: 1]
        }
    }

    void printBuffer(int targetCh = 255) { //[cite: 1]
        SerialPrint("I", "RingBuffer", "========== DUMP =========="); //[cite: 1]
        for (uint8_t ch = 0; ch < _maxChannels; ch++) { //[cite: 1]
            if (targetCh != 255 && targetCh != ch) continue; //[cite: 1]

            String targetID = (ch < _targetIDs.size()) ? _targetIDs[ch] : "ch_" + String(ch); //[cite: 1]
            String out = "Ch " + String(ch) + " [" + targetID + "] (" + String(_count[ch]) + "/" + String(_bufferSize) + "): [ "; //[cite: 1]
            
            // 👈 ИСПРАВЛЕНО: цикл по i до uint16_t
            for (uint16_t i = 0; i < _count[ch]; i++) {
                out += String(getPoint(ch, i)); //[cite: 1]
                if (i < _count[ch] - 1) out += ", "; //[cite: 1]
            }
            out += " ]"; //[cite: 1]
            SerialPrint("I", "RingBuffer", out); //[cite: 1]
        }
        SerialPrint("I", "RingBuffer", "=========================="); //[cite: 1]
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override { //[cite: 1]
        if (command == F("push") && param.size() >= 2) { //[cite: 1]
            push((uint8_t)param[0].valD, (uint16_t)param[1].valD); //[cite: 1]
        }
        else if (command == F("save")) { //[cite: 1]
            doByInterval(); //[cite: 1]
        }
        else if (command == F("print")) { //[cite: 1]
            uint8_t targetCh = (param.size() > 0) ? (uint8_t)param[0].valD : 255; //[cite: 1]
            printBuffer(targetCh); //[cite: 1]
        }
        else if (command == F("getCount") && param.size() > 0) { //[cite: 1]
            uint8_t ch = (uint8_t)param[0].valD; //[cite: 1]
            IoTValue res; //[cite: 1]
            res.valD = getCount(ch); //[cite: 1]
            res.isDecimal = true; //[cite: 1]
            return res; //[cite: 1]
        }
        else if (command == F("getPoint") && param.size() >= 2) { //[cite: 1]
            uint8_t ch = (uint8_t)param[0].valD; //[cite: 1]
            uint16_t idx = (uint16_t)param[1].valD; // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
            IoTValue res; //[cite: 1]
            res.valD = getPoint(ch, idx); //[cite: 1]
            res.isDecimal = true; //[cite: 1]
            return res; //[cite: 1]
        }
        return {}; //[cite: 1]
    }

    ~RingBuffer() { //[cite: 1]
        if (_buffer) { //[cite: 1]
            for (uint8_t i = 0; i < _maxChannels; i++) { //[cite: 1]
                delete[] _buffer[i]; //[cite: 1]
            }
            delete[] _buffer; //[cite: 1]
        }
        if (_head) delete[] _head; //[cite: 1]
        if (_count) delete[] _count; //[cite: 1]
    }
};

void *getAPI_RingBuffer(String subtype, String param) { //[cite: 1]
    if (subtype == F("RingBuffer")) { //[cite: 1]
        return new RingBuffer(param); //[cite: 1]
    }
    return nullptr; //[cite: 1]
}