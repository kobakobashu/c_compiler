#include "9cc.h"

char* strndup(const char* source, size_t length) {
    size_t source_length = strlen(source);
    size_t copy_length = (length < source_length) ? length : source_length;

    char* copy = malloc(copy_length + 1);  // +1 for the null terminator
    if (copy != NULL) {
        memcpy(copy, source, copy_length);
        copy[copy_length] = '\0';  // Add null terminator
    }
    return copy;
}