#include "syscall.h"
main() {
    int i;
    for(i = 0; i < 15; i++) {
        PrintInt(55);
        Sleep(100);
    }
    return 0;
}
