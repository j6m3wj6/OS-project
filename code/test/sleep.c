#include "syscall.h"
main() {
	int i;
	for (i=0;i<5;i++) {
		PrintInt(100);
		Sleep(100000);
	}
	return 0;
}
