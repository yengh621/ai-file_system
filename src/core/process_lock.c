/*
 * 跨平台进程锁管理器
 * 
 * 功能: 使用 vdisk.lock 文件的字节范围锁实现跨进程的 inode 锁定
 * 原理: 每个 inode 对应锁定文件中的一个字节位置
 *       - inode N 锁定第 N 个字节
 *       - 通过字节范围锁实现多进程同步
 */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN

#define open __system_open
#include <windows.h>
#undef open

#else

#define open __system_open
#include <fcntl.h>
#undef open

#endif

#include "filesystem.h"

<<<<<<< HEAD
/* ==================== Windows 平台实现 ==================== */
=======
#include <string.h>
#include <stdio.h>

>>>>>>> ade09a44633cc71fce0a0f77f3318d47f95f1ba9
#ifdef _WIN32

/* 锁文件句柄 (Windows) */
static HANDLE lock_file_handle = INVALID_HANDLE_VALUE;

/*
 * 确保锁文件已打开
 * 
 * 返回: 0=成功, -1=失败
 */
static int ensure_lock_file(void) {
    /* 如果句柄已有效，直接返回 */
    if (lock_file_handle != INVALID_HANDLE_VALUE) {
        return 0;
    }

    /* 创建或打开 vdisk.lock 文件 */
    lock_file_handle = CreateFileA(
        "vdisk.lock",
        GENERIC_READ | GENERIC_WRITE,           /* 读写权限 */
        FILE_SHARE_READ | FILE_SHARE_WRITE,     /* 允许其他进程共享读写 */
        NULL,
        OPEN_ALWAYS,                            /* 如果不存在则创建 */
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (lock_file_handle == INVALID_HANDLE_VALUE) {
        printf("Cannot open process lock file.\n");
        return -1;
    }
    return 0;
}

/*
 * 初始化进程锁管理器
 * 
 * 返回: 0=成功, -1=失败
 */
int init_process_lock_manager(void) {
    return ensure_lock_file();
}

/*
 * 关闭进程锁管理器，释放资源
 */
void close_process_lock_manager(void) {
    if (lock_file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(lock_file_handle);
        lock_file_handle = INVALID_HANDLE_VALUE;
    }
}

/*
 * 对指定 inode 加进程级锁
 * 
 * 参数:
 *   ino: 要锁定的 inode 号
 *   lock_type: 锁类型 (LOCK_READ 或 LOCK_WRITE)
 * 
 * 返回: 0=成功, -1=失败
 */
int process_lock_inode(unsigned short ino, int lock_type) {
    OVERLAPPED ov;
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;  /* 非阻塞模式 */

    /* 确保锁文件已打开 */
    if (ensure_lock_file() != 0) {
        return -1;
    }

    /* 初始化重叠结构，设置锁定位置为 inode 号 */
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)ino;  /* 锁定第 ino 个字节 */
    
    /* 如果是写锁，设置独占标志 */
    if (lock_type == LOCK_WRITE) {
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }

    /* 尝试锁定字节范围 (从 ino 开始，锁定 1 个字节) */
    if (!LockFileEx(lock_file_handle, flags, 0, 1, 0, &ov)) {
        printf(lock_type == LOCK_WRITE
            ? "File is write-locked by another process.\n"
            : "File is locked by another process writer.\n");
        return -1;
    }
    return 0;
}

/*
 * 对指定 inode 解进程级锁
 * 
 * 参数:
 *   ino: 要解锁的 inode 号
 *   lock_type: 锁类型 (未使用，保留接口兼容性)
 */
void process_unlock_inode(unsigned short ino, int lock_type) {
    OVERLAPPED ov;
    (void)lock_type;  /* 标记参数未使用 */

    if (lock_file_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    /* 初始化重叠结构，设置解锁位置为 inode 号 */
    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)ino;
    
    /* 解锁字节范围 */
    UnlockFileEx(lock_file_handle, 0, 1, 0, &ov);
}

/* ==================== Linux/Unix 平台实现 ==================== */
#else

#include <fcntl.h>
#include <unistd.h>

/* 锁文件指针 (Linux/Unix) */
static FILE *lock_file_fp = NULL;
/* 锁文件描述符 */
static int lock_file_fd = -1;

/*
 * 确保锁文件已打开 (Linux/Unix)
 * 
 * 返回: 0=成功, -1=失败
 */
static int ensure_lock_file(void) {
    /* 如果文件描述符已有效，直接返回 */
    if (lock_file_fd >= 0) {
        return 0;
    }

    /* 以追加模式打开 vdisk.lock 文件 */
    lock_file_fp = fopen("vdisk.lock", "a+b");
    if (lock_file_fp == NULL) {
        printf("Cannot open process lock file.\n");
        return -1;
    }
    
    /* 获取文件描述符 */
    lock_file_fd = fileno(lock_file_fp);
    return 0;
}

/*
 * 初始化进程锁管理器 (Linux/Unix)
 * 
 * 返回: 0=成功, -1=失败
 */
int init_process_lock_manager(void) {
    return ensure_lock_file();
}

/*
 * 关闭进程锁管理器，释放资源 (Linux/Unix)
 */
void close_process_lock_manager(void) {
    if (lock_file_fd >= 0) {
        fclose(lock_file_fp);
        lock_file_fp = NULL;
        lock_file_fd = -1;
    }
}

/*
 * 对指定 inode 加进程级锁 (Linux/Unix)
 * 
 * 参数:
 *   ino: 要锁定的 inode 号
 *   lock_type: 锁类型 (LOCK_READ 或 LOCK_WRITE)
 * 
 * 返回: 0=成功, -1=失败
 */
int process_lock_inode(unsigned short ino, int lock_type) {
    struct flock fl;

    /* 确保锁文件已打开 */
    if (ensure_lock_file() != 0) {
        return -1;
    }

    /* 初始化 flock 结构 */
    memset(&fl, 0, sizeof(fl));
    fl.l_type = (lock_type == LOCK_WRITE) ? F_WRLCK : F_RDLCK;  /* 锁类型 */
    fl.l_whence = SEEK_SET;                                      /* 从文件开头计算 */
    fl.l_start = ino;                                            /* 起始位置: inode 号 */
    fl.l_len = 1;                                                /* 锁定长度: 1 个字节 */

    /* 尝试加锁 (非阻塞模式 F_SETLK) */
    if (fcntl(lock_file_fd, F_SETLK, &fl) != 0) {
        printf(lock_type == LOCK_WRITE
            ? "File is write-locked by another process.\n"
            : "File is locked by another process writer.\n");
        return -1;
    }
    return 0;
}

/*
 * 对指定 inode 解进程级锁 (Linux/Unix)
 * 
 * 参数:
 *   ino: 要解锁的 inode 号
 *   lock_type: 锁类型 (未使用，保留接口兼容性)
 */
void process_unlock_inode(unsigned short ino, int lock_type) {
    struct flock fl;
    (void)lock_type;  /* 标记参数未使用 */

    if (lock_file_fd < 0) {
        return;
    }

    /* 初始化 flock 结构，设置为解锁 */
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;      /* 解锁 */
    fl.l_whence = SEEK_SET;   /* 从文件开头计算 */
    fl.l_start = ino;         /* 起始位置: inode 号 */
    fl.l_len = 1;             /* 解锁长度: 1 个字节 */
    
    /* 执行解锁 */
    fcntl(lock_file_fd, F_SETLK, &fl);
}

#endif
