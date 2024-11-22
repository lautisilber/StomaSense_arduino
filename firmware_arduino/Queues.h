#ifndef _QUEUES_H_
#define _QUEUES_H_

#include <Arduino.h>

template <typename T, size_t N>
class DequeBase
{
private:
    T _data[N];
    size_t _front = 0;
    size_t _back = 0;
    size_t _size = 0;

    const bool _overwrite;

protected:
    DequeBase(bool overwrite)
    : _overwrite(overwrite)
    {
        static_assert(N > 0, "Can't have an empty buffer");
    }

    bool push_back(const T *const entry)
    {
        if (empty())
        {
            // because N > 0
            memcpy(_data + _front, entry, sizeof(T));
            ++_size;
            return true;
        }

        size_t temp = _back < N - 1 ? _back + 1 : 0;

        if (full())
        {
            if (_overwrite)
            {
                _front = temp;
            }
            else
                return false;
        }

        // populate
        _back = temp;
        memcpy(_data + _back, entry, sizeof(T));
        if (_size < N) ++_size;
        return true;
    }

    bool push_front(const T *const entry)
    {
        if (empty())
        {
            // because N > 0
            memcpy(_data + _front, entry, sizeof(T));
            ++_size;
            return true;
        }

        size_t temp = _front > 0 ? _front-1 : N-1;

        if (full())
        {
            if (_overwrite)
            {
                _back = temp;
            }
            else
                return false;
        }

        // populate
        _front = temp;
        memcpy(_data + _front, entry, sizeof(T));
        if (_size < N) ++_size;
        return true;
    }

    bool pop_back(T *entry)
    {
        if (empty()) return false;

        memcpy(entry, _data + _back, sizeof(T));
        _back = _back > 0 ? _back - 1 : N-1;
        --_size;
        return true;
    }

    bool pop_front(T *entry)
    {
        if (empty()) return false;

        memcpy(entry, _data + _front, sizeof(T));
        _front = _front < N - 1 ? _front + 1 : 0;
        --_size;
        return true;
    }

    const T *peek_back() const
    {
        return _data + _back;
    }

    const T *peek_front() const
    {
        return _data + _front;
    }

    const T *peek(size_t i) const
    {
        if (i >= size()) return NULL;
        size_t temp = _front + i;
        if (temp >= N)
        {
            temp -= N;
        }
        return _data + temp;
    }

public:
    inline size_t buffer_size() const { return N; }
    inline size_t size() const { return _size; }
    inline bool empty() const { return size() == 0; }
    inline bool full() const { return size() == N; }
    inline void clean() { _back = _front; }
};

template <typename T, size_t N>
class Deque : public DequeBase<T, N>
{
public:
    Deque(bool overwrite): DequeBase<T, N>(overwrite) {}

    inline bool push_back(const T *const entry) { return DequeBase<T, N>::push_back(entry); }
    inline bool push_front(const T *const entry) { return DequeBase<T, N>::push_front(entry); }
    
    inline bool pop_back(T *entry) { return DequeBase<T, N>::pop_back(entry); }
    inline bool pop_front(T *entry) { return DequeBase<T, N>::pop_front(entry); }
    
    inline const T *peek_back() const { return DequeBase<T, N>::peek_back(); }
    inline const T *peek_front() const { return DequeBase<T, N>::peek_front(); }
    inline const T *peek(size_t i) const { return DequeBase<T, N>::peek(i); }
};

template <typename T, size_t N>
class QueueFIFO : public DequeBase<T, N>
{
public:
    QueueFIFO(bool overwrite): DequeBase<T, N>(overwrite) {}

    inline bool push(const T *const entry) { return DequeBase<T, N>::push_front(entry); }
    inline bool pop(T *entry) { return DequeBase<T, N>::pop_back(entry); }
    inline const T *peek() const { return DequeBase<T, N>::peek_back(); }
};

template <typename T, size_t N>
class StackLIFO : public DequeBase<T, N>
{
public:
    StackLIFO(bool overwrite): DequeBase<T, N>(overwrite) {}

    inline bool push(const T *const entry) { return DequeBase<T, N>::push_back(entry); }
    inline bool pop(T *entry) { return DequeBase<T, N>::pop_back(entry); }
    inline const T *peek() const { return DequeBase<T, N>::peek_back(); }
};

template <typename T, size_t N>
class DequeBaseThreadSafe : protected DequeBase<T, N>
{
protected:
    virtual inline void get_mutex_blocking() = 0;
    virtual inline void release_mutex() = 0;
    virtual inline bool get_mutex_timeout(uint32_t timeout_us) = 0;

