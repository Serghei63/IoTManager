#include "Global.h"
#include "classes/IoTItem.h"
#include "FS.h"
#include "SdFat.h"                // Работа через библиотеку SdFat

// Инициализируем глобальный объект sd, доступный во всем проекте
SdFat sd; 

class SDcard;
extern SDcard* globalSDCardInstance;

class SDcard : public IoTItem {
   private:
    bool _isReady = false;

   public:
    SDcard(String parameters): IoTItem(parameters) {
        Serial.println("[SDcard] Starting SPI SD Card with custom pins via SdFat...");
        
        // Настройка аппаратных пинов вашей платы
        int chipSelectPin = 5; 
        int mosiPin = 23; 
        int misoPin = 19;
        int sclkPin = 18;
        
        // 1. Инициализируем аппаратный интерфейс SPI на заданных GPIO
        SPI.begin(sclkPin, misoPin, mosiPin, chipSelectPin);
        
        // 2. Монтируем карту памяти на частоте 16 МГц
        if(!sd.begin(chipSelectPin, SD_SCK_MHZ(16))){
            Serial.println("[SDcard] ERROR: SdFat SPI Card Mount Failed!");
            _isReady = false;
            return;
        }

        _isReady = true;
        globalSDCardInstance = this;

        // Вывод информации о карте в консоль при старте
        uint32_t sectors = sd.card()->sectorCount();
        uint64_t cardSize = ((uint64_t)sectors * 512) / (1024 * 1024);
        Serial.printf("[SDcard] Init completed. Card size: %llu MB\n", cardSize);

        // Гарантируем наличие базовых папок для логов
        if (!sd.exists("/lg")) {
            sd.mkdir("/lg");
        }
        if (!sd.exists("/logs")) {
            sd.mkdir("/logs");
        }
    }

    // Метод проверки готовности карты для внешних модулей (например, логера)
    bool isReady() const { return _isReady; }

    void doByInterval() {}

    IoTValue execute(String command, std::vector<IoTValue> &param) {
        return {};  
    }

    ~SDcard() {
        if (globalSDCardInstance == this) {
            globalSDCardInstance = nullptr;
        }
    };
};

// Выделение памяти под глобальный указатель
SDcard* globalSDCardInstance = nullptr;

void* getAPI_SDcard(String subtype, String param) {
    if (subtype == F("SDcard")) {
        return new SDcard(param);
    } 
    return nullptr;
}