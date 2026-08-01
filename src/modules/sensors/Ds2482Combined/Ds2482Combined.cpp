#include "Global.h"
#include "classes/IoTItem.h"

#include <Wire.h>
#include <DallasTemperature.h>
#include <DS2482.h>
#include <DS18B20_DS2482.h>
#include <map>
#include <vector>

typedef uint8_t DeviceAddressDS2482[8];

struct Ds2482DeviceCache {
    float temperature = -127.0;
    uint32_t counterA = 0;
    uint32_t counterB = 0;
    bool isCounter = false;
    bool isFoundOnBus = false;
};

std::map<String, Ds2482DeviceCache> ds2482GlobalRegistry;
std::vector<String> discoveredTemperatures;
std::vector<String> discoveredCounters;

static DS2482* globalDS2482 = nullptr;
static DS18B20_DS2482* globalDS18B20 = nullptr;
static bool isDs2482MasterReady = false;

void scanOneWireBus() {
    if (!globalDS2482) return;

    SerialPrint("I", "DS2482 Scanner", "Запуск сканирования шины 1-Wire...");
    discoveredTemperatures.clear();
    discoveredCounters.clear();

    globalDS2482->wireResetSearch();
    DeviceAddress searchAddress;
    int totalDevices = 0;

    while (globalDS2482->wireSearch(searchAddress)) {
        totalDevices++;
        char addrStr[20] = "";
        hex2string(searchAddress, 8, addrStr);
        String hexAddress = String(addrStr);
        hexAddress.toUpperCase();

        uint8_t familyCode = searchAddress[0];
        String deviceType = "Unknown";

        if (familyCode == 0x28) {
            deviceType = "DS18B20 (Temperature)";
            discoveredTemperatures.push_back(hexAddress);
            if (ds2482GlobalRegistry.find(hexAddress) == ds2482GlobalRegistry.end()) {
                ds2482GlobalRegistry[hexAddress].isCounter = false;
            }
            ds2482GlobalRegistry[hexAddress].isFoundOnBus = true;
        } 
        else if (familyCode == 0x1D) {
            deviceType = "DS2423 (Counter)";
            discoveredCounters.push_back(hexAddress);
            if (ds2482GlobalRegistry.find(hexAddress) == ds2482GlobalRegistry.end()) {
                ds2482GlobalRegistry[hexAddress].isCounter = true;
            }
            ds2482GlobalRegistry[hexAddress].isFoundOnBus = true;
        }

        SerialPrint("I", "DS2482 Scanner", "Найдено устройство #" + String(totalDevices) + " [" + deviceType + "]: " + hexAddress);
    }
    SerialPrint("I", "DS2482 Scanner", "Сканирование завершено. Всего устройств: " + String(totalDevices));
}

void initDs2482HardwareMaster() {
    if (!isDs2482MasterReady) {
        Wire.begin();
        Wire.setClock(100000);
        
        Wire.beginTransmission(0x18);
        if (Wire.endTransmission() == 0) {
            globalDS2482 = new DS2482(0);
            globalDS2482->reset();
            
            globalDS18B20 = new DS18B20_DS2482(globalDS2482);
            globalDS18B20->begin();
            
            isDs2482MasterReady = true;
            SerialPrint("I", "DS2482 Combined", "Аппаратный I2C 1-Wire Мастер успешно инициализирован.");
            scanOneWireBus();
        } else {
            SerialPrint("E", "DS2482 Combined", "Критическая ошибка: Чип DS2482 не найден на I2C адресе 0x18!");
            globalDS2482 = nullptr;
            globalDS18B20 = nullptr;
            isDs2482MasterReady = true;
        }
    }
}

// ========================================================================
// 1. МАСТЕР-МОДУЛЬ ШИНЫ (Hub)
// ========================================================================
class Ds2482Hub : public IoTItem {
public:
    Ds2482Hub(String parameters) : IoTItem(parameters) {
        initDs2482HardwareMaster();
    }

