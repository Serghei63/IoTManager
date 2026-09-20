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
        
// Очищаем/останавливаем UART, если он уже был запущен ранее (при сохранении конфига)
if (_modbusUART != nullptr) {
    ((HardwareSerial *)_modbusUART)->end();
} else {
    _modbusUART = new HardwareSerial(MODBUS_UART_LINE);
}

if (_debug) {
    SerialPrint("I", "ModbusClientAsync", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
}

// Теперь изменение буферов внутри prepareHardwareSerial пройдет без ошибок!
RTUutils::prepareHardwareSerial((HardwareSerial &)*_modbusUART);
((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx);
((HardwareSerial *)_modbusUART)->setTimeout(200);
        // Очищаем/останавливаем UART, если он уже был запущен ранее (при сохранении конфига)
        if (_modbusUART != nullptr) {
            ((HardwareSerial *)_modbusUART)->end();
        } else {
            _modbusUART = new HardwareSerial(MODBUS_UART_LINE);
        }

        if (_debug) {
         SerialPrint("I", "ModbusClientAsync", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
        }

        // Теперь изменение буферов внутри prepareHardwareSerial пройдет без ошибок!
        RTUutils::prepareHardwareSerial((HardwareSerial &)*_modbusUART);
        ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx);
        ((HardwareSerial *)_modbusUART)->setTimeout(200);

        MB->onDataHandler(&handleModBusGroupData);
        MB->onErrorHandler(&handleModBusGroupError);
        MB->setTimeout(2000);
        MB->begin((HardwareSerial &)*_modbusUART);
    }

    // ЕДИНАЯ РЕАЛИЗАЦИЯ SETVALUE ВНУТРИ КЛАССА
    void setValue(const String& valStr, bool genEvent = true) override {
        String val = valStr;
        val.trim();

        if (_debug) {
            SerialPrint("I", "ModbusClientAsync", "setValue: " + val);
        }

        int openBracket = val.indexOf('(');
        int closeBracket = val.lastIndexOf(')');

        // Проверяем наличие скобок для вызова команды вида "writeSingleCoil(4, '0x0000', 1)"
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

    // --- ОБРАБОТКА ВЫЗОВОВ ИЗ СЦЕНАРИЕВ ---
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

        // 0x05: writeSingleCoil(addr, reg, state)
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

        // 0x06: writeSingleRegister(addr, reg, value)
        else if (command == "writeSingleRegister" && param.size() >= 3) {
            uint8_t addr = parseAddr(param[0]);
            uint16_t reg = parseRegister(param[1]);
            uint16_t val = param[2].isDecimal ? (uint16_t)param[2].valD : (uint16_t)param[2].valS.toInt();

            modBus_Token_count++;
            uint32_t token = modBus_Token_count;

            if (_debug) {
                SerialPrint("I", "ModbusClientAsync", "writeSingleRegister, addr: 0x" + String(addr, HEX) + ", reg: 0x" + String(reg, HEX) + ", val: " + String(val));
            }

            Error err = MB->addRequest(token, addr, WRITE_HOLD_REGISTER, reg, val);
            if (err != SUCCESS) {
                ModbusError e(err);
                SerialPrint("E", "ModbusClientAsync", "Error writeSingleRegister: " + String((int)e, HEX));
            }
            return {};
        }

        // 0x0F: writeMultipleCoils(addr, reg, count, value)
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

    else if (command == "writeMultipleRegisters" && param.size() >= 3) 
    {
      uint8_t addr = param[0].isDecimal ? (uint8_t)param[0].valD : (uint8_t)param[0].valS.toInt();
      uint16_t reg = 0;

      if (param[1].valS.startsWith("0x") || param[1].valS.startsWith("0X")) {
        reg = hexStringToUint16(param[1].valS);
      } else {
        reg = param[1].valS.toInt();
      }

      // Динамически определяем количество передаваемых регистров
      uint16_t numRegs = param.size() - 2;
      std::vector<uint16_t> wData(numRegs);

      String logMsg = "writeMultipleRegisters, addr: 0x" + String(addr, HEX) + 
                      ", reg: 0x" + String(reg, HEX) + 
                      ", count: " + String(numRegs) + " -> ";

      for (size_t i = 0; i < numRegs; i++) {
        // Поддерживаем конвертацию как из float/int (valD), так и из строк (valS)
        wData[i] = param[i + 2].isDecimal ? (uint16_t)param[i + 2].valD : (uint16_t)param[i + 2].valS.toInt();
        if (_debug) {
          logMsg += "val" + String(i + 1) + ": " + String(wData[i]) + " (0x" + String(wData[i], HEX) + ") ";
        }
      }

      if (_debug) {
        SerialPrint("I", "ModbusClientAsync", logMsg);
      }

      ModbusMessage msg;
      // Передаем фактическое количество регистров (numRegs) и байт (numRegs * 2)
      msg.setMessage(addr, WRITE_MULT_REGISTERS, reg, numRegs, (uint8_t)(numRegs * 2), wData.data());

      Error err = MB->addRequest(msg, (uint32_t)0);
      
      if (err != SUCCESS) {
        ModbusError e(err);
        SerialPrint("E", "ModbusClientAsync", "Ошибка 0x10: " + String((int)e, HEX) + " - " + String((const char *)e));
      }

      return {};
    }

    // 0x42: resetEnergy(addr)
else if (command == "resetEnergy" && param.size() >= 1) {
    uint8_t addr = parseAddr(param[0]);

    modBus_Token_count++;
    uint32_t token = modBus_Token_count;

    if (_debug) {
        SerialPrint("I", "ModbusClientAsync", "resetEnergy (0x42), addr: 0x" + String(addr, HEX));
    }

    // В eModbus кастомная функция передается как тип uint8_t (0x42)
    // Регистр и значение передаются как 0
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
    ~ModbusClientAsync() {}
};

// -------------------------------------------------------------
// 2. КЛАСС РОДИТЕЛЯ (Поллер — 0x01, 0x02, 0x03, 0x04)
// -------------------------------------------------------------
class ModbusGroupPoll : public IoTItem {
private:
    uint8_t _addr = 1;
    uint16_t _reg = 0;
    uint8_t _countReg = 1;
    uint8_t _func = 0x03;
    uint32_t _token = 0;
    
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

    void addChild(ModbusGroupSens* child) {
        _children.push_back(child);
    }

    void doByInterval() override {
        if (!MB) return;

        modBus_Token_count++;
        _token = modBus_Token_count;
        MBGroupTokenMap[_token] = this;

        if (_modbusDebug) {
            SerialPrint("I", "ModbusGroupPoll", "Групповой опрос token:" + String(_token) + " (" + getID() + ")");
        }

        Error err = SUCCESS;
        switch (_func) {
            case 0x01:
                err = MB->addRequest(_token, _addr, READ_COIL, _reg, _countReg);
                break;
            case 0x02:
                err = MB->addRequest(_token, _addr, READ_DISCR_INPUT, _reg, _countReg);
                break;
            case 0x04:
                err = MB->addRequest(_token, _addr, READ_INPUT_REGISTER, _reg, _countReg);
                break;
            case 0x03:
            default:
                err = MB->addRequest(_token, _addr, READ_HOLD_REGISTER, _reg, _countReg);
                break;
        }

        if (err != SUCCESS) {
            ModbusError e(err);
            SerialPrint("E", "ModbusGroupPoll", "Ошибка отправки запроса: " + String((int)e, HEX) + " - " + String((const char *)e));
            MBGroupTokenMap.erase(_token);
        }
    }

    void parseMB(ModbusMessage response);

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
    // В Modbus RTU ответе байты 0, 1, 2 — это Addr, Func, ByteCount.
    // Полезная дата начинается с индекса 3.
    // Если у вас в _offset хранится адрес регистра (например, 3 для тока):
    // uint16_t regIndex = _offset - _parentPollReg; 
    // Если в _offset хранится порядковый номер регистра от начала опроса (0, 3, 8...):
    uint16_t regIndex = _offset; 

    uint16_t byteOffset = 3 + (regIndex * 2);

    // Проверка на выход за пределы фактически полученного пакета
    uint8_t requiredBytes = (_count > 0) ? (_count * 2) : 2;
    if ((byteOffset + requiredBytes) > response.size()) {
        return; 
    }

    float currentVal = 0.0f;

    if (func == 0x01 || func == 0x02) {
        // Логика Coils / Discrete Inputs
        uint16_t byteIdx = 3 + (_offset / 8);
        uint8_t bitIdx   = _offset % 8;
        if (byteIdx < response.size()) {
            currentVal = (response[byteIdx] & (1 << bitIdx)) ? 1.0f : 0.0f;
        }
    } 
    else {
        if (_isFloat && _count == 2) {
            // Если прибор отдаст честный float32 (IEEE-754)
            union {
                uint32_t b32;
                float f;
            } u;
            u.b32 = ((uint32_t)response[byteOffset]     << 24) |
                    ((uint32_t)response[byteOffset + 1] << 16) |
                    ((uint32_t)response[byteOffset + 2] << 8)  |
                     (uint32_t)response[byteOffset + 3];
            currentVal = u.f;
        } 
        else if (_count == 2) {
            // 32-битное целое число (uint32_t / Long, например kWh)
            uint32_t rawVal = ((uint32_t)response[byteOffset]     << 24) |
                              ((uint32_t)response[byteOffset + 1] << 16) |
                              ((uint32_t)response[byteOffset + 2] << 8)  |
                               (uint32_t)response[byteOffset + 3];
            currentVal = (float)rawVal;
        } 
        else {
            // 16-битное целое число (uint16_t / Int, например U, I, P, F)
            uint16_t rawVal = ((uint16_t)response[byteOffset] << 8) |
                               (uint16_t)response[byteOffset + 1];
            
            // Если значение может быть отрицательным (например, мощность при отдаче в сеть)
            // currentVal = (int16_t)rawVal; 
            currentVal = (float)rawVal;
        }

        if (_div != 0.0f) {
            currentVal /= _div;
        }
    }

    // Округление результата
    if (_round > 0) {
        float factor = pow(10, _round);
        currentVal = round(currentVal * factor) / factor;
    }

    // Проверка изменения значения (оповещаем только при смене)
    if (_onlyOnChange) {
        if (currentVal == _lastVal) return;
        _lastVal = currentVal;
    }

    regEvent(currentVal, "ModbusGroupSens");
}
/*
    void updateValueFromBuffer(ModbusMessage& response, uint8_t func) {
        float currentVal = 0.0f;

        if (func == 0x01 || func == 0x02) {
            uint8_t byteIdx = 3 + (_offset / 8);
            uint8_t bitIdx  = _offset % 8;

            if (byteIdx >= response.size()) {
                if (_modbusDebug) {
                    SerialPrint("E", "ModbusGroupSens", "Ошибка: смещение бита " + String(_offset) + " выходит за рамки ответа (" + String(response.size()) + " байт)");
                }
                return;
            }

            uint8_t dataByte = response[byteIdx];
            currentVal = (dataByte & (1 << bitIdx)) ? 1.0f : 0.0f;
        } 
        else {
            uint8_t byteOffset = 3 + (_offset * 2);

            if (byteOffset + (_count * 2) > response.size()) {
                if (_modbusDebug) {
                    SerialPrint("E", "ModbusGroupSens", "Ошибка: смещение " + String(_offset) + " выходит за границы массива (" + String(response.size()) + " байт)");
                }
                return;
            }

            if (_isFloat) {
                float val;
                response.get(byteOffset, val);
                currentVal = val;
            } else {
                if (_count == 2) {
                    uint32_t rawVal = 0;
                    response.get(byteOffset, rawVal);
                    currentVal = (float)rawVal;
                } else {
                    uint16_t rawVal = 0;
                    response.get(byteOffset, rawVal);
                    currentVal = (float)rawVal;
                }
            }
            currentVal /= _div;
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
*/
    void doByInterval() override {}
    ~ModbusGroupSens() {}
};

// Передача ответа в дочерние сенсоры
void ModbusGroupPoll::parseMB(ModbusMessage response) {
    for (auto child : _children) {
        child->updateValueFromBuffer(response, _func);
    }
}

// -------------------------------------------------------------
// КОЛБЭКИ ОБРАБОТКИ ОТВЕТОВ EMODBUS
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
        MBGroupTokenMap.erase(token);
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