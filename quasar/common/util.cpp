#include "util.h"

char* Util::SafeCStrCopy(char* dest, size_t destSize, const char* src, size_t srcSize)
{
    if (destSize > 0 and srcSize > 0)
    {
        size_t i;
        for (i = 0; i < destSize - 1 and i < srcSize and src[i]; i++)
        {
            dest[i] = src[i];
        }
        dest[i] = '\0';
    }
    return dest;
}
