#ifndef UTILS_H
#define UTILS_H

#include <string.h>

#define DEL 127

bool isNumeric(String str);

void flushInputBuffer();

String getUserInput(String prompt);

#endif