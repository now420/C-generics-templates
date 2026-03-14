#include <stdio.h>

template <typename T>
T reinterpret_cast(void* address) {
    return (T)(address);
}

template <typename T>
void swap(T* a, T* b) {
    T temp = *a;
    *a = *b;
    *b = temp;
}

template <typename T>
T min(T a, T b) {
    return a < b ? a : b;
}

template <typename T>
T max(T a, T b) {
    return a > b ? a : b;
}

int main(void) {
    float f = 3.14f;
    int bits = *reinterpret_cast<int*>(&f);
    printf("3.14f bits as int: %d\n", bits);
    float again = *reinterpret_cast<float*>(&bits);
    printf("bits back to float: %.6f\n", again);

    int x = 1, y = 2;
    swap<int>(&x, &y);
    printf("swap<int>: %d %d\n", x, y);

    double da = 10.0, db = 20.0;
    swap<double>(&da, &db);
    printf("swap<double>: %.1f %.1f\n", da, db);

    printf("min<int>(3,7): %d\n",   min<int>(3, 7));
    printf("max<int>(3,7): %d\n",   max<int>(3, 7));
    printf("min<float>(1.5,2.5): %.1f\n", min<float>(1.5f, 2.5f));
    printf("max<float>(1.5,2.5): %.1f\n", max<float>(1.5f, 2.5f));

    return 0;
}
