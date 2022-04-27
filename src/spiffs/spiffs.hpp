#pragma once
#include <FS.h>

void writeFile(fs::FS &fs, const char *path, const char *message);
String readFile(fs::FS &fs, const char *path);
void initSPIFFS();
