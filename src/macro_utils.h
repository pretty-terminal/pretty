#ifndef MACRO_UTILS_H
    #define MACRO_UTILS_H

    #define cat(x) #x
    #define xcat(x) cat(x)

    #define length_of(arr) (sizeof(arr) / sizeof *(arr))
    #define default_value(a, b) (a) = (a) ? (a) : (b)

    #define _BUILD_CHANNEL_VAL(cd, cu)

    #define HEX_TO_INT(h_ascii) ((h_ascii & 0xf) + (9 * ((h_ascii >> 6) & 1)))
    #define HEX_TO_RGB(hex)                             \
        (HEX_TO_INT(hex[0]) << 4) | HEX_TO_INT(hex[1]), \
        (HEX_TO_INT(hex[2]) << 4) | HEX_TO_INT(hex[3]), \
        (HEX_TO_INT(hex[4]) << 4) | HEX_TO_INT(hex[5])
    #define HEX_TO_RGBA(hex) \
        HEX_TO_RGB(hex),     \
        (HEX_TO_INT(hex[6]) << 4) | HEX_TO_INT(hex[7])

    #define UNUSED(x) ((void)(x))

#endif
