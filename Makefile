asm: asm.o Y86.o
	gcc asm.o Y86.o -o asm -std=gnu11 -Wall
asm.o: asm.c lib/Y86.h
	gcc -c asm.c -o asm.o -std=gnu11 -Wall
Y86.o: lib/Y86.c lib/Y86.h lib/list.h
	gcc -c lib/Y86.c -o Y86.o -std=gnu11 -Wall
