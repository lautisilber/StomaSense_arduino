#include "pico/mutex.h"
#include <cstddef>
#include <sys/_stdint.h>
#ifndef _FIFO_STACK_H_
#define _FIFO_STACK_H_

#ifdef ARDUINO_ARCH_RP2040
#include <pico/sync.h>
#endif

template <typename T, size_t N>
class FIFOStack
{
private:
    T _data[N] = {0};
    size_t _curr_size = 0;

public:
    bool push(const T &entry);
    bool peek(T &entry) const;
    bool pop(T &entry);
    inline size_t size() const;
    inline bool available() const;
    inline void clear();
};

template <typename T, size_t N>
inline size_t FIFOStack<T, N>::size() const { return _curr_size; }
template <typename T, size_t N>
inline bool FIFOStack<T, N>::available() const { return size() > 0; }
template <typename T, size_t N>
inline void FIFOStack<T, N>::clear() { _curr_size = 0; }

template <typename T, size_t N>
bool FIFOStack<T, N>::push(const T &entry)
{
    if (_curr_size == N) return false;
    memcpy(_data[_curr_size++], &entry, sizeof(T));
    return true;
}

template <typename T, size_t N>
bool FIFOStack<T, N>::peek(T &entry) const
{
    if (!available()) return false;
    memcpy(&entry, &_data[_curr_size], sizeof(T));
    return true;
}

template <typename T, size_t N>
bool FIFOStack<T, N>::pop(T &entry)
{
    if (!pop(entry)) return false;
    --_curr_size;
    return true;
}

#define FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US 5000000
template <typename T, size_t N>
class FIFOStackThreadSafeBase : protected FIFOStack<T, N>
{
protected:
    virtual inline void _get_mutex_blocking() = 0;
    virtual inline void _release_mutex() = 0;
    virtual inline bool _get_mutex_timeout(uint32_t timeout_us) = 0;

public:
    // blocking methods
    bool push_blocking(const T &entry)
    {
        _get_mutex_blocking();
        bool res = FIFOStack<T, N>::push(entry);
        _release_mutex();
        return res;
    }
    bool peek_blocking(T &entry)
    {
        _get_mutex_blocking();
        bool res = FIFOStack<T, N>::peek(entry);
        _release_mutex();
        return res;
    }
    bool pop_blocking(T &entry)
    {
        _get_mutex_blocking();
        bool res = FIFOStack<T, N>::pop(entry);
        _release_mutex();
        return res;
    }
    inline size_t size_blocking()
    {
        _get_mutex_blocking();
        size_t res = FIFOStack<T, N>::size();
        _release_mutex();
        return res;
    }
    inline bool available_blocking()
    {
        _get_mutex_blocking();
        bool res = FIFOStack<T, N>::available();
        _release_mutex();
        return res;
    }
    inline void clear_blocking()
    {
        _get_mutex_blocking();
        FIFOStack<T, N>::clear();
        _release_mutex();
    }

    // timeout methods
    bool push_timeout(const T &entry, bool &success, uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        success = FIFOStack<T, N>::push(entry);
        _release_mutex();
        return true;
    }
    bool peek_timeout(const T *entry, uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        entry = FIFOStack<T, N>::peek();
        _release_mutex();
        return true;
    }
    bool pop_timeout(T &entry, uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        bool res = FIFOStack<T, N>::pop(entry);
        _release_mutex();
        return res;
    }
    inline bool size_timeout(size_t &size, uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        size = FIFOStack<T, N>::size();
        _release_mutex();
        return true;
    }
    inline bool available_timeout(bool &available, uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        available = FIFOStack<T, N>::available();
        _release_mutex();
        return true;
    }
    inline bool clear_timeout(uint32_t timeout_us=FIFO_STACK_THREAD_SAFE_DEFAULT_TIMEOUT_US)
    {
        if (!_get_mutex_timeout(timeout_us))
            return false;
        FIFOStack<T, N>::clear();
        _release_mutex();
        return true;
    }
};

#ifdef ARDUINO_ARCH_RP2040
template <typename T, size_t N>
class FIFOStackThreadSafeRP2040 : public FIFOStackThreadSafeBase<T, N>
{
private:
    mutex_t _mutex;

public:
    FIFOStackThreadSafeRP2040()
    {
        mutex_init(&_mutex);
    }

protected:
    inline void _get_mutex_blocking()
    {
        mutex_enter_blocking(&_mutex);
    }
    inline void _release_mutex()
    {
        mutex_exit(&_mutex);
    }
    inline bool _get_mutex_timeout(uint32_t timeout_us)
    {
        return  mutex_enter_timeout_us(&_mutex, timeout_us);
    }
};
#endif

#endif /* _FIFO_STACK_H_ */
