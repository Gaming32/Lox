#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "utils.h"

#ifdef DEBUG_PRINT_CODE
    #include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_BIT_OR,      // |
    PREC_BIT_XOR,     // ^
    PREC_BIT_AND,     // &
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! - ~
    PREC_CALL,        // . () []
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef struct {
    int start;
    int breakStmts[UINT8_COUNT];
    int breakCount;
} Loop;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    Loop loops[UINT8_COUNT];
    Loop* loopTop;
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
    Table strings;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler* enclosing;
    Token name;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;

Compiler* current = NULL;

ClassCompiler* currentClass = NULL;

Chunk* compilingChunk;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLongBytes(uint8_t byte, uint16_t shortVal) {
    emitByte(byte);
    emitByte((uint8_t)((shortVal >> 8) & 0xff));
    emitByte((uint8_t)(shortVal & 0xff));
}

static void emitLoop(int loopStart) {
    emitByte(OP_JUMP_BACKWARDS);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitBytes((offset >> 8) & 0xff, offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
        emitByte(OP_RETURN);
    } else {
        emitByte(OP_RETURN_NIL);
    }
}

static uint16_t makeConstant(Value value) {
    int constant = -1;
    if (IS_STRING(value)) {
        Value destConstant;
        if (tableGet(&current->strings, AS_STRING(value), &destConstant)) {
            constant = AS_INT(destConstant);
        }
    }
    if (constant == -1) {
        constant = addConstant(currentChunk(), value);
        if (constant > UINT16_MAX) {
            error("Too many constants in one chunk. (max is 65536)");
            return 0;
        }
        if (IS_STRING(value)) {
            tableSet(&current->strings, AS_STRING(value), INT_VAL(constant));
        }
    }

    return (uint16_t)constant;
}

static void emitConstantOperator(uint16_t id, OpCode shortCode, OpCode longCode) {
    if (id <= UINT8_MAX) {
        emitBytes(shortCode, (uint8_t)id);
    } else {
        emitLongBytes(longCode, id);
    }
}

static void emitConstant(Value value) {
    uint16_t constant = makeConstant(value);
    emitConstantOperator(constant, OP_CONSTANT, OP_CONSTANT_LONG);
}

static void patchJump(int offset) {
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    initTable(&compiler->strings);
    compiler->loopTop = compiler->loops - 1;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler() {
    emitReturn();
    freeTable(&current->strings);
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
        printf("\n");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

static void function(FunctionType type);
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint16_t identifierConstant(Token* name) {
    uint16_t constant = makeConstant(OBJ_VAL(copyString(name->start, name->length)));
    return constant;
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variabls in function.");
        return;
    }

    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
}

static void declareVariable() {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();

            if (argCount == 255) {
                error("Can't have more that 255 arguments");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:      emitByte(OP_NEQ); break;
        case TOKEN_EQUAL_EQUAL:     emitByte(OP_EQ); break;
        case TOKEN_GREATER:         emitByte(OP_GT); break;
        case TOKEN_GREATER_EQUAL:   emitByte(OP_GTE); break;
        case TOKEN_LESS:            emitByte(OP_LT); break;
        case TOKEN_LESS_EQUAL:      emitByte(OP_LTE); break;

        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;

        case TOKEN_LESS_LESS:       emitByte(OP_SHIFT_LEFT); break;
        case TOKEN_GREATER_GREATER: emitByte(OP_SHIFT_RIGHT); break;
        case TOKEN_AMPERSAND:       emitByte(OP_BIT_AND); break;
        case TOKEN_PIPE:            emitByte(OP_BIT_OR); break;
        case TOKEN_CARET:           emitByte(OP_BIT_XOR); break;
        default:
            return;
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static uint8_t arrayArgList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET)) {
        do {
            expression();

            if (argCount == 255) {
                error("Can't have more that 255 arguments");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after arguments.");
    return argCount;
}

static void subscript(bool canAssign) {
    uint8_t argCount = arrayArgList();
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SUBSCRIPT_ASSIGN, argCount);
    } else {
        emitBytes(OP_SUBSCRIPT, argCount);
    }
}

static void array(bool canAssign) {
    uint8_t argCount = arrayArgList();
    emitBytes(OP_NEW_ARRAY, argCount);
}

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint16_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitConstantOperator(name, OP_SET_PROPERTY, OP_SET_PROPERTY_LONG);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        emitConstantOperator(name, OP_INVOKE, OP_INVOKE_LONG);
        emitByte(argCount);
    } else {
        emitConstantOperator(name, OP_GET_PROPERTY, OP_GET_PROPERTY_LONG);
    }
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL:   emitByte(OP_NIL); break;
        case TOKEN_TRUE:  emitByte(OP_TRUE); break;
        default:
            return;
    }
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

