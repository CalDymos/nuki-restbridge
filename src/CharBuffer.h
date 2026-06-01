#pragma once

#include <cstddef>
#include <new>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @brief Static utility class for managing a shared character buffer.
 *
 * CharBuffer provides a single global character buffer that can be initialized
 * once and shared between different components. Access to the buffer can be
 * protected by an internal FreeRTOS mutex through acquire() and release().
 */
class CharBuffer
{
public:
    /**
     * @brief Initializes the shared character buffer and its mutex.
     *
     * Allocates the internal character buffer with the requested size and
     * creates the FreeRTOS mutex used to synchronize access.
     *
     * @param bufferSize Size of the buffer to allocate in bytes.
     * @return true if the buffer and mutex were initialized successfully.
     * @return false if memory allocation or mutex creation failed.
     */
    static bool initialize(size_t bufferSize);

    /**
     * @brief Acquires exclusive access to the shared buffer.
     *
     * Locks the internal mutex and returns the shared buffer pointer when the
     * mutex was acquired successfully. The caller must release the buffer by
     * calling release() after the protected operation is complete.
     *
     * @param timeout Maximum time to wait for the mutex.
     * @return Pointer to the shared buffer, or nullptr if the mutex could not be acquired.
     */
    static char* acquire(TickType_t timeout = pdMS_TO_TICKS(200));

    /**
     * @brief Releases exclusive access to the shared buffer.
     *
     * Unlocks the internal mutex after a successful acquire() call.
     */
    static void release();

    /**
     * @brief Returns the raw shared buffer pointer without locking.
     *
     * This method is intended only for legacy or initialization paths where
     * synchronized buffer access is not required.
     *
     * @return Pointer to the shared buffer, or nullptr if it is not initialized.
     */
    static char* get();

    /**
     * @brief Returns the configured size of the shared buffer.
     *
     * @return Size of the shared buffer in bytes.
     */
    static size_t size();

private:
    /**
     * @brief Pointer to the shared character buffer.
     */
    static char* _buffer;

    /**
     * @brief Size of the shared character buffer in bytes.
     */
    static size_t _bufferSize;

    /**
     * @brief Mutex used to synchronize access to the shared buffer.
     */
    static SemaphoreHandle_t _mutex;
};


/**
 * @brief RAII guard for exclusive access to the shared CharBuffer.
 *
 * CharBufferGuard acquires the shared CharBuffer in its constructor and
 * automatically releases it in its destructor. This prevents missing release()
 * calls when a function exits early.
 */
class CharBufferGuard
{
public:
    /**
     * @brief Acquires the shared CharBuffer.
     *
     * Attempts to lock the shared buffer mutex and stores the acquired buffer
     * pointer if successful.
     *
     * @param timeout Maximum time to wait for the shared buffer mutex.
     */
    explicit CharBufferGuard(TickType_t timeout = pdMS_TO_TICKS(200));

    /**
     * @brief Releases the shared CharBuffer if it was acquired.
     *
     * Automatically unlocks the shared buffer mutex when the guard goes out of
     * scope.
     */
    ~CharBufferGuard();

    /**
     * @brief Deleted copy constructor.
     *
     * Prevents copying the guard because ownership of the locked buffer must
     * remain unique.
     */
    CharBufferGuard(const CharBufferGuard&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     *
     * Prevents assigning one guard to another because ownership of the locked
     * buffer must remain unique.
     *
     * @return This operator is deleted and cannot be used.
     */
    CharBufferGuard& operator=(const CharBufferGuard&) = delete;

    /**
     * @brief Checks whether the shared buffer was acquired successfully.
     *
     * @return true if the guard owns a valid buffer lock.
     * @return false if the buffer could not be acquired.
     */
    explicit operator bool() const;

    /**
     * @brief Returns the acquired shared buffer pointer.
     *
     * @return Pointer to the shared buffer, or nullptr if acquisition failed.
     */
    char* get() const;

    /**
     * @brief Returns the size of the acquired shared buffer.
     *
     * @return Size of the shared buffer in bytes.
     */
    size_t size() const;

private:
    /**
     * @brief Pointer to the acquired shared buffer.
     */
    char* _buffer;
};