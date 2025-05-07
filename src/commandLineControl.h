#ifndef COMMAND_LINE_CONTROL_H
#define COMMAND_LINE_CONTROL_H

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

struct argument {
    char *flag;
    char *value;
    int isInt;
    int intValue;
};
typedef struct argument Argument;


void printArg(Argument *arg) {
    if (arg == NULL) return;
    printf("------------------------\nArgument\n");
    if (arg->flag != NULL) printf("Flag: \"%s\"\n", arg->flag);
    if (arg->value != NULL) printf("Value: \"%s\"\n", arg->value);
    printf("Is Int?: \"%s\"\n", (arg->isInt ? "True" : "False"));
    printf("Int: %d\n\n", arg->intValue);
}

void freeArguments(Argument **args) {
    if (args == NULL) return;
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]->flag);
        free(args[i]->value);
        free(args[i]);
    }
    free(args);
}

int isInteger(const char *str) {
    if (str == NULL || *str == '\0') return 0;
    if (*str == '-' || *str == '+') str++;
    
    if (*str == '0' && tolower((unsigned char)str[1]) == 'x') {
        str += 2;
        if (*str == '\0') return 0;
        while (*str) {
            if (!isxdigit((unsigned char)*str)) return 0;
            str++;
        }
        return 1;
    }
    while (*str) {
        if (!isdigit((unsigned char)*str)) return 0;
        str++;
    }
    return 1;
}

void cstringToLower(char *s) {
    if (s == NULL) return;
    for (int i = 0; s[i]; i++)
        s[i] = tolower((unsigned char)s[i]);
}

int convertIntValue(Argument *arg) {
    if (arg == NULL || arg->value == NULL) return 0;
    if (!arg->isInt) return 0;

    char *value = arg->value;
    int base = 10;
    if (strlen(value) > 1 && value[0] == '0' && tolower((unsigned char)value[1]) == 'x')
        base = 16;

    return (int)strtol(value, NULL, base);
}

Argument **getArgs(int argc, char *argv[]) {
    // 3 min values (execute, flag, flag arg) for argc or else 0 arguments
    if (argc < 3 || argc % 2 == 0) return NULL;
    int count = (argc - 1) / 2;
    Argument **list = (Argument**)calloc(count, sizeof(Argument*));
    if (list == NULL) return NULL;
    
    int argIndex = 0;
    for (int i = 1; i < argc && i + 1 < argc; i += 2) {
        Argument *current = (Argument*)malloc(sizeof(Argument));
        if (current == NULL) {
            list[argIndex] = NULL;
            freeArguments(list);
            return NULL;
        }
        current->flag = strdup(argv[i]);
        cstringToLower(current->flag);
        current->value = strdup(argv[i + 1]);
        current->isInt = isInteger(current->value);
        current->intValue = convertIntValue(current);

        list[argIndex++] = current;
    }
    return list;
}

int countArg(Argument **args) {
    if (args == NULL) return 0;
    int count = 0;
    for (; args[count] != NULL; count++) { }
    return count;
}

int compareFlag(Argument *arg, const char *flag) {
    return arg->flag != NULL && strcmp(arg->flag, flag) == 0;
}

Argument *tryGetArg(Argument **args, const char *arg) {
    if (args == NULL) return NULL;
    for (int i = 0; args[i] != NULL; i++) {
        if (compareFlag(args[i], arg))
            return args[i];
    }
    return NULL;
}

int setValue(Argument **args, const char *arg, int *value) {
    Argument *temp = tryGetArg(args, arg);
    if (temp == NULL) return 0;
    if (!temp->isInt) return -1;
    *value = temp->intValue;
    return 1;
}

#endif
