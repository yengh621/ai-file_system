@echo off
mkdir build 2>NUL
gcc -Wall -Wextra -Iinclude -c src/core/main.c -o build/main.o
gcc -Wall -Wextra -Iinclude -c src/core/disk.c -o build/disk.o
gcc -Wall -Wextra -Iinclude -c src/core/inode.c -o build/inode.o
gcc -Wall -Wextra -Iinclude -c src/core/block.c -o build/block.o
gcc -Wall -Wextra -Iinclude -c src/core/directory.c -o build/directory.o
gcc -Wall -Wextra -Iinclude -c src/core/file.c -o build/file.o
gcc -Wall -Wextra -Iinclude -c src/core/user.c -o build/user.o
gcc -Wall -Wextra -Iinclude -c src/core/permission.c -o build/permission.o
gcc -Wall -Wextra -Iinclude -c src/core/ai.c -o build/ai.o
gcc -Wall -Wextra -Iinclude -c src/innovations/kfs.c -o build/kfs.o
gcc -Wall -Wextra -Iinclude -c src/innovations/io_optimizer.c -o build/io_optimizer.o
gcc -Wall -Wextra -Iinclude -c src/innovations/security.c -o build/security.o
gcc -Wall -Wextra -o filesystem.exe build/main.o build/disk.o build/inode.o build/block.o build/directory.o build/file.o build/user.o build/permission.o build/ai.o build/kfs.o build/io_optimizer.o build/security.o
echo Build complete!
