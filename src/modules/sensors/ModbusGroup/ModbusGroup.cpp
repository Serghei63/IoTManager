#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <vector>
#include <HardwareSerial.h>

#include "Logging.h"
#include "ModbusClientRTU.h"
#include "CoilData.h"

// -------------------------------------------------------------
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МОДУЛЯ
// -------------------------------------------------------------
static Stream *_modbusUART = nullptr;
static bool _modbusDebug = false;
static ModbusClientRTU *MB = nullptr;
static uint32_t modBus_Token_count = 0;

// Указатель на экземпляр клиента для отправки ивентов ошибок
static IoTItem *_mbClientInstance = nullptr;

#ifndef MODBUS_DIR_PIN_DEF
#define MODBUS_DIR_PIN_DEF 4
#endif

#ifndef MODBUS_UART_LINE_DEF
#define MODBUS_UART_LINE_DEF 2
#endif

static int8_t MODBUS_DIR_PIN = MODBUS_DIR_PIN_DEF;
static int8_t MODBUS_UART_LINE = MODBUS_UART_LINE_DEF;

class ModbusGroupPoll;
class ModbusGroupSens;

static std::map<uint32_t, ModbusGroupPoll*> MBGroupTokenMap;
static std::map<String, ModbusGroupPoll*> GroupPollersMap;

// Прототипы глобальных обработчиков eModbus
void handleModBusGroupData(ModbusMessage response, uint32_t token);
void handleModBusGroupError(Error error, uint32_t token);

static void instanceModBus(int8_t dirPin) {
    if (MB == nullptr) {
        MB = new ModbusClientRTU(dirPin);
    }
}

// -------------------------------------------------------------
// 1. КЛАСС КЛИЕНТА (Шина RS-485 и обработка вызовов из скриптов)
// -------------------------------------------------------------
class ModbusClientAsync : public IoTItem {
private:
    int8_t _rx = 18;
    int8_t _tx = 19;
    int _baud = 9600;
    String _prot = "SERIAL_8N1";
    int protocol = SERIAL_8N1;
    bool _debug = false;

public:
    ModbusClientAsync(String parameters) : IoTItem(parameters) {
        _mbClientInstance = this; // Сохраняем клиент для отправки событий ошибок

        _rx = (int8_t)jsonReadInt(parameters, "RX");
        _tx = (int8_t)jsonReadInt(parameters, "TX");
        MODBUS_DIR_PIN = (int8_t)jsonReadInt(parameters, "DIR_PIN");
        _baud = jsonReadInt(parameters, "baud");
        _prot = jsonReadStr(parameters, "protocol");
        jsonRead(parameters, "debug", _debug);

        _modbusDebug = _debug;

        if (_prot == "SERIAL_8N1") protocol = SERIAL_8N1;
        else if (_prot == "SERIAL_8N2") protocol = SERIAL_8N2;

        pinMode(MODBUS_DIR_PIN, OUTPUT);
        digitalWrite(MODBUS_DIR_PIN, LOW);

        instanceModBus(MODBUS_DIR_PIN);
        
        if (_modbusUART != nullptr) {
            ((HardwareSerial *)_modbusUART)->end();
        } else {
            _modbusUART = new HardwareSerial(MODBUS_UART_LINE);
        }

        if (_debug) {
            SerialPrint("I", "ModbusClientAsync", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
        }

        RTUutils::prepareHardwareSerial((HardwareSerial &)*_modbusUART);
        ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx);
        ((HardwareSerial *)_modbusUART)->setTimeout(200);

        MB->onDataHandler(&handleModBusGroupData);
        MB->onErrorHandler(&handleModBusGroupError);
        MB->setTimeout(2000);
        MB->begin((HardwareSerial &)*_modbusUART);
    }

