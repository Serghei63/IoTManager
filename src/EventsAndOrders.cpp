#include "EventsAndOrders.h"
#include "classes/IoTScenario.h"

extern IoTScenario iotScen;  // объект управления сценарием

// Очередь команд (orders) - прямые команды, которые нужно выполнить сразу, нажали на кнопку в приложении
// необходимо тут же изменить параметр, записать новое значение

// Очередь событий (events) - в ней как на конвеере едут события, и каждое событие проверяется через все условия сценариев
// ее главное отличие от очереди команд в том что события в ней не для выполнения, а для проверки в условиях сценариев
// события в ней лишь вызывают другие команды, помещенные в блоке сценария, если условие совпало.

// Обе очереди - кольцевые буферы фиксированного размера, работают только из контекста loop()
// (MQTT - PubSubClient, WS - WebSocketsServer, модули - из своих loop), поэтому без блокировок.

#define EVENT_FIFO_SIZE 64      // должно быть степенью двойки
#define ORDER_FIFO_SIZE 16      // должно быть степенью двойки
#define EVENTS_PER_LOOP 4       // сколько событий обрабатываем за один вызов handleEvent()

template <typename T, size_t CAPACITY>
class IoTFifo {
    static_assert((CAPACITY & (CAPACITY - 1)) == 0, "IoTFifo capacity must be power of two");
    static const size_t MASK = CAPACITY - 1;

    T _buf[CAPACITY];
    size_t _head = 0;
    size_t _tail = 0;

   public:
    // при переполнении элемент не добавляется и возвращается false
    bool push(const T& item) {
        size_t next = (_tail + 1) & MASK;
        if (next == _head) return false;
        _buf[_tail] = item;
        _tail = next;
        return true;
    }

    bool pop(T& item) {
        if (_head == _tail) return false;
        item = _buf[_head];
        _buf[_head] = T();  // освобождаем возможные ресурсы элемента (String)
        _head = (_head + 1) & MASK;
        return true;
    }

    bool isEmpty() const { return _head == _tail; }
};

struct IoTOrder {
    uint32_t idHash = 0;
    String value;
};

static IoTFifo<IoTOrder, ORDER_FIFO_SIZE> orderFifo;
static IoTFifo<uint32_t, EVENT_FIFO_SIZE> eventFifo;

static void executeOrder(uint32_t idHash, const String& value) {
    IoTItem* item = findIoTItem(idHash);
    if (item) {
        SerialPrint("i", F("ORDER"), "order found " + item->getID() + " " + value);
        item->setValue(value);
    }
}

//эта функция будет вызываться:
// 1.на приеме сообщений в mqtt (см mqttCallback в mqttClient.cpp)
// 2.на приеме сообщений в ws
// 3.в модулях, получающих команды извне (Telegram, Nextion, Uart и т.п.)
void generateOrder(const String& id, const String& value) {
    if (id == "") return;
    IoTOrder order;
    order.idHash = hashFNV1a(id);
    order.value = value;
    if (!orderFifo.push(order)) {
        // очередь переполнена - не теряем команду, выполняем сразу
        SerialPrint("E", F("ORDER"), "order queue full, execute now " + id);
        executeOrder(order.idHash, value);
    }
}

//эта функция будет вызываться в loop()
void handleOrder() {
    IoTOrder order;
    if (orderFifo.pop(order)) {
        executeOrder(order.idHash, order.value);
    }
}

//эта функция будет вызываться:
// 1.в местах где событие только что произошло (IoTItem::regEvent)
void generateEvent(const String& id, const String& value) {
    if (id == "") return;
    generateEvent(hashFNV1a(id), value);
}

// значение события не хранится в очереди, при обработке берется актуальное значение элемента
void generateEvent(uint32_t idHash, const String& value) {
    if (idHash == FNV1A_EMPTY_HASH) return;
    if (!eventFifo.push(idHash)) {
        IoTItem* item = findIoTItem(idHash);
        SerialPrint("E", F("EVENT"), "event queue full, event lost " + (item ? item->getID() : String(idHash)));
    }
}

//эта функция будет вызываться в loop()
void handleEvent() {
    uint32_t idHash;
    for (int i = 0; i < EVENTS_PER_LOOP && eventFifo.pop(idHash); i++) {
        IoTItem* eventIoTItem = findIoTItem(idHash);
        if (!eventIoTItem) continue;  // элемент мог быть удален до обработки события (сетевой или после переконфигурирования)

        SerialPrint("i", F("EVENT"), eventIoTItem->getID() + " " + eventIoTItem->getValue());

        // распространяем событие через хуки
        for (std::list<IoTItem*>::iterator it = IoTItems.begin(); it != IoTItems.end(); ++it) {
            (*it)->onRegEvent(eventIoTItem);  // прямой хук

            // вызов хука при условии отслеживания изменения
            if ((*it)->isTracking(eventIoTItem)) (*it)->onTrackingValue(eventIoTItem);
        }

        //здесь нужно пропускать данное событие через условия сценариев
        //и если оно есть в условии сценария и совпадает
        iotScen.exec(idHash);
    }
}
