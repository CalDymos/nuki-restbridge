#pragma once

#ifdef DEBUG
#include <Print.h>
extern Print* Log;
#else
#include "WString.h"
class EmptyPrint
{
private:
protected:

public:
    EmptyPrint()
    {
    }
    virtual ~EmptyPrint() {}
  
    void printf(const char * format, ...){}

    // add availableForWrite to make compatible with Arduino Print.h
    // default to zero, meaning "a single write may block"
    // should be overriden by subclasses with buffering
    void print(const String &){}
    void print(const char[]){}
    void print(char){}
    void print(unsigned char, int = 10){}
    void print(int, int = 10){}
    void print(unsigned int, int = 10){}
    void print(long, int = 10){}
    void print(unsigned long, int = 10){}
    void print(long long, int = 10){}
    void print(unsigned long long, int = 10){}
    void print(double, int = 2){}

    void println(const String &s){}
    void println(const char[]){}
    void println(char){}
    void println(unsigned char, int = 10){}
    void println(int, int = 10){}
    void println(unsigned int, int = 10){}
    void println(long, int = 10){}
    void println(unsigned long, int = 10){}
    void println(long long, int = 10){}
    void println(unsigned long long, int = 10){}
    void println(double, int = 2){}
    void println(void){}
    
};
extern EmptyPrint* Log;
#endif


