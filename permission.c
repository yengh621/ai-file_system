#include "filesystem.h"

/* 文件锁数组 */
struct file_lock file_locks[SYSOPENFILE];

/* 初始化文件锁 */
void init_file_locks(void) {
    for (int i = 0; i < SYSOPENFILE; i++) {
        file_locks[i].ino = 0;
        file_locks[i].lock_type = LOCK_NONE;
        file_locks[i].owner_uid = -1;
        file_locks[i].read_count = 0;
    }
}

/* 加锁 */
int lock_file(unsigned short ino, int lock_type) {
    int idx = -1;
    for (int i = 0; i < SYSOPENFILE; i++) {
        if (file_locks[i].ino == ino) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        /* 找一个空槽 */
        for (int i = 0; i < SYSOPENFILE; i++) {
            if (file_locks[i].ino == 0) {
                idx = i;
                file_locks[i].ino = ino;
                file_locks[i].lock_type = LOCK_NONE;
                file_locks[i].read_count = 0;
                break;
            }
        }
        if (idx == -1) {
            printf("Lock table full.\n");
            return -1;
        }
    }
    
    struct file_lock *fl = &file_locks[idx];
    
    if (lock_type == LOCK_READ) {
        if (fl->lock_type == LOCK_WRITE) {
            if (fl->owner_uid != cur_uid) {
                printf("File is write-locked by another user.\n");
                return -1;
            }
        }
        fl->read_count++;
        fl->lock_type = LOCK_READ;
        fl->owner_uid = cur_uid;
        return 0;
    } else if (lock_type == LOCK_WRITE) {
        if (fl->lock_type != LOCK_NONE) {
            if (fl->owner_uid != cur_uid || (fl->lock_type == LOCK_READ && fl->read_count > 1)) {
                printf("File is locked by another user.\n");
                return -1;
            }
        }
        fl->lock_type = LOCK_WRITE;
        fl->owner_uid = cur_uid;
        fl->read_count = 0;
        return 0;
    }
    return -1;
}

/* 解锁 */
void unlock_file(unsigned short ino, int lock_type) {
    int idx = -1;
    for (int i = 0; i < SYSOPENFILE; i++) {
        if (file_locks[i].ino == ino) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) return;
    
    struct file_lock *fl = &file_locks[idx];
    
    if (lock_type == LOCK_READ) {
        if (fl->read_count > 0) {
            fl->read_count--;
            if (fl->read_count == 0) {
                fl->lock_type = LOCK_NONE;
                fl->owner_uid = -1;
                fl->ino = 0;
            }
        }
    } else if (lock_type == LOCK_WRITE) {
        fl->lock_type = LOCK_NONE;
        fl->owner_uid = -1;
        fl->read_count = 0;
        fl->ino = 0;
    }
}

/* 修改文件权限 */
void chmod(char *name, unsigned short mode) {
    struct inode *ip = namei(name);
    if (ip == NULL) {
        printf("File not found.\n");
        return;
    }
    
    if (cur_uid != 0 && cur_uid != ip->i_din.di_uid) {
        printf("Permission denied. Only owner or root can change mode.\n");
        iput(ip);
        return;
    }
    
    unsigned short old_mode = ip->i_din.di_mode;
    /* 保留文件类型位 */
    unsigned short new_mode = old_mode & S_IFMT;
    
    if (cur_uid == 0) {
        /* root: 保留所有者权限位，只修改组和其他用户权限 */
        unsigned short owner_perm = old_mode & 0700;
        unsigned short group_other_perm = mode & 0077;
        new_mode |= owner_perm | group_other_perm;
    } else {
        /* 普通用户：可以修改所有权限 */
        new_mode |= (mode & 0777);
    }
    
    ip->i_din.di_mode = new_mode;
    ip->i_flag = 1;
    iput(ip);
    printf("Mode changed to %04o.\n", new_mode & 0777);
}

/* 检查权限 */
int check_permission(struct inode *ip, int mode) {
    if (ip == NULL) return -1;
    if (cur_uid == -1) return -1; /* 未登录 */
    
    /* root用户(uid 0)拥有所有权限 */
    if (cur_uid == 0) return 0;
    
    int granted = 0;
    
    /* 检查所有者权限 */
    if (ip->i_din.di_uid == cur_uid) {
        if (mode & R_OK) granted |= (ip->i_din.di_mode & S_IRUSR) ? R_OK : 0;
        if (mode & W_OK) granted |= (ip->i_din.di_mode & S_IWUSR) ? W_OK : 0;
        if (mode & X_OK) granted |= (ip->i_din.di_mode & S_IXUSR) ? X_OK : 0;
    }
    /* 检查组权限 */
    else if (ip->i_din.di_gid == 100) { /* 假设所有用户都是组100 */
        if (mode & R_OK) granted |= (ip->i_din.di_mode & S_IRGRP) ? R_OK : 0;
        if (mode & W_OK) granted |= (ip->i_din.di_mode & S_IWGRP) ? W_OK : 0;
        if (mode & X_OK) granted |= (ip->i_din.di_mode & S_IXGRP) ? X_OK : 0;
    }
    /* 检查其他用户权限 */
    else {
        if (mode & R_OK) granted |= (ip->i_din.di_mode & S_IROTH) ? R_OK : 0;
        if (mode & W_OK) granted |= (ip->i_din.di_mode & S_IWOTH) ? W_OK : 0;
        if (mode & X_OK) granted |= (ip->i_din.di_mode & S_IXOTH) ? X_OK : 0;
    }
    
    return (granted & mode) == mode ? 0 : -1;
}

/* 检查是否是目录 */
int is_directory(struct inode *ip) {
    if (ip == NULL) return 0;
    return (ip->i_din.di_mode & S_IFMT) == S_IFDIR;
}

/* 检查目录是否为空 */
int is_empty_directory(struct inode *ip) {
    if (ip == NULL || !is_directory(ip)) return 0;
    
    unsigned long filesize = ip->i_din.di_size;
    struct direct dir;
    int count = 0;
    
    for (int i = 0; i < (filesize + 15) / 16; i++) {
        int bn = bmap(ip, i / 32);
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (dir.d_ino != 0) count++;
        if (count > 2) return 0; /* 超过.和..就不是空目录 */
    }
    
    return count <= 2;
}