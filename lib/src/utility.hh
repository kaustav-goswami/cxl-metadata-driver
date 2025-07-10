#ifndef __UTILITY_HH__
#define __UTILITY_HH__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void fatal(const char* reason, ...);
void info_who_locked(int host_id);
void info_who_unlocked(int host_id);

#endif  // __UTILITY_HH__