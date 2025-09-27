#ifndef MACRO_UTILS_H
    #define MACRO_UTILS_H

    #define cat(x) #x
    #define xcat(x) cat(x)

    #define length_of(arr) (sizeof(arr) / sizeof *(arr))
    #define default_value(a, b) (a) = (a) ? (a) : (b)
#endif
