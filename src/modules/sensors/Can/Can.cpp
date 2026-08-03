#include "Global.h"
#include "classes/IoTItem.h"
#include <map>

#include <SPI.h>
#include <CAN.h> // Библиотека от Sandeep Mistry

extern IoTGpio IoTgpio;
bool _debug;

// Флаг для того, чтобы инициализировать CAN-интерфейс только один раз для обоих классов
static bool isCANInitialized = false;

void initCANOnce(long bitrate) {
    if (!isCANInitialized) {
        // Устанавливаем SPI CS пин (в вашем случае 5)
        // Если прерывание не используется, передаем -1 (или пин прерывания, если он есть)
        CAN.setPins(5, -1);
        
       // CAN.setPins(rx, tx);
        
        // Запускаем CAN на выбранной скорости (500 Кбит/с = 500E3)
        if (!CAN.begin(bitrate)) {
            Serial.println("Starting CAN failed!");
        } else {
            isCANInitialized = true;
            if (_debug) Serial.println("CAN Initialized successfully.");
        }
    }
}

class Canm : public IoTItem
{
private:
    String _regStr = "";
    String _regAsk = "";
    String _dlcStr = "";
    uint16_t _reg = 0;
    uint16_t _ask = 0;
    uint8_t _addr = 0;    // Индекс байта в массиве данных
    uint8_t _dlc = 0;     // Data Length Code

public:
    Canm(String parameters) : IoTItem(parameters)
    {
        _addr = jsonReadInt(parameters, "addr"); 
        jsonRead(parameters, "can_id", _regStr);
        _reg = hexStringToUint16(_regStr);
        jsonRead(parameters, "ask_id", _regAsk);
        _ask = hexStringToUint16(_regAsk);
        _dlc = jsonReadInt(parameters, "dlc", 8);
        jsonRead(parameters, "debug", _debug);

        // Инициализируем аппаратную часть один раз (500E3 = 500Кб/с)
        initCANOnce(500E3);
    }

    void doByInterval()
    {
        // Проверяем размер пришедшего пакета
        int packetSize = CAN.parsePacket();
        
        if (packetSize > 0) {
            // Проверяем, не RTR ли это пакет и совпадает ли ID
            if (!CAN.packetRtr() && CAN.packetId() == _reg) {
                
                uint8_t dataBuffer[8] = {0};
                int i = 0;
                
                // Считываем доступные байты в буфер
                while (CAN.available() && i < 8) {
                    dataBuffer[i] = CAN.read();
                    i++;
                }

                // Забираем значение по нужному смещению (адресу)
                int tempInt = dataBuffer[_addr];
                value.valD = tempInt;

                if (_debug) {
                    Serial.print("Data received: ");
                    Serial.println(tempInt);
                }       

                regEvent(value.valD, "CanBUS");

                // --- Отправка подтверждения (ACK) ---
                // Открываем стандартный пакет (не расширенный) с ID = _ask
                if (CAN.beginPacket(_ask)) {
                    // Для пустого ACK пакета ничего не пишем в буфер (dlc будет 0)
                    CAN.endPacket();
                    
                    if (_debug) {
                        Serial.println("ACK sent");
                    }
                }
            }
        }
    }

    ~Canm() {}
};

class Cans : public IoTItem
{
private:
    String _regStr1 = "";
    uint16_t _reg1 = 0;
    uint8_t _adr = 0;
    uint8_t _dlc = 0;

public:
    Cans(String parameters) : IoTItem(parameters)
    {
        _adr = jsonReadInt(parameters, "adr"); 
        _dlc = jsonReadInt(parameters, "dlc");
        jsonRead(parameters, "cans_id", _regStr1);
        _reg1 = hexStringToUint16(_regStr1);
        jsonRead(parameters, "debug", _debug);

        initCANOnce(500E3);
    }

    // Команды из сценария
    IoTValue execute(String command, std::vector<IoTValue> &param)
    {
        if (command == "send") 
        {
            if (param.size() >= 3) 
            {
                uint16_t targetReg = hexStringToUint16(param[0].valS);
                uint8_t dlc = param[1].valD;
                uint16_t data = param[2].valD;

                // Начинаем пакет с нужным CAN ID
                if (CAN.beginPacket(targetReg)) {
                    
                    // Записываем байт данных. 
                    // Если вам нужно передавать больше байт в зависимости от dlc, 
                    // здесь можно сделать цикл или побайтовую запись.
                    CAN.write(data); 
                    
                    // Завершаем и отправляем пакет в шину
                    bool ok = CAN.endPacket();

                    if (_debug) {
                        SerialPrint("I", "Can", "send, registr: " + String(targetReg, HEX) + ", data: " + String(data) + ", status: " + String(ok));
                    }
                }
            }
            return {};
        }
        return {};
    }

    ~Cans() {}
};

void *getAPI_Can(String subtype, String param)
{
    if (subtype == F("canMaster")) {
        return new Canm(param);
    } else if (subtype == F("canSlave")) {
        return new Cans(param);
    } else {
        return nullptr;
    }
}

