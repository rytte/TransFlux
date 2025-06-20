#ifndef TOOL_H
#define TOOL_H

#include <iostream>
#include <Logger.h>

void check_frame_type(uint8_t *data, int size);
void printH264Stream(const char* inputFile, int printSize);


#endif // TOOL_H
