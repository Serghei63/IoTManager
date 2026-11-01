#include "Global.h"
#include "classes/IoTItem.h"
#include <map>
#include <HardwareSerial.h>

#include "Logging.h"
#include "ModbusClientRTU.h"
#include "CoilData.h"

Stream *_modbusUART = nullptr;

// Глобальный флаг дебага для всех функций модуля
bool _modbusDebug = false;

// Данные Modbus по умолчанию
int8_t MODBUS_DIR_PIN = 0;
#define MODBUS_UART_LINE 2
#define MODBUS_RX_PIN 18        // Rx pin
#define MODBUS_TX_PIN 19        // Tx pin
#define MODBUS_SERIAL_BAUD 9600 // Baud rate

uint32_t modBus_Token_count = 0; // Счетчик токенов для Нод
class ModbusNode;
std::map<uint32_t, ModbusNode *> MBNoneMap;
ModbusClientRTU *MB = nullptr;

ModbusClientRTU *instanceModBus(int8_t _DR)
{
  if (!MB)
  {
    if (_DR)
      MB = new ModbusClientRTU(_DR);
    else
      MB = new ModbusClientRTU();
  }
  return MB;
}

class ModbusNode : public IoTItem
{
private:
  uint8_t _addr = 0;    
  String _regStr = "";  
  String _funcStr = ""; 
  uint8_t _func;
  uint16_t _reg = 0;
  uint8_t _countReg = 1;
  uint32_t _token = 0;
  bool _isFloat = 0;
  CoilData _respCoil;

  // --- ФИЛЬТРАЦИЯ СПАМА ---
  bool _onlyOnChange = false; 
  float _lastVal = -999999.0f; 

public:
  ModbusNode(String parameters) : IoTItem(parameters)
  {
    _addr = jsonReadInt(parameters, "addr"); 
    jsonRead(parameters, "reg", _regStr);    
    jsonRead(parameters, "func", _funcStr);  
    jsonRead(parameters, "isFloat", _isFloat);
    _countReg = jsonReadInt(parameters, "count");
    
    jsonRead(parameters, "onlyOnChange", _onlyOnChange);
    jsonRead(parameters, "round", _round);

    _func = hexStringToUint8(_funcStr);
    
    if (_regStr.startsWith("0x") || _regStr.startsWith("0X")) {
      _reg = hexStringToUint16(_regStr);
    } else {
      _reg = _regStr.toInt();
    }

    modBus_Token_count++;
    _token = modBus_Token_count;
    MBNoneMap[_token] = this;

    if (_modbusDebug) {
      SerialPrint("I", "ModbusNode", "Добавлена нода id:" + getID() + ", token:" + String(_token) + ", reg:0x" + String(_reg, HEX) + ", onlyOnChange:" + String(_onlyOnChange));
    }
  }

  void doByInterval()
  {
    if (!MB) return;

    if (_modbusDebug) {
      SerialPrint("I", "ModbusNode", "sending request with token " + String(_token));
    }

    Error err = SUCCESS;
    if (_func == 0x04) {
      err = MB->addRequest(_token, _addr, READ_INPUT_REGISTER, _reg, _countReg);
    } else if (_func == 0x03) {
      err = MB->addRequest(_token, _addr, READ_HOLD_REGISTER, _reg, _countReg);
    } else if (_func == 0x01) {
      err = MB->addRequest(_token, _addr, READ_COIL, _reg, _countReg);
    } else if (_func == 0x02) {
      err = MB->addRequest(_token, _addr, READ_DISCR_INPUT, _reg, _countReg);
    }

    if (err != SUCCESS) {
      ModbusError e(err);
      SerialPrint("E", "ModbusNode", "Error creating request: " + String((int)e, HEX) + " - " + String((const char *)e));
    }
  }

  void parseMB(ModbusMessage response)
  {
    if (!MB) return;

    float currentVal = 0.0f;

    if (_func == 0x02 || _func == 0x01) 
    {
      CoilData cd(_countReg);
      cd.set(0, _countReg, (uint8_t *)response.data() + 3);
      _respCoil = cd;
      currentVal = (float)cd[0];
    }
    else 
    {
      if (_countReg == 2) 
      {
        if (_isFloat) {
          float val;
          response.get(3, val); 
          currentVal = val;
        } else {
          uint32_t rawVal = 0;
          response.get(3, rawVal);
          currentVal = (float)rawVal;

          if (_reg == 0x0005) {
            currentVal /= 1000.0f;
          }
        }
      } 
      else 
      {
        uint16_t val;
        response.get(3, val);
        
        if (_reg == 0x0000 && _func == 0x04) {
          currentVal = (float)val / 10.0f;
        } else {
          currentVal = (float)val;
        }
      }
    }

    // Фильтр повторов
    if (_onlyOnChange) {
      if (currentVal == _lastVal) {
        return; 
      }
      _lastVal = currentVal;
    }

    regEvent(currentVal, "ModbusNode");
  }

  IoTValue execute(String command, std::vector<IoTValue> &param)
  {
    IoTValue val;
    uint16_t _index = 0;

    if (command == "getBits") 
    {
      if (param.size())
      {
        if (_respCoil.size() > _index)
        {
          _index = param[0].valD;
          val.valD = _respCoil[_index];
          return val;
        }
      }
    }
    return {};
  }

  ~ModbusNode() {};
};