    void setValue(const String& valStr, bool genEvent = true) override {
        String val = valStr;
        val.trim();

        if (_debug) {
            SerialPrint("I", "ModbusClientAsync", "setValue: " + val);
        }

        int openBracket = val.indexOf('(');
        int closeBracket = val.lastIndexOf(')');

        if (openBracket != -1 && closeBracket != -1 && closeBracket > openBracket) {
            String cmd = val.substring(0, openBracket);
            cmd.trim();

            String paramsStr = val.substring(openBracket + 1, closeBracket);
            std::vector<IoTValue> params;

            int start = 0;
            bool inQuotes = false;
            char quoteChar = 0;

            for (size_t i = 0; i <= paramsStr.length(); i++) {
                char c = (i < paramsStr.length()) ? paramsStr[i] : ',';

                if (c == '\'' || c == '"') {
                    if (!inQuotes) {
                        inQuotes = true;
                        quoteChar = c;
                    } else if (c == quoteChar) {
                        inQuotes = false;
                    }
                }

                if (c == ',' && !inQuotes) {
                    String p = paramsStr.substring(start, i);
                    p.trim();

                    if ((p.startsWith("\"") && p.endsWith("\"")) || (p.startsWith("'") && p.endsWith("'"))) {
                        p = p.substring(1, p.length() - 1);
                    }

                    if (p.length() > 0) {
                        IoTValue paramVal;
                        paramVal.valS = p;

                        if (p.startsWith("0x") || p.startsWith("0X")) {
                            paramVal.isDecimal = false;
                            paramVal.valD = (float)hexStringToUint16(p);
                        } else if (p == "true" || p == "FALSE" || p == "TRUE" || p == "false") {
                            paramVal.isDecimal = true;
                            paramVal.valD = (p == "true" || p == "TRUE") ? 1.0f : 0.0f;
                        } else {
                            paramVal.isDecimal = true;
                            paramVal.valD = p.toFloat();
                        }

                        params.push_back(paramVal);
                    }
                    start = i + 1;
                }
            }

            execute(cmd, params);
        } else {
            IoTItem::setValue(valStr, genEvent);
        }
    }

    IoTValue execute(String command, std::vector<IoTValue> &param) override {
        if (!MB) return {};

        auto parseRegister = [](const IoTValue& item) -> uint16_t {
            if (item.isDecimal) {
                return (uint16_t)item.valD;
            }
            String s = item.valS;
            s.trim();
            if (s.startsWith("0x") || s.startsWith("0X")) {
                return hexStringToUint16(s);
            }
            return (uint16_t)s.toInt();
        };

        auto parseAddr = [](const IoTValue& item) -> uint8_t {
            return item.isDecimal ? (uint8_t)item.valD : (uint8_t)item.valS.toInt();
        };

        if (command == "writeSingleCoil" && param.size() >= 3) {
            uint8_t addr = parseAddr(param[0]);
            uint16_t reg = parseRegister(param[1]);
            bool state = param[2].isDecimal ? (param[2].valD != 0) : (param[2].valS == "1" || param[2].valS == "true");

            modBus_Token_count++;
            uint32_t token = modBus_Token_count;

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", "writeSingleCoil, addr: 0x" + String(addr, HEX) + ", reg: 0x" + String(reg, HEX) + ", state: " + String(state));
            }

            Error err;
            ModbusMessage msg;
            if (state) {
                msg.setMessage(addr, WRITE_COIL, reg, (uint16_t)0xFF00);
                err = MB->addRequest(msg, token);
            } else {
                err = MB->addRequest(token, addr, WRITE_COIL, reg, (uint16_t)0x0000);
            }

            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Error writeSingleCoil: " + String((int)e, HEX));
            }
            return {};
        }
        
        else if (command == "writeSingleRegister" && param.size() >= 3) {
            uint8_t addr = parseAddr(param[0]);
            uint16_t reg = parseRegister(param[1]);
            
            // Приводим к int16_t, а затем к uint16_t (для сохранения знакового бита)
            int32_t valInt = param[2].isDecimal ? (int32_t)param[2].valD : param[2].valS.toInt();
            uint16_t val = (uint16_t)(int16_t)valInt;

            modBus_Token_count++;
            uint32_t token = modBus_Token_count;

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", "writeSingleRegister, addr: 0x" + String(addr, HEX) + ", reg: 0x" + String(reg, HEX) + ", val: " + String(valInt) + " (0x" + String(val, HEX) + ")");
            }

            Error err = MB->addRequest(token, addr, WRITE_HOLD_REGISTER, reg, val);
            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Error writeSingleRegister: " + String((int)e, HEX));
            }
            return {};
        }        

        else if (command == "writeMultipleCoils" && param.size() >= 4) {
            uint8_t addr = parseAddr(param[0]);
            uint16_t reg = parseRegister(param[1]);
            uint16_t count = param[2].isDecimal ? (uint16_t)param[2].valD : (uint16_t)param[2].valS.toInt();
            uint16_t val = param[3].isDecimal ? (uint16_t)param[3].valD : (uint16_t)param[3].valS.toInt();

            modBus_Token_count++;
            uint32_t token = modBus_Token_count;

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", "writeMultipleCoils, addr: 0x" + String(addr, HEX) + ", reg: 0x" + String(reg, HEX) + ", count: " + String(count));
            }

            uint8_t numBytes = (count + 7) / 8;
            std::vector<uint8_t> coilBytes(numBytes, 0);
            for (uint16_t i = 0; i < count; i++) {
                if (val & (1 << i)) {
                    coilBytes[i / 8] |= (1 << (i % 8));
                }
            }

            Error err = MB->addRequest(token, addr, WRITE_MULT_COILS, reg, count, numBytes, coilBytes.data());
            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Error writeMultipleCoils: " + String((int)e, HEX));
            }
            return {};
        }

