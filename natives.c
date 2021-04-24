#include <time.h>

#include "natives.h"
#include "object.h"
#include "value.h"
#include "utils.h"

static Value funGetTypeName(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 1);
    char* result;
    int length;
    switch (args[0].type) {
        case VAL_BOOL:   length = asprintf(&result, "boolean"); break;
        case VAL_NUMBER: length = asprintf(&result, "number"); break;
        case VAL_NIL:    length = asprintf(&result, "nil"); break;
        case VAL_INT:    length = asprintf(&result, "int"); break;
        case VAL_OBJ: {
            if (IS_NULL(args[0])) {
                length = asprintf(&result, "null");
                break;
            }
            switch (OBJ_TYPE(args[0])) {
                case OBJ_CLOSURE:  length = asprintf(&result, "closure"); break;
                case OBJ_FUNCTION: length = asprintf(&result, "function"); break;
                case OBJ_NATIVE:   length = asprintf(&result, "native"); break;
                case OBJ_STRING:   length = asprintf(&result, "string"); break;
                default:           length = asprintf(&result, "object"); break;
            }
            break;
        }
        default: length = asprintf(&result, "value"); break;
    }
    return OBJ_VAL(takeString(result, length - 1));
}

static Value funToString(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 1);
    char* result;
    int length = stringifyValue(&result, args[0]);
    return OBJ_VAL(takeString(result, length));
}

static Value funClock(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 0);
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

void initializeNatives() {
    defineNative("getTypeName", funGetTypeName);
    defineNative("toString", funToString);
    defineNative("clock", funClock);
}
