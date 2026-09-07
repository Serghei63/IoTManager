#include "Global.h"
#include "classes/IoTItem.h"

class RingBuffer : public IoTItem {
   private:
    uint16_t** _buffer = nullptr; // Динамический двухмерный массив [channels][size]
    uint16_t* _head = nullptr;   // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
    uint16_t* _count = nullptr;  // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
    
    uint8_t _maxChannels = 0;
    uint16_t _bufferSize = 100;
    int _debug = 0; // Для jsonRead(int&)
    
    std::vector<String> _targetIDs; // Список ID логируемых элементов

   public:
    RingBuffer(String parameters) : IoTItem(parameters) {
        // 1. Парсим размер буфера
        String sizeStr;
        jsonRead(parameters, "size", sizeStr);
        if (sizeStr.length()) _bufferSize = sizeStr.toInt();

        // 2. Парсим отладку (0/1)
        jsonRead(parameters, "debug", _debug);

        // 3. Парсим список привязанных элементов
        String targetsStr;
        jsonRead(parameters, "targets", targetsStr);
        
        if (targetsStr.length()) {
            int prev = 0;
            int pos = 0;
            while ((pos = targetsStr.indexOf(',', prev)) != -1) {
                _targetIDs.push_back(targetsStr.substring(prev, pos));
                prev = pos + 1;
            }
            _targetIDs.push_back(targetsStr.substring(prev));
        }

        _maxChannels = _targetIDs.size();
        if (_maxChannels == 0) _maxChannels = 4;

        // 4. Выделяем динамическую память под буферы
        _buffer = new uint16_t*[_maxChannels]; //
        _head = new uint16_t[_maxChannels]();   // 👈 ИСПРАВЛЕНО: uint16_t
        _count = new uint16_t[_maxChannels]();  // 👈 ИСПРАВЛЕНО: uint16_t

        for (uint8_t i = 0; i < _maxChannels; i++) { 
            _buffer[i] = new uint16_t[_bufferSize]();
        }
    }

    // Добавление точки в конкретный канал
    void push(uint8_t ch, uint16_t val) {
        if (ch >= _maxChannels || !_buffer) return;

        _buffer[ch][_head[ch]] = val;
        _head[ch] = (_head[ch] + 1) % _bufferSize;
        if (_count[ch] < _bufferSize) _count[ch]++;

        if (_debug) {
            String chName = (ch < _targetIDs.size()) ? _targetIDs[ch] : String(ch);
            SerialPrint("I", "RingBuffer", "Add val: " + String(val) + " -> " + chName + " (Count: " + String(_count[ch]) + ")");
        }
    }

    IoTItem* findItem(const String& id) {
        for (auto item : IoTItems) {
            if (item && item->getID() == id) {
                return item;
            }
        }
        return nullptr;
    }

    void doByInterval() override {
        for (uint8_t i = 0; i < _targetIDs.size(); i++) {
            IoTItem* item = findItem(_targetIDs[i]);
            if (item) {
                uint16_t val = (uint16_t)item->value.valD;
                push(i, val);
            }
        }
    }

    void onRegEvent(IoTItem* eventItem) override {
        if (!eventItem) return;
        
        String eventID = eventItem->getID();
        for (uint8_t i = 0; i < _targetIDs.size(); i++) {
            if (_targetIDs[i] == eventID) {
                push(i, (uint16_t)eventItem->value.valD);
                break;
            }
        }
    }

    // 👈 ИСПРАВЛЕНО: index теперь uint16_t вместо uint8_t
    uint16_t getPoint(uint8_t ch, uint16_t index) {
        if (ch >= _maxChannels || index >= _count[ch]) return 0;
        uint16_t startIndex = (_count[ch] < _bufferSize) ? 0 : _head[ch]; // 👈 ИСПРАВЛЕНО: uint16_t
        uint16_t realIdx = (startIndex + index) % _bufferSize;            // 👈 ИСПРАВЛЕНО: uint16_t
        return _buffer[ch][realIdx]; 
    }

    // 👈 ИСПРАВЛЕНО: Возвращает uint16_t вместо uint8_t
    uint16_t getCount(uint8_t ch) {
        return (ch < _maxChannels) ? _count[ch] : 0;
    }

    uint8_t getMaxChannels() {
        return _maxChannels; 
    }

    void setValue(const IoTValue& Value, bool isTrigger = true) override {
        String cmd = Value.valS;
        
        if (cmd == F("print")) {
            printBuffer();
        }
        else if (cmd == F("save")) {
            doByInterval();
        }
        else {
            IoTItem::setValue(Value, isTrigger);
        }
    }

    void printBuffer(int targetCh = 255) {
        SerialPrint("I", "RingBuffer", "========== DUMP ==========");
        for (uint8_t ch = 0; ch < _maxChannels; ch++) {
            if (targetCh != 255 && targetCh != ch) continue;

            String targetID = (ch < _targetIDs.size()) ? _targetIDs[ch] : "ch_" + String(ch);
            String out = "Ch " + String(ch) + " [" + targetID + "] (" + String(_count[ch]) + "/" + String(_bufferSize) + "): [ ";
            
            // 👈 ИСПРАВЛЕНО: цикл по i до uint16_t
            for (uint16_t i = 0; i < _count[ch]; i++) {
                out += String(getPoint(ch, i));
                if (i < _count[ch] - 1) out += ", ";
            }
            out += " ]";
            SerialPrint("I", "RingBuffer", out);
        }
        SerialPrint("I", "RingBuffer", "==========================");
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == F("push") && param.size() >= 2) {
            push((uint8_t)param[0].valD, (uint16_t)param[1].valD);
        }
        else if (command == F("save")) {
            doByInterval();
        }
        else if (command == F("print")) {
            uint8_t targetCh = (param.size() > 0) ? (uint8_t)param[0].valD : 255;
            printBuffer(targetCh);
        }
        else if (command == F("getCount") && param.size() > 0) {
            uint8_t ch = (uint8_t)param[0].valD;
            IoTValue res;
            res.valD = getCount(ch);
            res.isDecimal = true;
            return res;
        }
        else if (command == F("getPoint") && param.size() >= 2) {
            uint8_t ch = (uint8_t)param[0].valD;
            uint16_t idx = (uint16_t)param[1].valD; // 👈 ИСПРАВЛЕНО: uint16_t вместо uint8_t
            IoTValue res;
            res.valD = getPoint(ch, idx);
            res.isDecimal = true;
            return res; 
        }
        return {};
    }

    ~RingBuffer() { 
        if (_buffer) {
            for (uint8_t i = 0; i < _maxChannels; i++) {
                delete[] _buffer[i];
            }
            delete[] _buffer; 
        }
        if (_head) delete[] _head; 
        if (_count) delete[] _count;
    }
};
void *getAPI_RingBuffer(String subtype, String param) {
    if (subtype == F("RingBuffer")) {
        return new RingBuffer(param);
    }
    return nullptr;
}