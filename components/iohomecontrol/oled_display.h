#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdint.h>
#include <string>

inline bool initDisplay() { return true; }
inline void display1WAction(const uint8_t *, const char *, const char *, const char * = nullptr) {}
inline void display1WPosition(const uint8_t *, float, const char * = nullptr) {}
inline void updateDisplayStatus() {}
inline void displayCustomMessage(const char*, const char* = nullptr) {}
inline void clearDisplayMessages() {}
inline bool isDisplayEnabled() { return false; }
inline void setDisplayEnabled(bool) {}

template<typename ... Args>
std::string format(const std::string &formatStr, Args ... args)
{
    char buf[250];
    snprintf(buf, 250, formatStr.c_str(), args ...);
    return std::string(buf);
}

#endif
