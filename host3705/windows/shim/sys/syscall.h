/* sys/syscall.h stand-in for the MSYS2 build: SYS_gettid is only printed */
#include <unistd.h>
#define SYS_gettid 0
#define syscall(n) ((long)getpid())
