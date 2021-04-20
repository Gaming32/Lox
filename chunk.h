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

    OP_PRINT,
    
    OP_RETURN,
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