// static void traceVariableName(char** output) {
//     int nameLength = 0;
//     Compiler* trace = current;
//     while (trace != NULL) {
//         nameLength += trace->function->name == NULL ? 8 : trace->function->name->length;
//         nameLength++;
//         trace = trace->enclosing;
//     }
//     trace = current;
//     char* result = malloc(nameLength);
//     char* currentResult = result;
//     while (trace != NULL) {
//         if (trace->function->name == NULL) {
//             memcpy(currentResult, ">tpircs<", 8);
//             currentResult += 8;
//         } else {
//             int length = trace->function->name->length;
//             revmemcpy(currentResult, trace->function->name->chars, length);
//             currentResult += length;
//         }
//         trace = trace->enclosing;
//         if (trace != NULL) {
//             *currentResult++ = '.';
//         }
//     }
//     for (char *i = result, *j = currentResult - 1; i < j; i++, j--) {
//         char tmp = *i;
//         *i = *j;
//         *j = tmp;
//     }
//     *currentResult = '\0';
//     *output = result;
// }

static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void lambda(bool canAssign) {
    int parentNameLength = current->function->name == NULL ? 8 : current->function->name->length;
    char* name = malloc(parentNameLength + 13);
    memcpy(name, current->function->name == NULL ? "<script>" : current->function->name->chars, parentNameLength);
    memcpy(name + parentNameLength, ".<anonymous>", 12);
    name[parentNameLength + 12] = '\0';
    parser.previous = syntheticToken(name);
    function(TYPE_FUNCTION);
    free(name);
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    uint8_t truncated;
    if (value <= UINT8_MAX && (truncated = (uint8_t)value) == value) {
        emitBytes(OP_BYTE_NUM, truncated);
    } else { 
        emitConstant(NUMBER_VAL(value));
    }
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_TRUE);

    emitByte(OP_POP);
    parsePrecedence(PREC_OR);

    patchJump(endJump);
}

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitConstantOperator((uint16_t)arg, setOp, OP_SET_GLOBAL_LONG);
    } else {
        emitConstantOperator((uint16_t)arg, getOp, OP_GET_GLOBAL_LONG);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint16_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitConstantOperator(name, OP_SUPER_INVOKE, OP_SUPER_INVOKE_LONG);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emitConstantOperator(name, OP_GET_SUPER, OP_GET_SUPER_LONG);
    }
}

