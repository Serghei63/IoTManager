#include "Global.h"
#include "classes/IoTItem.h"
#include "classes/IoTScenario.h"
#include "utils/FileUtils.h"
#include "utils/WiFiUtils.h"
#include "NTP.h"
#include <new>


// вызываем все нужное пока зависаем в сценариях
// HTTP.handleClient() здесь больше не вызываем: дерево сценария живет постоянно и обработчик
// запроса, перезагрузивший сценарий во время выполнения, освободил бы выполняемые узлы
void scenario_yield() {
    yield();
}

bool isIotScenException = false;  // признак исключения и попытки прекратить выполнение сценария заранее

// Лексический анализатор возвращает токены [0-255], если это неизвестны,
// иначе одну из известных единиц кода
enum Token {
    tok_eof = -1,

    // операнды (первичные выражения: идентификаторы, числа)
    tok_identifier = -4,
    tok_number = -5,
    tok_string = -3,

    // двухсимвольные операторы бинарных операций
    tok_equal = -2,
    tok_notequal = -9,
    tok_lesseq = -10,
    tok_greateq = -11,
    tok_silentset = -12,

    // управление
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,

    // инкременты, декременты и составные присваивания
    tok_inc = -13,        // ++
    tok_dec = -14,        // --
    tok_addassign = -15,  // +=
    tok_subassign = -16,  // -=
    tok_mulassign = -17,  // *=
    tok_divassign = -18   // /=
};

// Таблица приоритетов бинарных операторов. 1 - наименьший приоритет.
struct BinOpEntry {
    signed char op;
    int8_t prec;
};

static const BinOpEntry BinopPrecedence[] = {
    {tok_silentset, 1},
    {'=', 2},
    {tok_addassign, 2},
    {tok_subassign, 2},
    {tok_mulassign, 2},
    {tok_divassign, 2},
    {'|', 5},
    {'&', 6},
    {tok_equal, 10},     // ==
    {tok_notequal, 11},  // !=
    {tok_lesseq, 15},    // <=
    {tok_greateq, 16},   // >=
    {'<', 20},
    {'>', 21},
    {'+', 25},
    {'-', 26},
    {'/', 27},
    {'*', 28}  // highest.
};

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (Абстрактное Синтаксическое Дерево или Дерево Парсинга)
//===----------------------------------------------------------------------===//

/// ExprAST - Базовый класс для всех узлов выражений.
ExprAST::~ExprAST() {}
IoTValue *ExprAST::exec() { return nullptr; }
int ExprAST::setValue(IoTValue *val, bool generateEvent) { return 0; }  // 0 - установка значения не поддерживается наследником
bool ExprAST::hasEventIdName(uint32_t eventHash) { return false; }      // по умолчанию все узлы не связаны с ИД события, для которого выполняется сценарий

static bool isTruthy(IoTValue *val) {
    // если число не равно нулю или строка не равна пустой, то считаем значение истинным
    return val->isDecimal ? val->valD != 0 : val->valS != "";
}

static void deleteAll(std::vector<ExprAST *> &list) {
    for (unsigned int i = 0; i < list.size(); i++) {
        if (list[i]) delete list[i];
    }
    list.clear();
}

/// NumberExprAST - Класс узла выражения для числовых литералов (Например, "1.0").
class NumberExprAST : public ExprAST {
    IoTValue Val;

   public:
    NumberExprAST(const String &val) {
        Val.valD = strtod(val.c_str(), 0);
        Val.valS = val;  // сохраняем исходный вид числа для строковых сравнений и сложения со строками
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        return &Val;
    }
};

/// StringExprAST - Класс узла выражения для строковых литералов (Например, "Example with spaces and quotes").
class StringExprAST : public ExprAST {
    IoTValue Val;

   public:
    StringExprAST(const String &val) {
        Val.isDecimal = false;
        Val.valS = val;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        return &Val;
    }
};

/// VariableExprAST - Класс узла выражения для переменных (например, "a").
/// Элемент ищется по хэшу при каждом обращении, т.к. дерево живет долго,
/// а элементы могут появляться из сети и удаляться в процессе работы
class VariableExprAST : public ExprAST {
    uint32_t NameHash;

   public:
    VariableExprAST(uint32_t nameHash) : NameHash(nameHash) {}

    int setValue(IoTValue *val, bool generateEvent) {
        IoTItem *Item = findIoTItem(NameHash);
        if (!Item) return 0;

        Item->setValue(*val, generateEvent);
        return 1;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        IoTItem *Item = findIoTItem(NameHash);
        if (!Item) return nullptr;  // элемент не найден, возможно еще не пришел из сети

        Item->getRoundValue();
        return &(Item->value);
    }
};