else if (command == "writeMultipleRegisters" && param.size() >= 3) {
            uint8_t addr = parseAddr(param[0]);
            uint16_t reg = parseRegister(param[1]);

            std::vector<uint16_t> wData;

            // Если передано 1 число и оно дробное (float, например -1.5)
            if (param.size() == 3 && param[2].isDecimal && (param[2].valD != (float)(int32_t)param[2].valD)) {
                union {
                    float f;
                    uint16_t w[2];
                } u;
                u.f = param[2].valD; // Float32 сохраняет знак автоматически

                wData.push_back(u.w[1]); // High Word
                wData.push_back(u.w[0]); // Low Word
            } 
            else {
                // Массив целых чисел (поддержка отрицательных int16_t)
                uint16_t numRegs = param.size() - 2;
                wData.resize(numRegs);

                for (size_t i = 0; i < numRegs; i++) {
                    if (param[i + 2].isDecimal) {
                        wData[i] = (uint16_t)(int16_t)param[i + 2].valD;
                    } else {
                        String s = param[i + 2].valS;
                        s.trim();
                        if (s.startsWith("0x") || s.startsWith("0X")) {
                            wData[i] = hexStringToUint16(s);
                        } else {
                            wData[i] = (uint16_t)(int16_t)s.toInt();
                        }
                    }
                }
            }

            uint16_t numRegs = wData.size();
            String logMsg = "writeMultipleRegisters, addr: 0x" + String(addr, HEX) + 
                            ", reg: 0x" + String(reg, HEX) + 
                            ", count: " + String(numRegs) + " -> ";

            for (size_t i = 0; i < numRegs; i++) {
                if (_debug) {
                    logMsg += "val" + String(i + 1) + ": (0x" + String(wData[i], HEX) + ") ";
                }
            }

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", logMsg);
            }

            ModbusMessage msg;
            msg.setMessage(addr, WRITE_MULT_REGISTERS, reg, numRegs, (uint8_t)(numRegs * 2), wData.data());

            Error err = MB->addRequest(msg, (uint32_t)0);
            
            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Ошибка 0x10: " + String((int)e, HEX) + " - " + String((const char *)e));
            }

            return {};
        }

        else if (command == "resetEnergy" && param.size() >= 1) {
            uint8_t addr = parseAddr(param[0]);

            modBus_Token_count++;
            uint32_t token = modBus_Token_count;

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", "resetEnergy (0x42), addr: 0x" + String(addr, HEX));
            }

            Error err = MB->addRequest(token, addr, (uint8_t)0x42, 0, 0);
            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Error resetEnergy: " + String((int)e, HEX));
            }
            return {};
        }

        return {};
    }

    void doByInterval() override {}
    ~ModbusClientAsync() {
        if (_mbClientInstance == this) {
            _mbClientInstance = nullptr;
        }
    }
};

// -------------------------------------------------------------
// 2. КЛАСС РОДИТЕЛЯ (Поллер)
// -------------------------------------------------------------
class ModbusGroupPoll : public IoTItem {
private:
    uint8_t _addr = 1;
    uint16_t _reg = 0;
    uint8_t _countReg = 1;
    uint8_t _func = 0x03;
    
    uint32_t _lastToken = 0;
    bool _waitingResponse = false;

