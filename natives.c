#include <time.h>

#include "natives.h"
#include "object.h"
#include "value.h"
#include "utils.h"

static Value funHas(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 2);
    Value fieldValue = args[1];
    ObjString* field;
    if (!IS_STRING(fieldValue)) {
        return BOOL_VAL(false);
    }
    Value value = args[0];
    field = AS_STRING(fieldValue);
    if (IS_INSTANCE(value)) {
        Value ignored;
        return BOOL_VAL(tableGet(&AS_INSTANCE(value)->fields, field, &ignored));
    }
    return BOOL_VAL(false);
}

static Value funGet(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 2);
    Value fieldValue = args[1];
    ObjString* field;
    if (!IS_STRING(fieldValue)) {
        runtimeError("Cannot have non-string property of object");
        return NULL_VAL;
    }
    Value value = args[0];
    field = AS_STRING(fieldValue);
    Value result;
    if (!getProperty(value, field, &result)) {
        return ERR_PROPERTY(field, value);
    }
    return result;
}

static Value funSet(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 3);
    Value fieldValue = args[1];
    ObjString* field;
    if (!IS_STRING(fieldValue)) {
        runtimeError("Cannot have non-string property of object");
        return NULL_VAL;
    }
    Value value = args[0];
    field = AS_STRING(fieldValue);
    Value newValue = args[2];
    if (IS_INSTANCE(value)) {
        tableSet(&AS_INSTANCE(value)->fields, field, newValue);
        return NIL_VAL;
    }
    runtimeError("Only instances have fields.");
    return NULL_VAL;
}

static Value funSize(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 1);
    Value obj = args[0];
    if (!IS_OBJ(obj) || IS_NULL(obj)) {
        goto error;
    }
    switch (OBJ_TYPE(obj)) {
        case OBJ_STRING:
            return NUMBER_VAL(AS_STRING(obj)->length);

        case OBJ_ARRAY:
            return NUMBER_VAL(AS_ARRAY(obj).count);

        default:
            // Unsupported type
            break;
    }
    error:
    runtimeError("Only strings, arrays, and tables have size/length");
    return NULL_VAL;
}

static Value funGetTypeName(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 1);
    char* result;
    int length;
    switch (args->type) {
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
                case OBJ_CLASS:    length = asprintf(&result, "class"); break;
                case OBJ_CLOSURE:  length = asprintf(&result, "closure"); break;
                case OBJ_FUNCTION: length = asprintf(&result, "function"); break;
                case OBJ_INSTANCE: length = asprintf(&result, "%s", AS_INSTANCE(args[0])->klass->name->chars); break;
                case OBJ_NATIVE:   length = asprintf(&result, "native"); break;
                case OBJ_UPVALUE:  length = asprintf(&result, "upvalue"); break;
                case OBJ_STRING:   length = asprintf(&result, "string"); break;
                case OBJ_ARRAY:    length = asprintf(&result, "array"); break;
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
    return OBJ_VAL(toString(args[0]));
}

static Value funClock(int argCount, Value* args) {
    EXPECT_ARGS(argCount, 0);
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

void initializeNatives() {
    // Manage properties
    defineNative("has", funHas);
    defineNative("get", funGet);
    defineNative("set", funSet);

    // Array/string tools
    defineNative("size", funSize);

    // General tools
    defineNative("getTypeName", funGetTypeName);
    defineNative("toString", funToString);
    defineNative("clock", funClock);
}
