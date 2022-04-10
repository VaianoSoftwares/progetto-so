#include "../include/error.h"

void throw_err(const char* msg) {
    perror(msg);
    exit(errno);
}