    std::vector<ModbusGroupSens*> _children;

public:
    ModbusGroupPoll(String parameters) : IoTItem(parameters) {
        _addr = jsonReadInt(parameters, "addr");
        _countReg = jsonReadInt(parameters, "count");
        
        String funcStr = jsonReadStr(parameters, "func");
        if (funcStr.length() > 0) {
            _func = hexStringToUint8(funcStr);
        }

        String regStr = jsonReadStr(parameters, "reg");
        if (regStr.startsWith("0x") || regStr.startsWith("0X")) {
            _reg = hexStringToUint16(regStr);
        } else {
            _reg = regStr.toInt();
        }

        if (_modbusDebug) {
            SerialPrint("I", "ModbusGroupPoll", "Создан поллер id:" + getID() + ", func:0x" + String(_func, HEX) + ", addr:" + String(_addr) + ", count:" + String(_countReg));
        }
    }

    uint8_t getFunc() const { return _func; }
    void addChild(ModbusGroupSens* child) { _children.push_back(child); }

    void doByInterval() override {
        if (!MB) return;

        if (_waitingResponse) {
            if (_modbusDebug) {
                SerialPrint("E", "ModbusGroupPoll", "Таймаут шины! Ответ не получен на token:" + String(_lastToken));
            }
            handleModBusGroupError(TIMEOUT, _lastToken);
        }

        modBus_Token_count++;
        _lastToken = modBus_Token_count;
        _waitingResponse = true;

        MBGroupTokenMap[_lastToken] = this;

        if (_modbusDebug) {
            SerialPrint("I", "ModbusGroupPoll", "Групповой опрос token:" + String(_lastToken) + " (" + getID() + ")");
        }

        Error err = SUCCESS;
        switch (_func) {
            case 0x01:
                err = MB->addRequest(_lastToken, _addr, READ_COIL, _reg, _countReg);
                break;
            case 0x02:
                err = MB->addRequest(_lastToken, _addr, READ_DISCR_INPUT, _reg, _countReg);
                break;
            case 0x04:
                err = MB->addRequest(_lastToken, _addr, READ_INPUT_REGISTER, _reg, _countReg);
                break;
            case 0x03:
            default:
                err = MB->addRequest(_lastToken, _addr, READ_HOLD_REGISTER, _reg, _countReg);
                break;
        }

        if (err != SUCCESS) {
            _waitingResponse = false;
            ModbusError e(err);
            SerialPrint("E", "ModbusGroupPoll", "Ошибка добавления в очередь token " + String(_lastToken) + ": " + String((int)e, HEX));
            handleModBusGroupError(err, _lastToken);
        }
    }

    void parseMB(ModbusMessage response);
    void resetWaitingFlag() { _waitingResponse = false; }

    ~ModbusGroupPoll() {}
};

// -------------------------------------------------------------
// 3. КЛАСС ДОЧЕРНЕГО СЕНСОРА
// -------------------------------------------------------------
class ModbusGroupSens : public IoTItem {
private:
    uint8_t _offset = 0;
    uint8_t _count = 1;
    bool _isFloat = false;
    float _div = 1.0f;
    
    bool _onlyOnChange = false;
    float _lastVal = -999999.0f;

public:
    ModbusGroupSens(String parameters) : IoTItem(parameters) {
        _offset = jsonReadInt(parameters, "offset");
        _isFloat = jsonReadBool(parameters, "isFloat");

        jsonRead(parameters, "div", _div);
        if (_div == 0.0f) _div = 1.0f;
        
        _count = jsonReadInt(parameters, "count");
        if (_count == 0) _count = _isFloat ? 2 : 1;

        jsonRead(parameters, "onlyOnChange", _onlyOnChange);
        jsonRead(parameters, "round", _round);

        String parentId = jsonReadStr(parameters, "parent");
        if (GroupPollersMap.count(parentId)) {
            GroupPollersMap[parentId]->addChild(this);
            if (_modbusDebug) {
                SerialPrint("I", "ModbusGroupSens", "Сенсор id:" + getID() + " привязан к поллеру:" + parentId + " (offset/bit:" + String(_offset) + ")");
            }
        } else {
            SerialPrint("E", "ModbusGroupSens", "Ошибка: Родительский поллер не найден: " + parentId);
        }
    }

