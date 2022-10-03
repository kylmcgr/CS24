#include "exception.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

//// GLOBAL ERROR STATE ////

static exception_t exception_type;
static char *exception_error;

//// EVALUATION ERROR HANDLING ////

void exception_set(exception_t type, char *message) {
    free(exception_error);

    exception_type = type;
    exception_error = strdup(message);
}

void exception_set_format(exception_t type, char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (result < 0) {
        fprintf(stderr, "failed to format exception message with format '%s'\n", format);
        abort();
    }

    char buf[result + 1];
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);
    exception_set(type, buf);
}
void exception_clear() {
    free(exception_error);

    exception_type = EXC_NONE;
    exception_error = NULL;
}

bool exception_occurred() {
    return exception_type != EXC_NONE;
}

static char *exception_type_to_str(exception_t type) {
    switch (type) {
        case EXC_NONE:          return "<none>";
        case EXC_SYNTAX_ERROR:  return "SyntaxError";
        case EXC_NAME_ERROR:    return "NameError";
        case EXC_TYPE_ERROR:    return "TypeError";
        case EXC_VALUE_ERROR:   return "ValueError";
        case EXC_INDEX_ERROR:   return "IndexError";
        case EXC_KEY_ERROR:     return "KeyError";
        case EXC_MEMORY_ERROR:  return "MemoryError";
        case EXC_INTERNAL:      return "<internal>";
    }
    return "<unknown>";
}
void exception_print(FILE *stream) {
    if (exception_occurred()) {
        fprintf(stream, "%s: %s\n",
                exception_type_to_str(exception_type), exception_error);
    }
}
