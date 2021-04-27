#ifndef clox_vm_h
#define clox_vm_h

#include <stdarg.h>

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    bool hasError;

    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Chunk* chunk;
    uint8_t* ip;
    Value* stack;
    Value* stackTop;
    Table globals;
    Table strings;
    ObjUpvalue* openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;

    Obj* objects;
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void vruntimeError(const char* format, va_list args);
void runtimeError(const char* format, ...);
void defineNative(const char* name, NativeFn function);
ObjString* toString(Value value);
void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
bool getProperty(Value obj, ObjString* name, Value* result);

#endif
