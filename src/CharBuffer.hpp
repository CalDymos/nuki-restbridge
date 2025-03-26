#pragma once

/**
 * @brief A static utility class for managing a shared character buffer.
 *
 * CharBuffer provides a statically allocated character buffer that can be
 * initialized once and accessed globally via static methods.
 */
class CharBuffer
{
public:
    /**
     * @brief Initializes the internal static character buffer.
     *
     * Allocates a new character array of the specified size.
     * This buffer remains allocated until the program ends.
     *
     * @param buffer_size The size of the buffer to allocate.
     */
    static void initialize(char16_t buffer_size)
    {
        _buffer = new char[buffer_size];
    }

    /**
     * @brief Returns a pointer to the internal static buffer.
     *
     * @return Pointer to the internal character buffer.
     */
    static char *get()
    {
        return _buffer;
    }

private:
    static char *_buffer; // Pointer to the statically allocated buffer.
};

// definition of static var
char *CharBuffer::_buffer = nullptr;