    bool push_back_blocking(const T *const entry)
    {
        get_mutex_blocking();
        bool res = DequeBase<T, N>::push_back(entry);
        release_mutex();
        return res;
    }
    bool push_back_timeout(const T *const entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        bool res = DequeBase<T, N>::push_back(entry);
        release_mutex();
        return res;
    }
    bool push_front_blocking(const T *const entry)
    {
        get_mutex_blocking();
        bool res = DequeBase<T, N>::push_front(entry);
        release_mutex();
        return res;
    }
    bool push_front_timeout(const T *const entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        bool res = DequeBase<T, N>::push_front(entry);
        release_mutex();
        return res;
    }

    bool pop_back_blocking(T *entry)
    {
        get_mutex_blocking();
        bool res = DequeBase<T, N>::pop_back(entry);
        release_mutex();
        return res;
    }
    bool pop_back_timeout(T *entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        bool res = DequeBase<T, N>::pop_back(entry);
        release_mutex();
        return res;
    }
    bool pop_front_blocking(T *entry)
    {
        get_mutex_blocking();
        bool res = DequeBase<T, N>::pop_front(entry);
        release_mutex();
        return res;
    }
    bool pop_front_timeout(T *entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        bool res = DequeBase<T, N>::pop_front(entry);
        release_mutex();
        return res;
    }

    const T *peek_back_blocking()
    {
        get_mutex_blocking();
        const T *res = DequeBase<T, N>::peek_back();
        release_mutex();
        return res;
    }
    bool peek_back_timeout(const T *entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        entry = DequeBase<T, N>::peek_back();
        release_mutex();
        return true;
    }

    const T *peek_front_blocking()
    {
        get_mutex_blocking();
        const T *res = DequeBase<T, N>::peek_front();
        release_mutex();
        return res;
    }
    bool peek_front_timeout(const T *entry, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        entry = DequeBase<T, N>::peek_front();
        release_mutex();
        return true;
    }

public:
    inline size_t size_blocking()
    {
        get_mutex_blocking();
        size_t res = DequeBase<T, N>::size();
        release_mutex();
        return res;
    }
    inline bool size_timeout(size_t *s, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        *s = DequeBase<T, N>::size();
        release_mutex();
        return true;
    }
    inline bool empty_blocking()
    {
        get_mutex_blocking();
        size_t res = DequeBase<T, N>::empty();
        release_mutex();
        return res;
    }
    inline bool empty_timeout(bool *is_empty, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        *is_empty = DequeBase<T, N>::empty();
        release_mutex();
        return true;
    }
    inline bool full_blocking()
    {
        get_mutex_blocking();
        size_t res = DequeBase<T, N>::full();
        release_mutex();
        return res;
    }
    inline bool full_timeout(bool *is_full, uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        *is_full = DequeBase<T, N>::full();
        release_mutex();
        return true;
    }
    inline void clean_blocking()
    {
        get_mutex_blocking();
        DequeBase<T, N>::clean();
        release_mutex();
    }
    inline bool clean_timeout(uint32_t timeout_us)
    {
        if (!get_mutex_timeout(timeout_us)) return false;
        DequeBase<T, N>::full();
        release_mutex();
        return true;
    }
};

template <typename T, size_t N>
class DequeThreadSafe : protected DequeBaseThreadSafe<T, N>
{
public:
    DequeThreadSafe(bool overwrite) : DequeBaseThreadSafe<T, N>(overwrite)
    {}

    bool push_back_blocking(const T *const entry) { return DequeBaseThreadSafe<T, N>::push_back_blocking(entry); }
    bool push_back_timeout(const T *const entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::push_back_timeout(entry, timeout_us); }
    bool push_front_blocking(const T *const entry) { return DequeBaseThreadSafe<T, N>::push_front_blocking(entry); }
    bool push_front_timeout(const T *const entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::push_front_timeout(entry, timeout_us); }
    bool pop_back_blocking(T *entry) { return DequeBaseThreadSafe<T, N>::pop_back_blocking(entry); }
    bool pop_back_timeout(T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::pop_back_timeout(entry, timeout_us); }
    bool pop_front_blocking(T *entry) { return DequeBaseThreadSafe<T, N>::pop_front_blocking(entry); }
    bool pop_front_timeout(T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::pop_front_timeout(entry, timeout_us); }
    const T *peek_back_blocking() { return DequeBaseThreadSafe<T, N>::peek_back_blocking(); }
    bool peek_back_timeout(const T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::peek_back_timeout(entry, timeout_us); }
    const T *peek_front_blocking() { return DequeBaseThreadSafe<T, N>::peek_front_blocking(); }
    bool peek_front_timeout(const T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::peek_front_timeout(entry, timeout_us); }
};

