#pragma once
#include "Global.h"
#include "classes/IoTItem.h"

class ExprAST {
   public:
    virtual ~ExprAST();
    virtual IoTValue *exec();
    virtual int setValue(IoTValue *val, bool generateEvent);  // ret 0 - установка значения не поддерживается наследником
    virtual bool hasEventIdName(uint32_t eventHash);
};

// Сценарий разбирается в дерево AST один раз при загрузке (loadScenario) и хранится в памяти.
// При событии выполняются только те блоки if, в условиях которых встречается ИД элемента-источника события.
class IoTScenario {
    //===----------------------------------------------------------------------===//
    // Lexer (Лексический анализатор)
    //===----------------------------------------------------------------------===//

    String IdentifierStr;     // Заполняется, если tok_identifier или tok_string
    uint32_t IdentifierHash;  // хэш IdentifierStr, если tok_identifier
    String NumStr = "";       // Заполняется, если tok_number
    int LastChar;
    int curLine;

    /// gettok - Возвращает следующий токен из файла сценария.
    int gettok();

    //===----------------------------------------------------------------------===//
    // Parser (Парсер или Синтаксический Анализатор)
    //===----------------------------------------------------------------------===//

    /// CurTok/getNextToken - Предоставляет простой буфер токенов. CurTok - это текущий
    /// токен, просматриваемый парсером. getNextToken получает следующий токен от
    /// лексического анализатора и обновляет CurTok.
    int CurTok;
    int getNextToken();

    std::vector<uint32_t> _condHashes;  // накопитель хэшей идентификаторов, встречающихся в условиях текущего if верхнего уровня
    int _ifDepth = 0;                   // глубина вложенности разбираемых if
    bool _parseError = false;

    /// GetTokPrecedence - Возвращает приоритет текущего бинарного оператора.
    int GetTokPrecedence();

    /// Error* - Это небольшие вспомогательные функции для обработки ошибок.
    ExprAST *Error(const String &Str);

    /// identifierexpr
    ///   ::= identifier
    ///   ::= identifier '++' | identifier '--'
    ///   ::= identifier '(' expression* ')'
    ///   ::= identifier '.' identifier '(' expression* ')'
    ExprAST *ParseIdentifierExpr(bool callFromCondition);

    /// numberexpr ::= number
    ExprAST *ParseNumberExpr(bool negative = false);

    /// parenexpr ::= '(' expression ')'
    ExprAST *ParseParenExpr(bool callFromCondition);

    /// bracketsexpr ::= '{' expression* '}'
    ExprAST *ParseBracketsExpr(bool callFromCondition);

    /// quotesexpr ::= '"' string '"'
    ExprAST *ParseQuotesExpr();

    /// ifexpr ::= 'if' expression 'then' expression 'else' expression
    ExprAST *ParseIfExpr();

    /// primary
    ///   ::= identifierexpr | numberexpr | parenexpr | bracketsexpr | quotesexpr | ifexpr
    ///   ::= '-' primary | '!' primary
    ExprAST *ParsePrimary(bool callFromCondition);

    /// binoprhs
    ///   ::= ('+' primary)*
    ExprAST *ParseBinOpRHS(int ExprPrec, ExprAST *LHS, bool callFromCondition);

    /// expression
    ///   ::= primary binoprhs
    ExprAST *ParseExpression(bool callFromCondition);

    int getLastChar();
    fs::File file;

    std::vector<ExprAST *> _ifs;  // разобранные блоки if верхнего уровня

    int _execDepth = 0;           // защита от перезагрузки сценария во время его выполнения
    String _pendingFileName = ""; // отложенная загрузка, если loadScenario вызван во время exec

    void clear();

   public:
    void loadScenario(const String &fileName);
    void exec(const String &eventIdName);
    void exec(uint32_t eventHash);

    IoTScenario();
    ~IoTScenario();
};
