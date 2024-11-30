#ifndef _SMART_SORT_ARRAY_H_
#define _SMART_SORT_ARRAY_H_


namespace SmartSortArray
{
    template <typename T>
    void _swap(T *array, size_t idx_a, size_t idx_b)
    {
        T temp;
        memcpy(&temp, &array[idx_a], sizeof(T));
        memcpy(&array[idx_a], &array[idx_b], sizeof(T));
        memcpy(&array[idx_b], &temp, sizeof(T));
    }

    template <typename T>
    int32_t _qs_partition(T *array, int32_t low, int32_t high, bool (*cmp)(const T *a, const T *b))
    {
        T pivot = array[high];
        int32_t i = low - 1;

        for (int32_t j = low; j < high; ++j)
        {
            if (cmp(&array[j], &pivot))
            {
                ++i;
                _swap(array, i, j);
            }
        }

        _swap(array, i+1, high);
        return i+1;
    }
    template <typename T>
    void _quick_sort(T *array, int32_t low, int32_t high, bool (*cmp)(const T *a, const T *b))
    {
        if (low < high)
        {
            int32_t pi = _qs_partition(array, low, high, cmp);

            _quick_sort(array, low, pi - 1);
            _quick_sort(array, pi + 1, high);
        }
    }

    struct _ScoreIndex
    {
        int32_t score;
        size_t index;
    };
    bool _cmp_score_index(const _ScoreIndex *a, const _ScoreIndex *b)
    {
        return a->score <= b->score;
    }

    template <typename T>
    static void _sort_with_index_array(T *arr, _ScoreIndex *idx, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            while (idx[i].index != i)
            {
                size_t target = idx[i].index;

                // swap arr (only if idx[target] != -1 in which case overwrite)
                _swap(arr, i, target);

                // swap idx
                _swap(idx, i, target);
            }
        }
    }

    template <typename T>
    inline void sort_cmp(T *arr, size_t n, bool (*cmp)(const T *a, const T *b))
    {
        _quick_sort(arr, 0, n-1, cmp);
    }
    template <typename T, size_t N>
    void sort_score(T *arr, size_t n, bool (*score)(const int32_t *e, const T *arr))
    {
        // N is the size of the array, n is the length of useful elements in the array
        _ScoreIndex score_indices[N];
        if (!score_indices) return; // not allocated!
        for (size_t i = 0; i < n; ++i)
        {
            score_indices[i].score = score(&arr[i], arr);
            score_indices[i].index = i;
        }

        // sort by scores and get the indices sorted along with the scores
        _quick_sort(score_indices, 0, n, _cmp_score_index);

        // sort arr according to the sorted indices
        _sort_with_index_array(arr, score_indices, n);
    }
}




#endif /* _SMART_SORT_ARRAY_H_ */