/// UnaryNotExprAST - Класс узла выражения для логического отрицания "!x".
class UnaryNotExprAST : public ExprAST {
    ExprAST *Operand;
    IoTValue val;

   public:
    UnaryNotExprAST(ExprAST *operand) : Operand(operand) {}

    ~UnaryNotExprAST() {
        if (Operand) delete Operand;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        if (Operand == nullptr) return nullptr;

        IoTValue *opVal = Operand->exec();
        if (opVal == nullptr) return nullptr;

        val.valD = !isTruthy(opVal);
        return &val;
    }
};

/// BinaryExprAST - Класс узла выражения для бинарных операторов.
class BinaryExprAST : public ExprAST {
    signed char Op;
    ExprAST *LHS, *RHS;
    IoTValue *val = nullptr;  // создается при первом вычислении, т.к. для простых присваиваний не нужен

    IoTValue *getVal() {
        if (!val) {
            val = new (std::nothrow) IoTValue();
            if (!val) SerialPrint("E", F("Scenario"), F("Out of memory"));
        }
        return val;
    }

   public:
    BinaryExprAST(signed char op, ExprAST *lhs, ExprAST *rhs)
        : Op(op), LHS(lhs), RHS(rhs) {}

    ~BinaryExprAST() {
        if (LHS) delete LHS;
        if (RHS) delete RHS;
        if (val) delete val;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        if (RHS == nullptr || LHS == nullptr) return nullptr;

        IoTValue *rhs = RHS->exec();  // получаем значение правого операнда для возможного использования в операции присваивания
        if (rhs == nullptr) return nullptr;

        if (Op == '=' && LHS->setValue(rhs, true)) {  // если установка значения не поддерживается, т.е. слева не переменная, то работаем по другим комбинациям далее
            return rhs;                               // иначе возвращаем присвоенное значение справа
        }

        if (Op == tok_silentset && LHS->setValue(rhs, false)) {  // если установка значения не поддерживается, т.е. слева не переменная, то работаем по другим комбинациям далее
            return rhs;                                          // иначе возвращаем присвоенное значение справа
        }

        IoTValue *lhs = LHS->exec();  // если присваивания не произошло, значит операция иная и необходимо значение левого операнда
        if (lhs == nullptr) return nullptr;

        // составные присваивания += -= *= /= вычисляются как обычная операция с последующей записью результата
        signed char calcOp = Op;
        switch (Op) {
            case tok_addassign: calcOp = '+'; break;
            case tok_subassign: calcOp = '-'; break;
            case tok_mulassign: calcOp = '*'; break;
            case tok_divassign: calcOp = '/'; break;
        }

        IoTValue *res = calc(calcOp, lhs, rhs);
        if (res && calcOp != Op) LHS->setValue(res, true);
        return res;
    }

    IoTValue *calc(signed char op, IoTValue *lhs, IoTValue *rhs) {
        IoTValue *val = getVal();
        if (val == nullptr) return nullptr;

        // все бинарные операции кроме +, -, != и == обязаны работать с числами
        if (op != '+' && op != '-' && op != tok_equal && op != tok_notequal) {
            // поэтому преобразовываем строки в булевые интерпретации
            if (!lhs->isDecimal) lhs->valD = lhs->valS != "";  // пустая строка = false
            if (!rhs->isDecimal) rhs->valD = rhs->valS != "";  // пустая строка = false

            val->isDecimal = true;
            switch (op) {
                case '>':
                    val->valD = lhs->valD > rhs->valD;
                    break;
                case '<':
                    val->valD = lhs->valD < rhs->valD;
                    break;
                case tok_lesseq:
                    val->valD = lhs->valD <= rhs->valD;
                    break;
                case tok_greateq:
                    val->valD = lhs->valD >= rhs->valD;
                    break;

                case '*':
                    val->valD = lhs->valD * rhs->valD;
                    break;
                case '/':
                    if (rhs->valD != 0)
                        val->valD = lhs->valD / rhs->valD;
                    else
                        val->valD = 3.4E+38;
                    break;

                case '|':
                    val->valD = lhs->valD || rhs->valD;
                    break;
                case '&':
                    val->valD = lhs->valD && rhs->valD;
                    break;

                default:
                    break;
            }
        } else {    // иначе имеем дело с операциями + или - или == или !=, которые могут работать с разными типами данных
            if (lhs->isDecimal && lhs->valS == "") lhs->valS = (String)lhs->valD;   // небольшой костыль пока не переделаем работу со значениями, планируется добавить long, работу со временем, перенести округление и модификаторы в IoTValue
            if (rhs->isDecimal && rhs->valS == "") rhs->valS = (String)rhs->valD;   // пока для сохранения округления в IoTItem применяется хитрость с сохранением внешнего вида числа в строку valS,
                                                                                    // но некоторые модули и системные не делают этого, поэтому отлавливаем эту ситуацию тут и учитываем.
            val->isDecimal = true;
            switch (op) {
                case tok_equal:
                    if (lhs->isDecimal && rhs->isDecimal)
                        val->valD = lhs->valD == rhs->valD;
                    else
                        val->valD = compStr(lhs->valS, rhs->valS);
                    break;

                case tok_notequal:
                    if (lhs->isDecimal && rhs->isDecimal)
                        val->valD = lhs->valD != rhs->valD;
                    else
                        val->valD = !compStr(lhs->valS, rhs->valS);
                    break;

                case '+':
                    if (lhs->isDecimal && rhs->isDecimal)
                        val->valD = lhs->valD + rhs->valD;
                    else {
                        val->valS = lhs->valS + rhs->valS;
                        val->valD = 1;
                        val->isDecimal = false;
                    }
                    break;

                case '-':
                    if (lhs->isDecimal && rhs->isDecimal)
                        val->valD = lhs->valD - rhs->valD;
                    else {
                        val->valS = lhs->valS;
                        val->valS.replace(rhs->valS, "");
                        val->valD = 1;
                        val->isDecimal = false;
                    }
                    break;

                default:
                    break;
            }
        }
        if (val->isDecimal) val->valS = "";  // числовой результат не должен нести строковое представление предыдущих вычислений
        return val;
    }

