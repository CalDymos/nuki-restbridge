#pragma once

class CharBuffer
{
public:
    static void initialize(char16_t buffer_size)
    {
        _buffer = new char[buffer_size];
    }

    static char* get()
    {
        return _buffer;
    }

private:
    static char* _buffer;
};

// Definition der statischen Variable
char* CharBuffer::_buffer = nullptr;
