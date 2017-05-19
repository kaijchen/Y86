CC = gcc
CFLAGS = -std=gnu11 -Wall

Y86asm: Y86.o Y86asm.o
	$(CC) Y86.o Y86asm.o -o Y86asm $(CFLAGS)
Y86asm.o: Y86asm.c lib/Y86.h lib/list.h
	$(CC) -c Y86asm.c -o Y86asm.o $(CFLAGS)
Y86.o: lib/Y86.c lib/Y86.h
	$(CC) -c lib/Y86.c -o Y86.o $(CFLAGS)
clean:
	$(RM) *.o Y86asm y.out