    bool compStr(const String &str1, const String &str2) {
        if (str1.length() != str2.length()) return false;
        for (unsigned int i = 0; i < str1.length(); i++) {
            if (str1[i] == '*' || str2[i] == '*') continue;  //считаем, что если есть подстановочная звезда, то символы равны
            if (str1[i] != str2[i]) return false;
        }

        return true;  // str1 == str2;
    }
};

/// CallExprAST - Класс узла выражения для вызова команды модуля.
class CallExprAST : public ExprAST {
    enum CallOp : uint8_t {
        callop_module = 0,
        callop_exit,
        callop_getIntFromNet,
        callop_doByInterval,
        callop_setInterval
    };

    uint32_t CalleeHash;
    String Cmd;
    CallOp operation;
    std::vector<ExprAST *> Args;
    IoTValue ret;  // хранение возвращаемого значения, т.к. возврат по ссылке осуществляется

   public:
    CallExprAST(uint32_t calleeHash, const String &callee, const String &cmd, std::vector<ExprAST *> &args)
        : CalleeHash(calleeHash), Cmd(cmd), Args(args) {
        if (Cmd == F("exit") || callee == F("exit"))
            operation = callop_exit;
        else if (Cmd == F("getIntFromNet"))
            operation = callop_getIntFromNet;
        else if (Cmd == F("doByInterval"))
            operation = callop_doByInterval;
        else if (Cmd == F("setInterval"))
            operation = callop_setInterval;
        else
            operation = callop_module;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;  // если прерывание, то сразу выходим

        if (operation == callop_exit) {  // если системная команда, то выполняем и выходим
            IoTValue *tmp = nullptr;
            if (Args.size() > 0 && Args[0]) tmp = Args[0]->exec();
            if (tmp)
                SerialPrint("i", "SysExt", "Exit = '" + tmp->valS + "'");
            else
                SerialPrint("i", "SysExt", "Exit");

            isIotScenException = true;
            return nullptr;
        }

        IoTItem *Item = findIoTItem(CalleeHash);  // элемент мог появиться из сети или удалиться в процессе работы
        if (!Item) return nullptr;

        if (operation == callop_getIntFromNet) {
            ret.valD = Item->getIntFromNet();
            ret.isDecimal = true;
            return &ret;

        } else if (operation == callop_doByInterval) {  // вызываем системную функцию периодического выполнения вне таймера
            Item->doByInterval();
            ret = Item->value;
            return &ret;
        }

        // если все же все ок, то готовим параметры для передачи в модуль
        std::vector<IoTValue> ArgsAsIoTValue;
        ArgsAsIoTValue.reserve(Args.size());
        for (unsigned int i = 0; i < Args.size(); i++) {
            if (Args[i] == nullptr) return nullptr;
            IoTValue *tmp = Args[i]->exec();
            if (tmp != nullptr)
                ArgsAsIoTValue.push_back(*tmp);
            else
                return nullptr;
        }

        if (operation == callop_setInterval) {  // меняем интервал выполнения задач модуля налету
            if (ArgsAsIoTValue.size() == 1) {
                Item->setInterval(ArgsAsIoTValue[0].valD);
                ret.valD = Item->getInterval();
                ret.isDecimal = true;
                return &ret;
            }
        }

        ret = Item->execute(Cmd, ArgsAsIoTValue);  // вызываем команду из модуля напрямую с передачей всех аргументов
        return &ret;
    }

