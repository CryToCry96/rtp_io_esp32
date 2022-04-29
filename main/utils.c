#include "utils.h"
#include <string.h>

void strncpyz(char* dst, const char* src, int dstsize) {
    strncpy(dst, src, dstsize);
    dst[dstsize-1] = '\0';
}
