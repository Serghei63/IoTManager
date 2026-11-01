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

   // Метод для чтения текстового файла в String
    String readFileContent(const char* path) {
        if (!_isReady) return "";
        FsFile file;
        if (!file.open(path, O_READ)) {
            Serial.printf("[SDcard] File not found on SD: %s\n", path);
            return "";
        }
        String content = "";
        while (file.available()) {
            content += (char)file.read();
        }
        file.close();
        return content;
    }

    // Метод проверки и переноса файла с SD на внутреннюю LittleFS
    bool importFileToLittleFS(const char* filename) {
        String content = readFileContent(filename);
        if (content.length() == 0) return false;

        File f = LittleFS.open(filename, "w");
        if (!f) {
            Serial.printf("[SDcard] Failed to open LittleFS file for writing: %s\n", filename);
            return false;
        }
        f.print(content);
        f.close();
        Serial.printf("[SDcard] Successfully imported %s from SD to LittleFS\n", filename);
        return true;
    }

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

        // Проверяем наличие файла сценария на SD и переносим во внутреннюю память
        if (sd.exists("/scenario.txt")) {
            if (importFileToLittleFS("/scenario.txt")) {
                Serial.println("[SDcard] Scenario updated from SD card!");
            }
        }
        
        // Аналогично можно сделать для config.json, если это требуется
        if (sd.exists("/config.json")) {
            if (importFileToLittleFS("/config.json")) {
                Serial.println("[SDcard] Config updated from SD card!");
                // При необходимости можно сразу вызвать перезагрузку: 
                ESP.restart();
            }
        }

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