// Обработчик входящих ответов Modbus
void handleModBusData(ModbusMessage response, uint32_t token)
{
  if (_modbusDebug)
  {
    SerialPrint("I", "ModbusRTU", 
      "Response --- Token:" + String(token) + 
      " FC:0x" + String(response.getFunctionCode(), HEX) + 
      " Server:" + String(response.getServerID()) + 
      " Length:" + String(response.size())
    );
    HEXDUMP_N("Data dump", response.data(), response.size());
  }

  if (MBNoneMap[token])
  {
    MBNoneMap[token]->parseMB(response);
  }
  else if (_modbusDebug)
  {
    SerialPrint("E", "ModbusRTU", "Токен/Нода не найден: " + String(token));
  }
}

void handleModBusError(Error error, uint32_t token)
{
  ModbusError me(error);
  SerialPrint("E", "ModbusRTU", "Error response: " + String((int)me, HEX) + " - " + String((const char *)me));
}

class ModbusClientAsync : public IoTItem
{
private:
  int8_t _rx = MODBUS_RX_PIN;
  int8_t _tx = MODBUS_TX_PIN;
  int _baud = MODBUS_SERIAL_BAUD;
  String _prot = "SERIAL_8N1";
  int protocol = SERIAL_8N1;

  int _addr = 0;       
  String _regStr = ""; 
  uint16_t _reg = 0;
  bool _debug = false;         
  uint32_t _token = 0; 

public:
  ModbusClientAsync(String parameters) : IoTItem(parameters)
  {
    _rx = (int8_t)jsonReadInt(parameters, "RX");
    _tx = (int8_t)jsonReadInt(parameters, "TX");
    MODBUS_DIR_PIN = (int8_t)jsonReadInt(parameters, "DIR_PIN");
    _baud = jsonReadInt(parameters, "baud");
    _prot = jsonReadStr(parameters, "protocol");
    jsonRead(parameters, "debug", _debug);

    // Синхронизируем глобальный флаг дебага
    _modbusDebug = _debug;

    if (_prot == "SERIAL_8N1") protocol = SERIAL_8N1;
    else if (_prot == "SERIAL_8N2") protocol = SERIAL_8N2;

    pinMode(MODBUS_DIR_PIN, OUTPUT);
    digitalWrite(MODBUS_DIR_PIN, LOW);

    instanceModBus(MODBUS_DIR_PIN);
    _modbusUART = new HardwareSerial(MODBUS_UART_LINE);

    if (_debug)
    {
      SerialPrint("I", "ModbusClientAsync", "baud: " + String(_baud) + ", protocol: " + String(protocol, HEX) + ", RX: " + String(_rx) + ", TX: " + String(_tx));
    }

    RTUutils::prepareHardwareSerial((HardwareSerial &)*_modbusUART);
    ((HardwareSerial *)_modbusUART)->begin(_baud, protocol, _rx, _tx);
    ((HardwareSerial *)_modbusUART)->setTimeout(200);

    MB->onDataHandler(&handleModBusData);
    MB->onErrorHandler(&handleModBusError);
    MB->setTimeout(2000);
    MB->begin((HardwareSerial &)*_modbusUART);
  }

  IoTValue execute(String command, std::vector<IoTValue> &param)
  {
    IoTValue val;
    uint16_t _reg = 0;

    if (command == "writeSingleRegister") 
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        uint16_t state = param[2].valD;
        
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeSingleRegister, addr: 0x" + String((uint8_t)_addr, HEX) + ", reg: 0x" + String(_reg, HEX) + ", state: " + String(state));
        }

        Error err = MB->addRequest(_token, _addr, WRITE_HOLD_REGISTER, _reg, state);
        if (err != SUCCESS)
        {
          ModbusError e(err);
          SerialPrint("E", "ModbusClientAsync", "Error creating request: " + String((int)e, HEX) + " - " + String((const char *)e));
        }
      }
      return {};
    }
    else if (command == "writeSingleCoil") 
    {
      if (param.size())
      {
        _addr = param[0].valD;
        _reg = hexStringToUint16(param[1].valS);
        bool state = param[2].valD;
        
        if (_debug)
        {
          SerialPrint("I", "ModbusClientAsync", "writeSingleCoil, addr: 0x" + String((uint8_t)_addr, HEX) + ", reg: 0x" + String(_reg, HEX) + ", state: " + String(state));
        }

        Error err;
        ModbusMessage msg;
        if (state)
        {
          msg.setMessage(_addr, WRITE_COIL, _reg, 0xFF00);
          err = MB->addRequest(msg, _token);
        }
        else
        {
          err = MB->addRequest(_token, _addr, WRITE_COIL, _reg, 0);
        }

        if (err != SUCCESS)
        {
          ModbusError e(err);
          SerialPrint("E", "ModbusClientAsync", "Error creating request: " + String((int)e, HEX) + " - " + String((const char *)e));
        }
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
    return val;
  }

  ~ModbusClientAsync()
  {
    delete _modbusUART;
    _modbusUART = nullptr;
    MBNoneMap.clear();
  };
};

void *getAPI_ModbusRTUasync(String subtype, String param)
{
  if (subtype == F("mbNode"))
  {
    return new ModbusNode(param);
  }
  else if (subtype == F("mbClient"))
  {
    return new ModbusClientAsync(param);
  }
  return nullptr;
}