#ifndef _PICO_VECTOR_H_
#define _PICO_VECTOR_H_

template <typename T, size_t N>
class PicoVector
{
private:
    T _array[N];
    size_t _size = 0;

public:
    inline size_t size() const { return _size; }
    inline bool full() const { return size() == N; }
    inline bool available() const { return size() > 0; }
    inline bool empty() const { return size() == 0; }
    inline void clear() { _size = 0; }
    inline T *c_arr() { return _array; }

    bool insert(const T *t, size_t index)
    {
        if (size() >= N-1) return false; // will be full
        if (index > size()) return false; // want to access non-contiguous index

        if (index < size())
        {
            memmove(_array + index, _array + index + 1, sizeof(T) * (size() - index));
        }
        memcpy(_array + index, t, sizeof(T));

        ++_size;
        return true;
    }
    inline bool push_back(const T *t) { return insert(t, size()); }
    inline bool push_front(const T *t) { return insert(t, 0); }

    bool remove(size_t index)
    {
        if (index >= size()) return false; // trying to remove outside of array

        if (index < size() - 1)
        {
            memmove(_array + index + 1, _array + index, sizeof(T) * (size() - index - 1));
        }
        --_size;
        return true;
    }
    bool remove(T *t, size_t index)
    {
        if (index >= size()) return false; // trying to remove outside of array
        memcpy(t, &_array[index], sizeof(T));
        return remove(index);
    }
    inline bool pop_back()
    {
        return remove(size() - 1);
    }
    inline bool pop_back(T *t)
    {
        return remove(t, size() - 1);
    }
    inline bool pop_front()
    {
        return remove(0);
    }
    inline bool pop_front(T *t)
    {
        return remove(t, 0);
    }
};


#endif /* _PICO_VECTOR_H_ */