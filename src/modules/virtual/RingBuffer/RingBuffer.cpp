#include "Global.h"
#include "classes/IoTItem.h"


class RingBuffer : public IoTItem {
   private:
    uint16_t** _buffer = nullptr; // Динамический двухмерный массив [channels][size]
    uint8_t* _head = nullptr;
    uint8_t* _count = nullptr;
    
    uint8_t _maxChannels = 0;
    uint16_t _bufferSize = 100;
    
    std::vector<String> _targetIDs; // Список ID логируемых элементов

   public:
    RingBuffer(String parameters) : IoTItem(parameters) {
        // 1. Парсим размер буфера (по умолчанию 100 точек)
        String sizeStr;
        jsonRead(parameters, "size", sizeStr);
        if (sizeStr.length()) _bufferSize = sizeStr.toInt();

        // 2. Парсим список привязанных элементов (targets: "temp1,press1,pzem_p")
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
        if (_maxChannels == 0) _maxChannels = 4; // Резерв под 4 канала по умолчанию

        // 3. Выделяем динамическую память под буферы
        _buffer = new uint16_t*[_maxChannels];
        _head = new uint8_t[_maxChannels]();
        _count = new uint8_t[_maxChannels]();

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
    }

// Вспомогательная универсальная функция поиска элемента по ID через std::list
IoTItem* findItem(const String& id) {
    for (auto item : IoTItems) {
        if (item && item->getID() == id) {
            return item;
        }
    }
    return nullptr;
}

    // Вызывается базовым классом IoTItem с интервалом "int" из config.json
    void doByInterval() override {
        for (uint8_t i = 0; i < _targetIDs.size(); i++) {
            IoTItem* item = findItem(_targetIDs[i]);
            if (item) {
                uint16_t val = (uint16_t)item->value.valD;
                push(i, val);
            }
        }
    }

    // Автоматическое логирование при смене значения элементов в реальном времени
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

    // Геттеры для внешних модулей (DWIN, Nextion и т.д.)
    uint16_t getPoint(uint8_t ch, uint8_t index) {
        if (ch >= _maxChannels || index >= _count[ch]) return 0;
        uint8_t startIndex = (_count[ch] < _bufferSize) ? 0 : _head[ch];
        uint8_t realIdx = (startIndex + index) % _bufferSize;
        return _buffer[ch][realIdx];
    }

    uint8_t getCount(uint8_t ch) {
        return (ch < _maxChannels) ? _count[ch] : 0;
    }

    uint8_t getMaxChannels() {
        return _maxChannels;
    }

    // Обработка прямых команд из сценариев IoTmanager
    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (command == "push" && param.size() >= 2) {
            push((uint8_t)param[0].valD, (uint16_t)param[1].valD);
        }
        else if (command == "save") {
            doByInterval();
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