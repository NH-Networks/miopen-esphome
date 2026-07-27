#ifndef FILESYSTEMHELPERS_H
#define FILESYSTEMHELPERS_H

#include <string>
#include <LittleFS.h>

#if defined(ESP32)
    #include <FS.h>
#endif

void listFS();
void cat(const char *fname);
void rm(const char *fname);
#endif
