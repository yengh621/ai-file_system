@echo off
gcc -Wall -Wextra -o filesystem.exe main.c disk.c inode.c block.c directory.c file.c user.c permission.c ai.c innovations.c
echo Build complete!