    ~CallExprAST() {
        deleteAll(Args);
    }
};

// Для сокращения количества преобразований используем числовые коды для фиксации названия системной функции,
// которые поддерживает прошивка
enum SysOp {
    sysop_notfound = 0,
    sysop_reboot = 1,
    sysop_digitalRead,
    sysop_analogRead,     //
    sysop_digitalWrite,   //
    sysop_digitalInvert,  //
    sysop_deepSleep,      //
    sysop_getHours,       //
    sysop_getMinutes,     //
    sysop_getSeconds,     //
    sysop_getMonth,       //
    sysop_getDay,
    sysop_gethhmm,
    sysop_gethhmmss,
    sysop_getTime,
    sysop_getRSSI,
    sysop_getIP,
    sysop_mqttPub,
    sysop_getUptime,
    sysop_mqttIsConnect,
    sysop_wifiIsConnect,
    sysop_setInterval,
    sysop_addPortMap
};

static SysOp getSysOp(uint32_t calleeHash) {
    switch (calleeHash) {
        case hashLit("reboot"): return sysop_reboot;
        case hashLit("digitalRead"): return sysop_digitalRead;
        case hashLit("analogRead"): return sysop_analogRead;
        case hashLit("digitalWrite"): return sysop_digitalWrite;
        case hashLit("digitalInvert"): return sysop_digitalInvert;
        case hashLit("deepSleep"): return sysop_deepSleep;
        case hashLit("getHours"): return sysop_getHours;
        case hashLit("getMinutes"): return sysop_getMinutes;
        case hashLit("getSeconds"): return sysop_getSeconds;
        case hashLit("getMonth"): return sysop_getMonth;
        case hashLit("getDay"): return sysop_getDay;
        case hashLit("gethhmm"): return sysop_gethhmm;
        case hashLit("gethhmmss"): return sysop_gethhmmss;
        case hashLit("getTime"): return sysop_getTime;
        case hashLit("getRSSI"): return sysop_getRSSI;
        case hashLit("getIP"): return sysop_getIP;
        case hashLit("mqttPub"): return sysop_mqttPub;
        case hashLit("getUptime"): return sysop_getUptime;
        case hashLit("mqttIsConnect"): return sysop_mqttIsConnect;
        case hashLit("wifiIsConnect"): return sysop_wifiIsConnect;
        case hashLit("setInterval"): return sysop_setInterval;
        case hashLit("addPortMap"): return sysop_addPortMap;
        default: return sysop_notfound;
    }
}

IoTValue sysExecute(SysOp command, std::vector<IoTValue> &param) {
    IoTValue value = {};

    if (_time_isTrust)
        switch (command) {
            case sysop_getHours:
                value.valD = _time_local.hour;
                break;
            case sysop_getMinutes:
                value.valD = _time_local.minute;
                break;
            case sysop_getSeconds:
                value.valD = _time_local.second;
                break;
            case sysop_getMonth:
                value.valD = _time_local.month;
                break;
            case sysop_getDay:
                value.valD = _time_local.day_of_month;
                break;
            case sysop_gethhmm:
                value.isDecimal = false;
                value.valS = getTimeLocal_hhmm();
                break;
            case sysop_gethhmmss:
                value.isDecimal = false;
                value.valS = getTimeLocal_hhmmss();
                break;
            case sysop_getTime:
                value.isDecimal = false;
                value.valS = getDateTimeDotFormated();
                break;
            default:
                break;
        }
    else {
        value.isDecimal = false;
        value.valS = "none";
    }

    switch (command) {
        case sysop_reboot:
            ESP.restart();
            break;
        case sysop_digitalRead:
            if (param.size()) {
                IoTgpio.pinMode(param[0].valD, INPUT);
                value.valD = IoTgpio.digitalRead(param[0].valD);
            }
            break;
        case sysop_analogRead:
            if (param.size()) {
                IoTgpio.pinMode(param[0].valD, INPUT);
                value.valD = IoTgpio.analogRead(param[0].valD);
            }
            break;
        case sysop_digitalWrite:
            if (param.size() == 2) {
                IoTgpio.pinMode(param[0].valD, OUTPUT);
                IoTgpio.digitalWrite(param[0].valD, param[1].valD);
            }
            break;
        case sysop_digitalInvert:
            if (param.size()) {
                IoTgpio.pinMode(param[0].valD, OUTPUT);
                IoTgpio.digitalInvert(param[0].valD);
            }
            break;
        case sysop_deepSleep:
            if (param.size()) {
                Serial.printf("Ушел спать на %d сек...", (int)param[0].valD);
#if defined(ESP32)
                esp_sleep_enable_timer_wakeup(param[0].valD * 1000000);
                delay(1000);
                esp_deep_sleep_start();
#elif defined(ESP8266)
                ESP.deepSleep(param[0].valD * 1000000);
#endif
            }
            break;
        case sysop_getRSSI:
            value.valD = WiFi.RSSI();
            value.isDecimal = true;
            break;
        case sysop_getIP:
            value.valS = jsonReadStr(settingsFlashJson, F("ip"));
            value.isDecimal = false;
            break;
        case sysop_mqttPub:
            if (param.size() == 2) {
                String tmpStr = param[1].valS;
                if (param[1].isDecimal) tmpStr = String(param[1].valD);
                value.valD = mqtt.publish(param[0].valS.c_str(),  tmpStr.c_str(), false);
            }
            break;
        case sysop_getUptime:
            value.valS = jsonReadStr(errorsHeapJson, F("upt"));
            value.isDecimal = false;
            break;
        case sysop_mqttIsConnect:
            value.valD = mqttIsConnect();
            break;
        case sysop_wifiIsConnect:
            value.valD = isNetworkActive();
            break;
        case sysop_setInterval:
            if (param.size() == 1) {

            }
            break;
        case sysop_addPortMap:
            if (param.size() == 5) {
                addPortMap(param[0].valS,  param[1].valS, param[2].valD, param[3].valS, param[4].valD);
            }
            break;
        default:
            break;
    }

    return value;
}

