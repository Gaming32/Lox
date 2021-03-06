#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_BYTE_NUM,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,

    OP_EQ,
    OP_GT,
    OP_LT,
    OP_NEQ,
    OP_GTE,
    OP_LTE,

    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_SHIFT_LEFT,
    OP_SHIFT_RIGHT,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_BIT_AND,

    OP_NEGATE,
    OP_INVERT,
    OP_NOT,

    OP_DEFINE_GLOBAL,
    OP_DEFINE_GLOBAL_LONG,
    OP_GET_GLOBAL,
    OP_GET_GLOBAL_LONG,
    OP_SET_GLOBAL,
    OP_SET_GLOBAL_LONG,

    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,

    OP_GET_PROPERTY,
    OP_GET_PROPERTY_LONG,
    OP_SET_PROPERTY,
    OP_SET_PROPERTY_LONG,

    OP_GET_SUPER,
    OP_GET_SUPER_LONG,
    OP_SUPER_INVOKE,
    OP_SUPER_INVOKE_LONG,

    OP_JUMP,
    OP_JUMP_BACKWARDS,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,

    OP_CALL,
    OP_INVOKE,
    OP_INVOKE_LONG,
    OP_CLOSURE,
    OP_CLOSURE_LONG,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_RETURN_NIL,

    OP_SUBSCRIPT,
    OP_SUBSCRIPT_ASSIGN,
    OP_NEW_ARRAY,

    OP_CLASS,
    OP_CLASS_LONG,
    OP_INHERIT,
    OP_METHOD,
    OP_METHOD_LONG,

    OP_PRINT,
    OP_POP,
} OpCode;

typedef struct {
    int count;
    int capacity;
    uint8_t* code;
    ValueArray constants;
    int* lines;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);

#endif
