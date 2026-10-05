/* hires.c - ask Windows for a 1 ms timer: comm3705 and the 3705 wait with usleep() of microseconds, and the default
   15.6 ms tick turns every one of them into 15.6 ms */
#include <windows.h>
__attribute__((constructor)) static void shim_hires(void) { timeBeginPeriod(1); }