/// SysCallExprAST - Класс узла выражения для вызова системных команд.
class SysCallExprAST : public ExprAST {
    std::vector<ExprAST *> Args;
    IoTValue ret;  // хранение возвращаемого значения, т.к. возврат по ссылке осуществляется
    SysOp operation;

   public:
    SysCallExprAST(uint32_t calleeHash, std::vector<ExprAST *> &args)
        : Args(args), operation(getSysOp(calleeHash)) {}

    IoTValue *exec() {
        if (isIotScenException) return nullptr;  // если прерывание, то сразу выходим

        // готовим параметры для передачи в в функцию интерпретатор действий
        std::vector<IoTValue> ArgsAsIoTValue;
        ArgsAsIoTValue.reserve(Args.size());
        for (unsigned int i = 0; i < Args.size(); i++) {
            if (Args[i] == nullptr) return nullptr;
            IoTValue *tmp = Args[i]->exec();
            if (tmp != nullptr)
                ArgsAsIoTValue.push_back(*tmp);
            else
                return nullptr;
        }

        ret = sysExecute(operation, ArgsAsIoTValue);  // вызываем функцию интерпретатор с передачей всех аргументов

        return &ret;
    }

    ~SysCallExprAST() {
        deleteAll(Args);
    }
};

/// IfExprAST - Класс узла выражения для if/then/else.
class IfExprAST : public ExprAST {
    ExprAST *Cond, *Then, *Else;
    uint32_t *IDHashes = nullptr;  // хэши ИД, встречающихся в условиях (только у if верхнего уровня, включая вложенные условия)
    uint16_t IDCount = 0;

   public:
    IfExprAST(ExprAST *cond, ExprAST *then, ExprAST *_else, const std::vector<uint32_t> *hashes)
        : Cond(cond), Then(then), Else(_else) {
        if (hashes && hashes->size()) {
            IDHashes = new (std::nothrow) uint32_t[hashes->size()];
            if (IDHashes) {
                IDCount = hashes->size();
                for (uint16_t i = 0; i < IDCount; i++) IDHashes[i] = (*hashes)[i];
            } else
                SerialPrint("E", F("Scenario"), F("Out of memory"));
        }
    }

    bool hasEventIdName(uint32_t eventHash) {
        for (uint16_t i = 0; i < IDCount; i++) {  // определяем встречался ли ИД, для которого исполняем сценарий в выражении IF
            if (IDHashes[i] == eventHash) return true;
        }
        return false;
    }

    IoTValue *exec() {
        if (isIotScenException) return nullptr;
        IoTValue *cond_ret = nullptr;

        if (Cond) cond_ret = Cond->exec();

        if (!cond_ret) return nullptr;

        if (isTruthy(cond_ret)) {
            if (Then == nullptr) return nullptr;
            Then->exec();
        } else {
            if (Else == nullptr) return nullptr;
            Else->exec();
        }

        scenario_yield();

        return cond_ret;
    }

    ~IfExprAST() {
        if (Cond) delete Cond;
        if (Then) delete Then;
        if (Else) delete Else;
        if (IDHashes) delete[] IDHashes;
    }
};

/// BracketsExprAST - Класс узла блока кода {}.
class BracketsExprAST : public ExprAST {
    std::vector<ExprAST *> BracketsList;

   public:
    BracketsExprAST(std::vector<ExprAST *> &bracketsList)
        : BracketsList(bracketsList) {}