    void doByInterval() {
        if (!globalDS2482) return;

        // Сбрасываем флаг присутствия (классический итератор C++11)
        for (auto it = ds2482GlobalRegistry.begin(); it != ds2482GlobalRegistry.end(); ++it) {
            it->second.isFoundOnBus = false;
        }

        // --- Шаг 1: Опрос датчиков температуры DS18B20 ---
        if (globalDS18B20) {
            globalDS18B20->requestTemperatures();
            
            for (auto it = ds2482GlobalRegistry.begin(); it != ds2482GlobalRegistry.end(); ++it) {
                if (!it->second.isCounter) { // Только градусники
                    String hexAddr = it->first;
                    DeviceAddress binAddr;
                    string2hex(hexAddr.c_str(), binAddr);
                    
                    // Проверяем физический сброс шины. Если wireReset() вернул 1 (или true),
                    // значит на шине есть хоть какое-то устройство.
                    if (globalDS2482->wireReset()) {
                        globalDS2482->wireSelect(binAddr); // Просто вызываем без присвоения (void)
                        
                        float tempC = globalDS18B20->getTempC(binAddr);
                        if (tempC != DEVICE_DISCONNECTED_C && tempC != 85.0) {
                            it->second.temperature = tempC;
                            it->second.isFoundOnBus = true; // Отмечаем, что данные считаны успешно
                        }
                    }
                }
            }
        }

        // --- Шаг 2: Низкоуровневый опрос счетчиков импульсов DS2423 ---
        for (auto it = ds2482GlobalRegistry.begin(); it != ds2482GlobalRegistry.end(); ++it) {
            if (it->second.isCounter) { // Только счетчики
                String hexAddr = it->first;
                DeviceAddress binAddr;
                string2hex(hexAddr.c_str(), binAddr);

                // Делаем сброс шины перед сессией работы со счетчиком
                if (!globalDS2482->wireReset()) {
                    continue; // Шина пуста, пропускаем чип
                }

                uint32_t channelValues[2] = {0, 0};
                bool readSuccess = true;

                for (int q = 0; q < 2; q++) {
                    uint8_t buffer[45] = {0};
                    
                    globalDS2482->wireReset();
                    globalDS2482->wireSelect(binAddr);
                    globalDS2482->wireWriteByte(0xA5); 
                    
                    if (q == 0) globalDS2482->wireWriteByte(0xC0); 
                    if (q == 1) globalDS2482->wireWriteByte(0xE0); 
                    
                    globalDS2482->wireWriteByte(0x01);

                    for (int j = 3; j < 45; j++) {
                        buffer[j] = globalDS2482->wireReadByte();
                    }
                    globalDS2482->wireReset();

                    // Защита от фантомных чтений: если все критические байты равны 0xFF, чип не ответил
                    if (buffer[35] == 0xFF && buffer[36] == 0xFF && buffer[37] == 0xFF && buffer[38] == 0xFF) {
                        readSuccess = false;
                        break;
                    }

                    uint32_t count = (uint32_t)buffer[38];
                    for (int j = 37; j >= 35; j--) {
                        count = (count << 8) + (uint32_t)buffer[j];
                    }
                    channelValues[q] = count;
                }

                if (readSuccess) {
                    it->second.counterA = channelValues[0];
                    it->second.counterB = channelValues[1];
                    it->second.isFoundOnBus = true;
                }
            }
        }
    }

    ~Ds2482Hub() {}
};

// ========================================================================
// 2. ДОЧЕРНИЙ МОДУЛЬ: Градусник DS18B20
// ========================================================================
class Ds2482Temp : public IoTItem {
private:
    String _addr = "";
    int _index = -1;
public:
    Ds2482Temp(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "addr", _addr);
        jsonRead(parameters, "index", _index, false);
        _addr.toUpperCase();
    }

    void doByInterval() {
        if (_addr == "" && _index >= 0) {
            if (_index < discoveredTemperatures.size()) {
                _addr = discoveredTemperatures[_index];
                ds2482GlobalRegistry[_addr].isCounter = false;
                SerialPrint("I", "Ds2482Temp", "Датчик " + _id + " привязан по индексу [" + String(_index) + "] к адресу: " + _addr);
            }
        }

        if (_addr != "" && ds2482GlobalRegistry.find(_addr) != ds2482GlobalRegistry.end()) {
            if (ds2482GlobalRegistry[_addr].isFoundOnBus) {
                float cachedTemp = ds2482GlobalRegistry[_addr].temperature;
                if (cachedTemp != -127.0) {
                    value.valD = cachedTemp;
                    regEvent(value.valD, "Ds2482Temp");
                }
            }
        }
    }
    ~Ds2482Temp() {}
};

// ========================================================================
// 3. ДОЧЕРНИЙ МОДУЛЬ: Счетчик DS2423
// ========================================================================
class Ds2482Counter : public IoTItem {
private:
    String _addr = "";
    String _channel = "A";
    int _index = -1;
public:
    Ds2482Counter(String parameters) : IoTItem(parameters) {
        jsonRead(parameters, "addr", _addr);
        jsonRead(parameters, "chan", _channel);
        jsonRead(parameters, "index", _index, false);
        _addr.toUpperCase();
        _channel.toUpperCase();
    }

    void doByInterval() {
        if (_addr == "" && _index >= 0) {
            if (_index < discoveredCounters.size()) {
                _addr = discoveredCounters[_index];
                ds2482GlobalRegistry[_addr].isCounter = true;
                SerialPrint("I", "Ds2482Counter", "Счетчик " + _id + " привязан по индексу [" + String(_index) + "] к адресу: " + _addr);
            }
        }

        if (_addr != "" && ds2482GlobalRegistry.find(_addr) != ds2482GlobalRegistry.end()) {
            if (ds2482GlobalRegistry[_addr].isFoundOnBus) {
                uint32_t rawValue = 0;
                if (_channel == "A") rawValue = ds2482GlobalRegistry[_addr].counterA;
                else if (_channel == "B") rawValue = ds2482GlobalRegistry[_addr].counterB;

               // value.valD = (double)rawValue / 430.0;
                 value.valD = (double)rawValue;  
                regEvent(value.valD, "Ds2482Counter");
            }
        }
    }
    ~Ds2482Counter() {}
};

void* getAPI_Ds2482Combined(String subtype, String param) {
    if (subtype == F("Ds2482Hub"))     return new Ds2482Hub(param);
    if (subtype == F("Ds2482Temp"))    return new Ds2482Temp(param);
    if (subtype == F("Ds2482Counter")) return new Ds2482Counter(param);
    return nullptr;
}