template <typename T, size_t N>
class QueueFIFOThreadSafe : protected DequeBaseThreadSafe<T, N>
{
public:
    QueueFIFOThreadSafe(bool overwrite) : DequeBaseThreadSafe<T, N>(overwrite)
    {}

    bool push_blocking(const T *const entry) { return DequeBaseThreadSafe<T, N>::push_front_blocking(entry); }
    bool push_timeout(const T *const entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::push_front_timeout(entry, timeout_us); }
    bool pop_blocking(T *entry) { return DequeBaseThreadSafe<T, N>::pop_back_blocking(entry); }
    bool pop_timeout(T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::pop_back_timeout(entry, timeout_us); }
    const T *peek_blocking() { return DequeBaseThreadSafe<T, N>::peek_back_blocking(); }
    bool peek_timeout(const T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::peek_back_timeout(entry, timeout_us); }
};

template <typename T, size_t N>
class StackLIFOThreadSafe : protected DequeBaseThreadSafe<T, N>
{
public:
    StackLIFOThreadSafe(bool overwrite) : DequeBaseThreadSafe<T, N>(overwrite)
    {}

    bool push_blocking(const T *const entry) { return DequeBaseThreadSafe<T, N>::push_front_blocking(entry); }
    bool push_timeout(const T *const entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::push_front_timeout(entry, timeout_us); }
    bool pop_blocking(T *entry) { return DequeBaseThreadSafe<T, N>::pop_back_blocking(entry); }
    bool pop_timeout(T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::pop_back_timeout(entry, timeout_us); }
    const T *peek_blocking() { return DequeBaseThreadSafe<T, N>::peek_back_blocking(); }
    bool peek_timeout(const T *entry, uint32_t timeout_us) { return DequeBaseThreadSafe<T, N>::peek_back_timeout(entry, timeout_us); }
};

// RP2040 mutexes

#ifdef ARDUINO_ARCH_RP2040

#include <pico/sync.h>
#include "pico/mutex.h"

template <typename T, size_t N>
class DequeThreadSafe_RP2040 : public DequeThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { mutex_enter_blocking(&_mutex); }
    inline void release_mutex() { mutex_exit(&_mutex); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { return mutex_enter_timeout_us(&_mutex, timeout_us); }

public:
    DequeThreadSafe_RP2040(bool overwrite) : DequeThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

template <typename T, size_t N>
class QueueFIFOThreadSafe_RP2040 : public QueueFIFOThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { mutex_enter_blocking(&_mutex); }
    inline void release_mutex() { mutex_exit(&_mutex); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { return mutex_enter_timeout_us(&_mutex, timeout_us); }

public:
    QueueFIFOThreadSafe_RP2040(bool overwrite) : QueueFIFOThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

template <typename T, size_t N>
class StackLIFOThreadSafe_RP2040 : public StackLIFOThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { mutex_enter_blocking(&_mutex); }
    inline void release_mutex() { mutex_exit(&_mutex); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { return mutex_enter_timeout_us(&_mutex, timeout_us); }

public:
    StackLIFOThreadSafe_RP2040(bool overwrite) : StackLIFOThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

#endif


#ifdef ARDUINO_ARCH_AVR

template <typename T, size_t N>
class DequeThreadSafe_RP2040 : public DequeThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { noInterrupts(); }
    inline void release_mutex() { interrupts(); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { noInterrupts(); return true; }

public:
    DequeThreadSafe_RP2040(bool overwrite) : DequeThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

template <typename T, size_t N>
class QueueFIFOThreadSafe_RP2040 : public QueueFIFOThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { noInterrupts(); }
    inline void release_mutex() { interrupts(); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { noInterrupts(); return true; }

public:
    QueueFIFOThreadSafe_RP2040(bool overwrite) : QueueFIFOThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

template <typename T, size_t N>
class StackLIFOThreadSafe_RP2040 : public StackLIFOThreadSafe<T, N>
{
private:
    mutex_t _mutex;

protected:
    inline void get_mutex_blocking() { noInterrupts(); }
    inline void release_mutex() { interrupts(); }
    inline bool get_mutex_timeout(uint32_t timeout_us) { noInterrupts(); return true; }

public:
    StackLIFOThreadSafe_RP2040(bool overwrite) : StackLIFOThreadSafe<T, N>(overwrite)
    {
        mutex_init(&_mutex);
    }
};

#endif

#endif /* _QUEUES_H_ */