    IoTValue *exec() {
        if (isIotScenException) return nullptr;

        IoTValue *lastExecValue = nullptr;
        for (unsigned int i = 0; i < BracketsList.size(); i++) {
            if (BracketsList[i] == nullptr) return nullptr;
            lastExecValue = BracketsList[i]->exec();
            if (isIotScenException) break;
        }

        return lastExecValue;
    }

    ~BracketsExprAST() {
        deleteAll(BracketsList);
    }
};

//===----------------------------------------------------------------------===//
// Lexer (Лексический анализатор)
//===----------------------------------------------------------------------===//

int IoTScenario::getLastChar() {
    if (file && file.available()) {
        LastChar = file.read();
        if (LastChar == 10) curLine++;
        return LastChar;
    }
    return EOF;
}

/// gettok - Возвращает следующий токен из файла сценария.
int IoTScenario::gettok() {
    // Пропускаем пробелы.
    while (isspace(LastChar) || LastChar == ';')
        LastChar = getLastChar();

    if (isalpha(LastChar) || LastChar == '_') {  // идентификатор: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = String((char)LastChar);
        while (isalnum((LastChar = getLastChar())) || LastChar == '_') {
            IdentifierStr += (char)LastChar;
        }

        if (IdentifierStr == "if") return tok_if;
        if (IdentifierStr == "then") return tok_then;
        if (IdentifierStr == "else") return tok_else;
        IdentifierHash = hashFNV1a(IdentifierStr);
        return tok_identifier;
    }

    if (isdigit(LastChar)) {  // Число: [0-9.]+   знак минус разбирается парсером
        NumStr = "";
        do {
            NumStr += (char)LastChar;
            LastChar = getLastChar();
        } while (isdigit(LastChar) || LastChar == '.');

        return tok_number;
    }

    if (LastChar == '#') {
        // Комментарий до конца строки
        do LastChar = getLastChar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    if (LastChar == '"') {  // "строка"
        IdentifierStr = "";
        LastChar = getLastChar();
        while (LastChar != '"' && LastChar != EOF) {
            if (LastChar == '\\') {     // обработка экранированных символов в строке
                LastChar = getLastChar();
                if (LastChar == '"') {
                    IdentifierStr += '"';
                } else if (LastChar == 'n') {
                    IdentifierStr += '\n';
                } else if (LastChar == '\\') {
                    IdentifierStr += '\\';
                }
            } else {
                IdentifierStr += (char)LastChar;
            }
            LastChar = getLastChar();
        }
        LastChar = getLastChar();

        return tok_string;
    }

    // Проверка конца файла.
    if (LastChar == EOF)
        return tok_eof;

    // двухсимвольные операторы: первый символ, второй символ, токен
    static const struct {
        char first, second;
        signed char tok;
    } twoCharOps[] = {
        {'=', '=', tok_equal},
        {':', '=', tok_silentset},
        {'!', '=', tok_notequal},
        {'<', '=', tok_lesseq},
        {'>', '=', tok_greateq},
        {'+', '+', tok_inc},
        {'+', '=', tok_addassign},
        {'-', '-', tok_dec},
        {'-', '=', tok_subassign},
        {'*', '=', tok_mulassign},
        {'/', '=', tok_divassign}};

    int ThisChar = LastChar;
    LastChar = getLastChar();
    for (unsigned int i = 0; i < sizeof(twoCharOps) / sizeof(twoCharOps[0]); i++) {
        if (twoCharOps[i].first == ThisChar && twoCharOps[i].second == LastChar) {
            LastChar = getLastChar();
            return twoCharOps[i].tok;
        }
    }

    // В противном случае просто возвращаем символ как значение ASCII
    return ThisChar;
}

//===----------------------------------------------------------------------===//
// Parser (Парсер или Синтаксический Анализатор)
//===----------------------------------------------------------------------===//

int IoTScenario::getNextToken() {
    return CurTok = gettok();
}

/// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
int IoTScenario::GetTokPrecedence() {
    for (unsigned int i = 0; i < sizeof(BinopPrecedence) / sizeof(BinopPrecedence[0]); i++) {
        if (BinopPrecedence[i].op == CurTok) return BinopPrecedence[i].prec;
    }
    return -1;
}

/// Error* - Это небольшие вспомогательные функции для обработки ошибок.
ExprAST *IoTScenario::Error(const String &Str) {
    Serial.printf("Scenario error in line %d: %s\n", curLine, Str.c_str());
    _parseError = true;
    return nullptr;
}

ExprAST *IoTScenario::ParseIdentifierExpr(bool callFromCondition) {
    if (_parseError) return nullptr;

    String IdName = IdentifierStr;
    uint32_t IdHash = IdentifierHash;
    String Cmd = "";

    getNextToken();  // получаем идентификатор.

    if (CurTok == '.') {
        getNextToken();
        Cmd = IdentifierStr;
        getNextToken();
    }

    if (CurTok != '(') {  // Обычная переменная.
        // создаем экземпляр переменной в любом случае, даж если элемента нет, т.к. переменная может придти из сети позже
        ExprAST *Var = new VariableExprAST(IdHash);

        if (CurTok == tok_inc || CurTok == tok_dec) {  // постфиксные a++ и a--
            signed char op = CurTok == tok_inc ? tok_addassign : tok_subassign;
            getNextToken();
            return new BinaryExprAST(op, Var, new NumberExprAST("1"));
        }
        return Var;
    }

    // Вызов функции.
    getNextToken();  // получаем (
    std::vector<ExprAST *> Args;
    if (CurTok != ')') {
        while (1) {
            ExprAST *Arg = ParseExpression(callFromCondition);
            if (!Arg) {
                deleteAll(Args);
                return nullptr;
            }
            Args.push_back(Arg);

            if (CurTok == ')') break;

            if (CurTok != ',') {
                deleteAll(Args);
                return Error(F("Expected ')' or ',' in argument list"));
            }
            getNextToken();
        }
    }

    getNextToken();  // получаем )

    if (Cmd == "")
        return new SysCallExprAST(IdHash, Args);  // создаем объект запуска системной функции
    else
        return new CallExprAST(IdHash, IdName, Cmd, Args);  // создаем объект запуска функции в любом случае даж если элемента пока нет
}

ExprAST *IoTScenario::ParseNumberExpr(bool negative) {
    if (_parseError) return nullptr;

    String numText = NumStr;
    if (negative) numText = "-" + NumStr;
    ExprAST *Result = new NumberExprAST(numText);
    getNextToken();  // получаем число
    return Result;
}

ExprAST *IoTScenario::ParseParenExpr(bool callFromCondition) {
    getNextToken();  // получаем (.
    ExprAST *V = ParseExpression(callFromCondition);
    if (!V) return nullptr;

    if (CurTok != ')') {
        delete V;
        return Error(F("expected ')'"));
    }
    getNextToken();  // получаем ).
    return V;
}

ExprAST *IoTScenario::ParseBracketsExpr(bool callFromCondition) {
    if (_parseError) return nullptr;

    getNextToken();  // получаем {.
    std::vector<ExprAST *> bracketsList;

    while (CurTok != '}') {
        ExprAST *Expr = ParseExpression(callFromCondition);
        if (!Expr) {
            deleteAll(bracketsList);
            return nullptr;
        }
        bracketsList.push_back(Expr);

        if (CurTok == tok_eof) {
            deleteAll(bracketsList);
            return Error(F("Expected '}'"));
        }
    }

    getNextToken();  // получаем }.
    return new BracketsExprAST(bracketsList);
}

ExprAST *IoTScenario::ParseQuotesExpr() {
    if (_parseError) return nullptr;

    ExprAST *Result = new StringExprAST(IdentifierStr);
    getNextToken();  // получаем строку
    return Result;
}

ExprAST *IoTScenario::ParseIfExpr() {
    if (_parseError) return nullptr;

    bool topLevel = _ifDepth++ == 0;
    getNextToken();  // Получаем if.

    // условие.
    ExprAST *Cond = ParseExpression(true);
    if (!Cond) return nullptr;

    if (CurTok != tok_then) {
        delete Cond;
        return Error(F("expected then"));
    }
    getNextToken();  // Получаем then

    ExprAST *Then = ParseExpression(false);
    if (!Then) {
        delete Cond;
        return nullptr;
    }

    ExprAST *Else = nullptr;
    if (CurTok == tok_else) {
        getNextToken();
        Else = ParseExpression(false);
        if (!Else) {
            delete Cond;
            delete Then;
            return nullptr;
        }
    }

    _ifDepth--;
    // ИД из условий вложенных if тоже относятся к if верхнего уровня, т.к. только он проверяется при событии
    return new IfExprAST(Cond, Then, Else, topLevel ? &_condHashes : nullptr);
}

ExprAST *IoTScenario::ParsePrimary(bool callFromCondition) {
    switch (CurTok) {
        default:
            Serial.println(CurTok);
            return Error(F("unknown token when expecting an expression"));
        case tok_identifier: {
            if (callFromCondition) {
                bool found = false;
                for (unsigned int i = 0; i < _condHashes.size(); i++) {
                    if (_condHashes[i] == IdentifierHash) {
                        found = true;
                        break;
                    }
                }
                if (!found) _condHashes.push_back(IdentifierHash);
            }
            return ParseIdentifierExpr(callFromCondition);
        }
        case tok_number:
            return ParseNumberExpr();
        case '-': {  // унарный минус
            getNextToken();
            if (CurTok == tok_number) return ParseNumberExpr(true);
            ExprAST *Operand = ParsePrimary(callFromCondition);
            if (!Operand) return nullptr;
            return new BinaryExprAST('-', new NumberExprAST("0"), Operand);
        }
        case '!': {  // логическое отрицание
            getNextToken();
            ExprAST *Operand = ParsePrimary(callFromCondition);
            if (!Operand) return nullptr;
            return new UnaryNotExprAST(Operand);
        }
        case '(':
            return ParseParenExpr(callFromCondition);
        case '{':
            return ParseBracketsExpr(callFromCondition);
        case tok_string:
            return ParseQuotesExpr();
        case tok_if:
            return ParseIfExpr();
    }
}

ExprAST *IoTScenario::ParseBinOpRHS(int ExprPrec, ExprAST *LHS, bool callFromCondition) {
    // Если это бинарный оператор, получаем его приоритет
    while (1) {
        if (_parseError) {
            delete LHS;
            return nullptr;
        }

        int TokPrec = GetTokPrecedence();

        // Если этот бинарный оператор связывает выражения по крайней мере так же,
        // как текущий, то используем его
        if (TokPrec < ExprPrec)
            return LHS;

        // Отлично, мы знаем, что это бинарный оператор.
        int BinOp = CurTok;
        getNextToken();  // eat binop

        // Разобрать первичное выражение после бинарного оператора
        ExprAST *RHS = ParsePrimary(callFromCondition);
        if (!RHS) {
            delete LHS;
            return nullptr;
        }

        // Если BinOp связан с RHS меньшим приоритетом, чем оператор после RHS,
        // то берём часть вместе с RHS как LHS.
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, RHS, callFromCondition);
            if (RHS == nullptr) {
                delete LHS;
                return nullptr;
            }
        }

        // Собираем LHS/RHS.
        LHS = new BinaryExprAST(BinOp, LHS, RHS);
    }
}