    void updateValueFromBuffer(ModbusMessage& response, uint8_t func) {
        uint16_t regIndex = _offset; 
        uint16_t byteOffset = 3 + (regIndex * 2);

        uint8_t requiredBytes = (_count > 0) ? (_count * 2) : 2;
        if ((byteOffset + requiredBytes) > response.size()) {
            return; 
        }

        float currentVal = 0.0f;

        if (func == 0x01 || func == 0x02) {
            uint16_t byteIdx = 3 + (_offset / 8);
            uint8_t bitIdx   = _offset % 8;
            if (byteIdx < response.size()) {
                currentVal = (response[byteIdx] & (1 << bitIdx)) ? 1.0f : 0.0f;
            }
        } 
else {
            if (_isFloat && _count == 2) {
                union {
                    uint32_t b32;
                    float f;
                } u;
                u.b32 = ((uint32_t)response[byteOffset]     << 24) |
                        ((uint32_t)response[byteOffset + 1] << 16) |
                        ((uint32_t)response[byteOffset + 2] << 8)  |
                         (uint32_t)response[byteOffset + 3];
                currentVal = u.f; // Для float знак распарсится сам
            } 
            else if (_count == 2) {
                // Знаковый Int32 (2 регистра)
                int32_t rawVal = ((uint32_t)response[byteOffset]     << 24) |
                                 ((uint32_t)response[byteOffset + 1] << 16) |
                                 ((uint32_t)response[byteOffset + 2] << 8)  |
                                  (uint32_t)response[byteOffset + 3];
                currentVal = (float)rawVal;
            } 
            else {
                // Знаковый Int16 (1 регистр) — кастуем к int16_t!
                uint16_t uVal = ((uint16_t)response[byteOffset] << 8) |
                                 (uint16_t)response[byteOffset + 1];
                int16_t rawVal = (int16_t)uVal; 
                currentVal = (float)rawVal;
            }

            if (_div != 0.0f) {
                currentVal /= _div;
            }
        }        

        if (_round > 0) {
            float factor = pow(10, _round);
            currentVal = round(currentVal * factor) / factor;
        }

        if (_onlyOnChange) {
            if (currentVal == _lastVal) return;
            _lastVal = currentVal;
        }

        regEvent(currentVal, "ModbusGroupSens");
    }

    void doByInterval() override {}
    ~ModbusGroupSens() {}
};

// -------------------------------------------------------------
// ВЫНОСНАЯ РЕАЛИЗАЦИЯ МЕТОДА parseMB
// -------------------------------------------------------------
void ModbusGroupPoll::parseMB(ModbusMessage response) {
    _waitingResponse = false;
    for (auto child : _children) {
        child->updateValueFromBuffer(response, _func);
    }
}

// -------------------------------------------------------------
// КОЛБЭКИ ОБРАБОТКИ ОТВЕТОВ EMODBUS И ОШИБОК
// -------------------------------------------------------------
void handleModBusGroupData(ModbusMessage response, uint32_t token) {
    if (_modbusDebug) {
        String hexBuf = "";
        for (auto byte : response) {
            if (byte < 16) hexBuf += "0";
            hexBuf += String(byte, HEX) + " ";
        }
        SerialPrint("I", "ModbusGroup", "Ответ token " + String(token) + " (" + String(response.size()) + " байт): " + hexBuf);
    }

    if (MBGroupTokenMap.count(token)) {
        MBGroupTokenMap[token]->parseMB(response);
        MBGroupTokenMap.erase(token);
    }
}

void handleModBusGroupError(Error error, uint32_t token) {
    ModbusError me(error);
    SerialPrint("E", "ModbusGroup", "Ошибка ответа token " + String(token) + ": " + String((int)me, HEX) + " - " + String((const char *)me));

    if (MBGroupTokenMap.count(token)) {
        if (MBGroupTokenMap[token]) {
            MBGroupTokenMap[token]->resetWaitingFlag();
        }
        MBGroupTokenMap.erase(token);
    }

    // Отправляем событие ошибки строго клиенту (mb16)
    if (_mbClientInstance) {
        _mbClientInstance->regEvent(1.0f, "mb_error");
    }
}

void *getAPI_ModbusGroup(String subtype, String param) {
    if (subtype == F("mbClient")) {
        return new ModbusClientAsync(param);
    }
    else if (subtype == F("mbPoll")) {
        ModbusGroupPoll* poll = new ModbusGroupPoll(param);
        GroupPollersMap[poll->getID()] = poll;
        return poll;
    }
    else if (subtype == F("mbSens")) {
        return new ModbusGroupSens(param);
    }
    return nullptr;
}