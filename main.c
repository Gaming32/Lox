#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        char errorMessage[2048];
        sprintf(errorMessage, "Could not open file \"%s\"", path);
        perror(errorMessage);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read file \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    fclose(file);
    return buffer;
}

static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char* argv[]) {
    initVM();

#ifdef DEBUG_STATISTICS
    printf("CallFrame size: %lu\n", (unsigned long)sizeof(CallFrame));
    printf("Value size: %lu\n", (unsigned long)sizeof(Value));
    printf("VM size: %lu\n\n", (unsigned long)sizeof(VM));

    printf("vm size (on C stack): %lu\n", (unsigned long)sizeof(VM));
    printf("vm.frames size (on C stack): %lu\n", (unsigned long)sizeof(vm.frames));
    printf("vm.stack size (on C heap): %lu\n\n", (unsigned long)(STACK_MAX * sizeof(Value)));
#endif

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}
