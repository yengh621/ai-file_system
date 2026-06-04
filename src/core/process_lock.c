#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "filesystem.h"

#ifdef _WIN32

static HANDLE lock_file_handle = INVALID_HANDLE_VALUE;

static int ensure_lock_file(void) {
    if (lock_file_handle != INVALID_HANDLE_VALUE) {
        return 0;
    }

    lock_file_handle = CreateFileA(
        "vdisk.lock",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (lock_file_handle == INVALID_HANDLE_VALUE) {
        printf("Cannot open process lock file.\n");
        return -1;
    }
    return 0;
}

int init_process_lock_manager(void) {
    return ensure_lock_file();
}

void close_process_lock_manager(void) {
    if (lock_file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(lock_file_handle);
        lock_file_handle = INVALID_HANDLE_VALUE;
    }
}

int process_lock_inode(unsigned short ino, int lock_type) {
    OVERLAPPED ov;
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;

    if (ensure_lock_file() != 0) {
        return -1;
    }

    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)ino;
    if (lock_type == LOCK_WRITE) {
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }

    if (!LockFileEx(lock_file_handle, flags, 0, 1, 0, &ov)) {
        printf(lock_type == LOCK_WRITE
            ? "File is write-locked by another process.\n"
            : "File is locked by another process writer.\n");
        return -1;
    }
    return 0;
}

void process_unlock_inode(unsigned short ino, int lock_type) {
    OVERLAPPED ov;
    (void)lock_type;

    if (lock_file_handle == INVALID_HANDLE_VALUE) {
        return;
    }

    memset(&ov, 0, sizeof(ov));
    ov.Offset = (DWORD)ino;
    UnlockFileEx(lock_file_handle, 0, 1, 0, &ov);
}

#else
#include <fcntl.h>

static FILE *lock_file_fp = NULL;
static int lock_file_fd = -1;

static int ensure_lock_file(void) {
    if (lock_file_fd >= 0) {
        return 0;
    }

    lock_file_fp = fopen("vdisk.lock", "a+b");
    if (lock_file_fp == NULL) {
        printf("Cannot open process lock file.\n");
        return -1;
    }
    lock_file_fd = fileno(lock_file_fp);
    return 0;
}

int init_process_lock_manager(void) {
    return ensure_lock_file();
}

void close_process_lock_manager(void) {
    if (lock_file_fd >= 0) {
        fclose(lock_file_fp);
        lock_file_fp = NULL;
        lock_file_fd = -1;
    }
}

int process_lock_inode(unsigned short ino, int lock_type) {
    struct flock fl;

    if (ensure_lock_file() != 0) {
        return -1;
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = (lock_type == LOCK_WRITE) ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = ino;
    fl.l_len = 1;

    if (fcntl(lock_file_fd, F_SETLK, &fl) != 0) {
        printf(lock_type == LOCK_WRITE
            ? "File is write-locked by another process.\n"
            : "File is locked by another process writer.\n");
        return -1;
    }
    return 0;
}

void process_unlock_inode(unsigned short ino, int lock_type) {
    struct flock fl;
    (void)lock_type;

    if (lock_file_fd < 0) {
        return;
    }

    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = ino;
    fl.l_len = 1;
    fcntl(lock_file_fd, F_SETLK, &fl);
}
#endif
