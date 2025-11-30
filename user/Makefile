CC = i686-elf-gcc
LD = i686-elf-ld
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -I../include
LDFLAGS = -m elf_i386 -Ttext 0x08048000 --entry main

all: hello

hello: hello.o ../build/lib/syscall.o
	$(LD) $(LDFLAGS) -o hello hello.o ../build/lib/syscall.o

hello.o: hello.c
	$(CC) $(CFLAGS) -c hello.c -o hello.o

clean:
	rm -f hello hello.o
