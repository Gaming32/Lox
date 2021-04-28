#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"
#include "utils.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-18s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int constantInstructionLong(const char* name, Chunk* chunk, int offset) {
    uint8_t constantA = chunk->code[offset + 1];
    uint8_t constantB = chunk->code[offset + 2];
    uint16_t constant = DECODE16BITS(constantA, constantB);
    printf("%-18s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-18s %4d\n", name, slot);
    return offset + 2; 
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = DECODE16BITS(chunk->code[offset + 1], chunk->code[offset + 2]);
    printf("%-18s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int closureInstruction(const char* name, uint16_t constant, Chunk* chunk, int offset) {
    printf("%-18s %4d ", "OP_CLOSURE", constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");

    ObjFunction* function = AS_FUNCTION(chunk->constants.values[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04d      |                       %s %d\n", offset - 2, isLocal ? "local" : "upvalue", index);
    }

    return offset;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG:
            return constantInstructionLong("OP_CONSTANT_LONG", chunk, offset);
        case OP_BYTE_NUM:
            return byteInstruction("OP_BYTE_NUM", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);

        case OP_EQ:
            return simpleInstruction("OP_EQ", offset);
        case OP_GT:
            return simpleInstruction("OP_GT", offset);
        case OP_LT:
            return simpleInstruction("OP_LT", offset);
        case OP_NEQ:
            return simpleInstruction("OP_NEQ", offset);
        case OP_GTE:
            return simpleInstruction("OP_GTE", offset);
        case OP_LTE:
            return simpleInstruction("OP_LTE", offset);

        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_SHIFT_LEFT:
            return simpleInstruction("OP_SHIFT_LEFT", offset);
        case OP_SHIFT_RIGHT:
            return simpleInstruction("OP_SHIFT_RIGHT", offset);
        case OP_BIT_AND:
            return simpleInstruction("OP_BIT_AND", offset);
        case OP_BIT_OR:
            return simpleInstruction("OP_BIT_OR", offset);
        case OP_BIT_XOR:
            return simpleInstruction("OP_BIT_XOR", offset);

        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_INVERT:
            return simpleInstruction("OP_INVERT", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);

        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL_LONG:
            return constantInstructionLong("OP_DEFINE_GLOBAL_LONG", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_GET_GLOBAL_LONG:
            return constantInstructionLong("OP_GET_GLOBAL_LONG", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL_LONG:
            return constantInstructionLong("OP_SET_GLOBAL_LONG", chunk, offset);

        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);

        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_GET_PROPERTY_LONG:
            return constantInstructionLong("OP_GET_PROPERTY_LONG", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY_LONG:
            return constantInstructionLong("OP_SET_PROPERTY_LONG", chunk, offset);

        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_BACKWARDS:
            return jumpInstruction("OP_JUMP_BACKWARDS", -1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_JUMP_IF_TRUE:
            return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset);

        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            return closureInstruction("OP_CLOSURE", (uint16_t)constant, chunk, offset);
        }
        case OP_CLOSURE_LONG: {
            offset += 3;
            uint16_t constant = DECODE16BITS(chunk->code[offset - 2], chunk->code[offset - 1]);
            return closureInstruction("OP_CLOSURE_LONG", constant, chunk, offset);
        }
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);

        case OP_CLASS:
            return constantInstruction("OP_CLASS", chunk, offset);
        case OP_CLASS_LONG:
            return constantInstructionLong("OP_CLASS_LONG", chunk, offset);
        case OP_METHOD:
            return constantInstruction("OP_METHOD", chunk, offset);
        case OP_METHOD_LONG:
            return constantInstructionLong("OP_METHOD_LONG", chunk, offset);

        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}
