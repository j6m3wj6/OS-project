#include "syscall.h"
main() {
	int i;
	for (i=0;i<5;i++) {
		PrintInt(11111);
		Sleep(500);
	}
	return 0;
}
