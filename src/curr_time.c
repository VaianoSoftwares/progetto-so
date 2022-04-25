#include "../include/curr_time.h"

char* get_curr_time() {
    const time_t now = time(NULL);
    const struct tm *time_ptr = localtime(&now);
    return asctime(time_ptr);
}