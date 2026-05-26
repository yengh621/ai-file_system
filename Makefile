CC=gcc
CFLAGS=-Wall -Wextra -Iinclude
TARGET=filesystem

# Core object files
CORE_OBJS=main.o disk.o inode.o block.o directory.o file.o user.o permission.o ai.o

# Innovations object files
INNOVATIONS_OBJS=kfs.o io_optimizer.o security.o

OBJS=$(addprefix build/,$(CORE_OBJS) $(INNOVATIONS_OBJS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

build/%.o: src/core/%.c include/filesystem.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/innovations/%.c include/filesystem.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -f $(TARGET)
	rm -rf build
	rm -f vdisk.bin