ExprAST *IoTScenario::ParseExpression(bool callFromCondition) {
    ExprAST *LHS = ParsePrimary(callFromCondition);
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, LHS, callFromCondition);
}

void IoTScenario::clear() {
    deleteAll(_ifs);
    std::vector<ExprAST *>().swap(_ifs);
}

// разбираем файл сценария в дерево AST один раз, далее при событиях работаем только с деревом в памяти
void IoTScenario::loadScenario(const String &fileName) {
    if (_execDepth > 0) {  // нельзя удалять дерево, которое сейчас выполняется
        _pendingFileName = fileName;
        return;
    }

    clear();
    isIotScenException = false;
    _parseError = false;

    if (file) file.close();
    file = FileFS.open(fileName.c_str(), "r");
    if (!file) {
        Error(F("Open file scenario error"));
        return;
    }

    LastChar = ' ';
    curLine = 1;
    getNextToken();

    while (CurTok != tok_eof && !_parseError) {
        if (CurTok == tok_if) {
            _ifDepth = 0;
            _condHashes.clear();
            ExprAST *tmpAST = ParseIfExpr();
            if (tmpAST)
                _ifs.push_back(tmpAST);
            else if (!_parseError)
                getNextToken();
        } else
            getNextToken();
    }

    file.close();
    std::vector<uint32_t>().swap(_condHashes);
    _ifs.shrink_to_fit();

    if (_parseError)
        SerialPrint("E", F("Scenario"), "parsing stopped at line " + String(curLine) + ", loaded blocks: " + String(_ifs.size()));
    else
        SerialPrint("i", F("Scenario"), "loaded blocks: " + String(_ifs.size()) + ", free heap: " + String(ESP.getFreeHeap()));
}

void IoTScenario::exec(const String &eventIdName) {
    if (eventIdName == "") return;
    exec(hashFNV1a(eventIdName));
}

// выполняем только те блоки if, в условиях которых встречается ИД события
void IoTScenario::exec(uint32_t eventHash) {
    if (_ifs.empty() || eventHash == FNV1A_EMPTY_HASH) return;

    _execDepth++;
    isIotScenException = false;
    for (unsigned int i = 0; i < _ifs.size(); i++) {
        if (_ifs[i]->hasEventIdName(eventHash)) {
            _ifs[i]->exec();
            if (isIotScenException) break;  // exit() прекращает выполнение сценария для текущего события
        }
    }
    isIotScenException = false;
    _execDepth--;

    if (_execDepth == 0 && _pendingFileName != "") {
        String fileName = _pendingFileName;
        _pendingFileName = "";
        loadScenario(fileName);
    }
}

IoTScenario::IoTScenario() {}

IoTScenario::~IoTScenario() {
    clear();
    if (file) file.close();
}