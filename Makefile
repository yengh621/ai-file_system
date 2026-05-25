CC=gcc
CFLAGS=-Wall -Wextra
TARGET=filesystem
OBJS=main.o disk.o inode.o block.o directory.o file.o user.o permission.o ai.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

main.o: main.c filesystem.h
	$(CC) $(CFLAGS) -c main.c

disk.o: disk.c filesystem.h
	$(CC) $(CFLAGS) -c disk.c

inode.o: inode.c filesystem.h
	$(CC) $(CFLAGS) -c inode.c

block.o: block.c filesystem.h
	$(CC) $(CFLAGS) -c block.c

directory.o: directory.c filesystem.h
	$(CC) $(CFLAGS) -c directory.c

file.o: file.c filesystem.h
	$(CC) $(CFLAGS) -c file.c

user.o: user.c filesystem.h
	$(CC) $(CFLAGS) -c user.c

permission.o: permission.c filesystem.h
	$(CC) $(CFLAGS) -c permission.c

ai.o: ai.c filesystem.h
	$(CC) $(CFLAGS) -c ai.c

clean:
	rm -f $(TARGET) *.o vdisk.bin
