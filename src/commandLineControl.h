#ifndef COMMAND_LINE_CONTROL_H
#define COMMAND_LINE_CONTROL_H

#include <stdlib.h>

void clearScreen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

#endif
