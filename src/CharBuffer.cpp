#include "CharBuffer.h"
#include <cstdlib>

char* CharBuffer::_buffer = nullptr;
size_t CharBuffer::_bufferSize = 0;
SemaphoreHandle_t CharBuffer::_mutex = nullptr;

bool CharBuffer::initialize(size_t bufferSize)
{
    if (_buffer != nullptr && _mutex != nullptr)
    {
        return true;
    }

    _buffer = static_cast<char*>(malloc(bufferSize));
    if (_buffer == nullptr)
    {
        return false;
    }

    _buffer[0] = '\0';
    _bufferSize = bufferSize;

    _mutex = xSemaphoreCreateMutex();
    if (_mutex == nullptr)
    {
        free(_buffer);
        _buffer = nullptr;
        _bufferSize = 0;
        return false;
    }

    return true;
}

char* CharBuffer::acquire(TickType_t timeout)
{
    if (_buffer == nullptr || _mutex == nullptr)
    {
        return nullptr;
    }

    if (xSemaphoreTake(_mutex, timeout) != pdTRUE)
    {
        return nullptr;
    }

    _buffer[0] = '\0';
    return _buffer;
}

void CharBuffer::release()
{
    if (_mutex != nullptr)
    {
        xSemaphoreGive(_mutex);
    }
}

char* CharBuffer::get()
{
    return _buffer;
}

size_t CharBuffer::size()
{
    return _bufferSize;
}

CharBufferGuard::CharBufferGuard(TickType_t timeout)
    : _buffer(CharBuffer::acquire(timeout))
{
}

CharBufferGuard::~CharBufferGuard()
{
    if (_buffer != nullptr)
    {
        CharBuffer::release();
    }
}

CharBufferGuard::operator bool() const
{
    return _buffer != nullptr;
}

char* CharBufferGuard::get() const
{
    return _buffer;
}

size_t CharBufferGuard::size() const
{
    return CharBuffer::size();
}