static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG:  emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_TILDE: emitByte(OP_INVERT); break;
        default:
            return;
    }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {grouping, call,      PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,      PREC_NONE},
    [TOKEN_LEFT_BRACKET]    = {array,    subscript, PREC_CALL}, 
    [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,      PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,     NULL,      PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,      PREC_NONE},
    [TOKEN_COMMA]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_DOT]             = {NULL,     dot,       PREC_CALL},
    [TOKEN_MINUS]           = {unary,    binary,    PREC_TERM},
    [TOKEN_PLUS]            = {NULL,     binary,    PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL,     NULL,      PREC_NONE},
    [TOKEN_SLASH]           = {NULL,     binary,    PREC_FACTOR},
    [TOKEN_STAR]            = {NULL,     binary,    PREC_FACTOR},
    [TOKEN_AMPERSAND]       = {NULL,     binary,    PREC_BIT_AND},
    [TOKEN_PIPE]            = {NULL,     binary,    PREC_BIT_OR},
    [TOKEN_CARET]           = {NULL,     binary,    PREC_BIT_XOR},
    [TOKEN_TILDE]           = {unary,    NULL,      PREC_UNARY},
    [TOKEN_BANG]            = {unary,    NULL,      PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,     binary,    PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,     binary,    PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,     binary,    PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,     binary,    PREC_COMPARISON},
    [TOKEN_GREATER_GREATER] = {NULL,     binary,    PREC_SHIFT},
    [TOKEN_LESS]            = {NULL,     binary,    PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,     binary,    PREC_COMPARISON},
    [TOKEN_LESS_LESS]       = {NULL,     binary,    PREC_SHIFT},
    [TOKEN_IDENTIFIER]      = {variable, NULL,      PREC_NONE},
    [TOKEN_STRING]          = {string,   NULL,      PREC_NONE},
    [TOKEN_NUMBER]          = {number,   NULL,      PREC_NONE},
    [TOKEN_AND]             = {NULL,     and_,      PREC_AND},
    [TOKEN_CLASS]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_ELSE]            = {NULL,     NULL,      PREC_NONE},
    [TOKEN_FALSE]           = {literal,  NULL,      PREC_NONE},
    [TOKEN_FOR]             = {NULL,     NULL,      PREC_NONE},
    [TOKEN_FUN]             = {lambda,   NULL,      PREC_NONE},
    [TOKEN_IF]              = {NULL,     NULL,      PREC_NONE},
    [TOKEN_NIL]             = {literal,  NULL,      PREC_NONE},
    [TOKEN_OR]              = {NULL,     or_,       PREC_OR},
    [TOKEN_PRINT]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_RETURN]          = {NULL,     NULL,      PREC_NONE},
    [TOKEN_SUPER]           = {super_,   NULL,      PREC_NONE},
    [TOKEN_THIS]            = {this_,    NULL,      PREC_NONE},
    [TOKEN_TRUE]            = {literal,  NULL,      PREC_NONE},
    [TOKEN_VAR]             = {NULL,     NULL,      PREC_NONE},
    [TOKEN_WHILE]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_BREAK]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_CONTINUE]        = {NULL,     NULL,      PREC_NONE},
    [TOKEN_ERROR]           = {NULL,     NULL,      PREC_NONE},
    [TOKEN_EOF]             = {NULL,     NULL,      PREC_NONE},
};

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static uint16_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint16_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitConstantOperator(global, OP_DEFINE_GLOBAL, OP_DEFINE_GLOBAL_LONG);
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more that 255 parameters.");
            }

            uint16_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = endCompiler();
    emitConstantOperator(makeConstant(OBJ_VAL(function)), OP_CLOSURE, OP_CLOSURE_LONG);

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method() {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint16_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);
    emitConstantOperator(constant, OP_METHOD, OP_METHOD_LONG);
}

static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint16_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitConstantOperator(nameConstant, OP_CLASS, OP_CLASS_LONG);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.name = parser.previous;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }

        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass) {
        endScope();
    }

    currentClass = currentClass->enclosing;
}

static void funDeclaration() {
    if (check(TOKEN_LEFT_PAREN)) {
        error("Can't have an anonmynous function expression statement");
        return;
    }
    uint16_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration() {
    uint16_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void pushLoop(int loopStart) {
    if (current->loopTop - current->loops == UINT8_MAX) {
        error("Too many nested loops in chunk");
        return;
    }
    current->loopTop++;
    current->loopTop->start = loopStart;
    current->loopTop->breakCount = 0;
}

static void popLoop() {
    for (int i = 0; i < current->loopTop->breakCount; i++) {
        patchJump(current->loopTop->breakStmts[i]);
    }
    if (current->loopTop == current->loops)
        return; // Prevent underflow on too many nested loops error
    current->loopTop--;
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);

        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }
    pushLoop(loopStart);

    statement();

    popLoop();
    emitLoop(loopStart);
    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

static void breakStatement() {
    if (current->loopTop < current->loops) { // No loops
        error("No loop to break out of.");
    }
    if (current->loopTop->breakCount) {
        error("Too many break statements in loop.");
        return;
    }
    current->loopTop->breakStmts[current->loopTop->breakCount++] = emitJump(OP_JUMP);
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
}

static void continueStatement() {
    if (current->loopTop < current->loops) { // No loops
        error("No loop to continue to top of.");
    }
    emitLoop(current->loopTop->start);
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement() {
    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        if (current->type == TYPE_SCRIPT) {
            error("Can't return value from top-level code.");
        } else if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() {
    int loopStart = currentChunk()->count;
    pushLoop(loopStart);

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    popLoop();
    patchJump(exitJump);
    emitByte(OP_POP);
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;

        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:;
        }

        advance();
    }
}

static void declaration() {
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_BREAK)) {
        breakStatement();
    } else if (match(TOKEN_CONTINUE)) {
        continueStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
