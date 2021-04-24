#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include "utils.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

int stringifyValue(char** result, Value value) {
    switch (value.type) {
        case VAL_BOOL:
            return asprintf(result, AS_BOOL(value) ? "true" : "false");
        case VAL_NIL: return asprintf(result, "nil");
        case VAL_NUMBER: return asprintf(result, "%g", AS_NUMBER(value));
        case VAL_OBJ: return stringifyObject(result, value);
        case VAL_INT: return asprintf(result, "<internal int %d>", AS_INT(value));
    }
    return -1;
}

void printValue(Value value) {
    char* result;
    int length = stringifyValue(&result, value);
    if (length != -1) {
        printf("%s", result);
        free(result);
    }
}

bool valuesEqual(Value a, Value b) {
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
        default:
            return false;
    }
}

bool valuesNotEqual(Value a, Value b) {
    if (a.type != b.type) return true;

    switch (a.type) {
        case VAL_BOOL:   return AS_BOOL(a) != AS_BOOL(b);
        case VAL_NIL:    return false;
        case VAL_NUMBER: return AS_NUMBER(a) != AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) != AS_OBJ(b);
        default:
            return true;
    }
}
