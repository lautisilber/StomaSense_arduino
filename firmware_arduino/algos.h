#ifndef _ALGOS_H_
#define _ALGOS_H_

// template <typename T, typename U, size_t N>
// static void sort_with_index_array(T arr[N], U idx[N])
// {
//     // if an index is -1, no cpying will be performed!

//     // copy array
//     T temp[N];
//     memcpy(temp, arr, sizeof(T)*N);

//     // sort arr
//     memset(arr, 0, sizeof(T)*N);
//     for (U i = 0; i < N; i++)
//     {
//         const U x = idx[i];
//         if (x == -1) continue;
//         memcpy(&arr[x], &temp[x], sizeof(T));
//     }
// }

template <typename T>
static inline void swap(T *a, T *b)
{
    char temp[sizeof(T)];
    memcpy(temp, a, sizeof(T));
    memcpy(a, b, sizeof(T));
    memcpy(b, temp, sizeof(T));
}

template <typename T, typename U, size_t N>
static void sort_with_index_array(T arr[N], U idx[N])
{
    // order remaining non -1 indices
    for (U i = 0; i < N; i++)
    {
        while (idx[i] != i && idx[i] != -1)
        {
            U target = idx[i];

            // swap arr (only if idx[target] != -1 in which case overwrite)
            if (idx[target] == -1)
                memcpy(&arr[target], &arr[i], sizeof(T));
            else
                swap(&arr[i], &arr[target]);

            // swap idx
            swap(&idx[i], &idx[target]);
        }
    }
}

#endif /* _ALGOS_H_ */