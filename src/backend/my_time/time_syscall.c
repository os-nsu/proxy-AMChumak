#include <sys/syscall.h>
#include <unistd.h>
#include "../../include/my_time.h"

time_t get_time(void) {
    return (time_t) syscall(SYS_time, NULL);
}