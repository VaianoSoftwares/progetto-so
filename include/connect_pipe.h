#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>

#include "../include/protocol.h"

int connect_to_pipe(const char*, int);