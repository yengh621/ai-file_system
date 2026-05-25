#include "filesystem.h"

struct file sysopenfile[SYSOPENFILE];
int u_ofile[NOFILE];

void create(char *name) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    struct inode *dip = iget(cur_dir);
    if (dip == NULL) {
        printf("Directory not found.\n");
        return;
    }
    
    /* 检查父目录的写权限 */
    if (check_permission(dip, W_OK | X_OK) != 0) {
        printf("Permission denied.\n");
        iput(dip);
        return;
    }
    
    unsigned long filesize = dip->i_din.di_size;
    struct direct dir;
    int i, slot = -1;
    for (i = 0; i < (filesize + 15) / 16; i++) {
        int bn = bmap(dip, i / 32);
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (strcmp(dir.d_name, name) == 0) {
            printf("File exists.\n");
            iput(dip);
            return;
        }
        if (slot == -1 && dir.d_ino == 0) slot = i;
    }
    int ino = ialloc();
    if (ino == 0) {
        printf("No inode.\n");
        iput(dip);
        return;
    }
    struct inode *ip = iget(ino);
    ip->i_din.di_mode = S_IFREG | 0644;
    ip->i_din.di_uid = cur_uid;
    ip->i_din.di_gid = 100;
    ip->i_din.di_nlink = 1;
    ip->i_din.di_size = 0;
    memset(ip->i_din.di_addr, 0, sizeof(ip->i_din.di_addr));
    if (slot == -1) slot = (filesize + 15) / 16;
    int bn = bmap(dip, slot / 32);
    if (bn == 0) {
        iput(ip);
        ifree(ino);
        iput(dip);
        return;
    }
    bread(bn, block_buf);
    struct direct *dirp = (struct direct*)(block_buf + (slot % 32) * 16);
    strncpy(dirp->d_name, name, DIRSIZ - 1);
    dirp->d_name[DIRSIZ - 1] = '\0';
    dirp->d_ino = ino;
    bwrite(bn, block_buf);
    if ((slot + 1) * 16 > filesize) {
        dip->i_din.di_size = (slot + 1) * 16;
    }
    iput(ip);
    iput(dip);
    printf("Create successful.\n");
}

void delete(char *name) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    struct inode *dip = iget(cur_dir);
    if (dip == NULL) {
        printf("Directory not found.\n");
        return;
    }
    
    /* 检查父目录的写权限 */
    if (check_permission(dip, W_OK | X_OK) != 0) {
        printf("Permission denied.\n");
        iput(dip);
        return;
    }
    
    unsigned long filesize = dip->i_din.di_size;
    struct direct dir;
    int i, found = 0, ino = 0;
    for (i = 0; i < (filesize + 15) / 16; i++) {
        int bn = bmap(dip, i / 32);
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (strcmp(dir.d_name, name) == 0) {
            found = 1;
            ino = dir.d_ino;
            break;
        }
    }
    if (!found) {
        printf("File not found.\n");
        iput(dip);
        return;
    }
    
    struct inode *ip = iget(ino);
    
    /* 检查是否是目录 - delete不能删除目录 */
    if (is_directory(ip)) {
        printf("Is a directory. Use rmdir.\n");
        iput(ip);
        iput(dip);
        return;
    }
    
    /* 检查权限：只有所有者或root可以删除 */
    if (cur_uid != 0 && ip->i_din.di_uid != cur_uid) {
        printf("Permission denied.\n");
        iput(ip);
        iput(dip);
        return;
    }
    
    int bn = bmap(dip, i / 32);
    bread(bn, block_buf);
    struct direct *dirp = (struct direct*)(block_buf + (i % 32) * 16);
    dirp->d_ino = 0;
    memset(dirp->d_name, 0, DIRSIZ);
    bwrite(bn, block_buf);
    ip->i_din.di_nlink--;
    if (ip->i_din.di_nlink <= 0) {
        unsigned long size = ip->i_din.di_size;
        int j;
        for (j = 0; j < (size + BLOCKSIZ - 1) / BLOCKSIZ; j++) {
            int dbn = bmap(ip, j);
            if (dbn != 0) bfree(dbn);
        }
        ifree(ino);
    }
    iput(ip);
    iput(dip);
    printf("Delete successful.\n");
}

int open(char *name, int mode) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    struct inode *ip = namei(name);
    if (ip == NULL) {
        printf("File not found.\n");
        return -1;
    }
    if (is_directory(ip)) {
        printf("Is directory.\n");
        iput(ip);
        return -1;
    }
    
    /* 检查文件权限 */
    if (mode == O_RDONLY || mode == O_RDWR) {
        if (check_permission(ip, R_OK) != 0) {
            printf("Permission denied.\n");
            iput(ip);
            return -1;
        }
    }
    if (mode == O_WRONLY || mode == O_RDWR) {
        if (check_permission(ip, W_OK) != 0) {
            printf("Permission denied.\n");
            iput(ip);
            return -1;
        }
    }
    
    /* 尝试加锁 */
    int lock_type = (mode == O_RDONLY) ? LOCK_READ : LOCK_WRITE;
    if (lock_file(ip->i_ino, lock_type) != 0) {
        iput(ip);
        return -1;
    }
    
    int i;
    for (i = 0; i < SYSOPENFILE; i++) {
        if (sysopenfile[i].f_count == 0) break;
    }
    if (i >= SYSOPENFILE) {
        printf("Too many open files.\n");
        unlock_file(ip->i_ino, lock_type);
        iput(ip);
        return -1;
    }
    int fd;
    for (fd = 0; fd < NOFILE; fd++) {
        if (u_ofile[fd] == -1) break;
    }
    if (fd >= NOFILE) {
        printf("Too many open files.\n");
        unlock_file(ip->i_ino, lock_type);
        iput(ip);
        return -1;
    }
    sysopenfile[i].f_flag = mode;
    sysopenfile[i].f_count = 1;
    sysopenfile[i].f_inode = ip;
    sysopenfile[i].f_offset = 0;
    u_ofile[fd] = i;
    printf("Open successful, fd = %d\n", fd);
    return fd;
}

void close(int fd) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    if (fd < 0 || fd >= NOFILE || u_ofile[fd] == -1) {
        printf("Invalid fd.\n");
        return;
    }
    struct file *f = &sysopenfile[u_ofile[fd]];
    
    /* 解锁 */
    int lock_type = (f->f_flag == O_RDONLY) ? LOCK_READ : LOCK_WRITE;
    unlock_file(f->f_inode->i_ino, lock_type);
    
    f->f_count--;
    if (f->f_count == 0) {
        iput(f->f_inode);
    }
    u_ofile[fd] = -1;
    printf("Close successful.\n");
}

int read(int fd, unsigned char *buf, int count) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    if (fd < 0 || fd >= NOFILE || u_ofile[fd] == -1) {
        printf("Invalid fd.\n");
        return -1;
    }
    struct file *f = &sysopenfile[u_ofile[fd]];
    
    /* 检查打开模式 */
    if (f->f_flag == O_WRONLY) {
        printf("File not open for reading.\n");
        return -1;
    }
    
    struct inode *ip = f->f_inode;
    unsigned long offset = f->f_offset;
    unsigned long size = ip->i_din.di_size;
    if (offset >= size) return 0;
    if (offset + count > size) count = size - offset;
    int total = 0;
    while (count > 0) {
        int lbn = offset / BLOCKSIZ;
        int bn = bmap(ip, lbn);
        if (bn == 0) break;
        int len = BLOCKSIZ - (offset % BLOCKSIZ);
        if (len > count) len = count;
        bread(bn, block_buf);
        memcpy(buf + total, block_buf + (offset % BLOCKSIZ), len);
        total += len;
        offset += len;
        count -= len;
    }
    f->f_offset = offset;
    printf("Read %d bytes.\n", total);
    return total;
}

int write(int fd, unsigned char *buf, int count) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    if (fd < 0 || fd >= NOFILE || u_ofile[fd] == -1) {
        printf("Invalid fd.\n");
        return -1;
    }
    struct file *f = &sysopenfile[u_ofile[fd]];
    
    /* 检查打开模式 */
    if (f->f_flag == O_RDONLY) {
        printf("File not open for writing.\n");
        return -1;
    }
    
    struct inode *ip = f->f_inode;
    unsigned long offset = f->f_offset;
    int total = 0;
    while (count > 0) {
        int lbn = offset / BLOCKSIZ;
        int bn = bmap(ip, lbn);
        if (bn == 0) break;
        int len = BLOCKSIZ - (offset % BLOCKSIZ);
        if (len > count) len = count;
        bread(bn, block_buf);
        memcpy(block_buf + (offset % BLOCKSIZ), buf + total, len);
        bwrite(bn, block_buf);
        total += len;
        offset += len;
        count -= len;
    }
    if (offset > ip->i_din.di_size) {
        ip->i_din.di_size = offset;
    }
    f->f_offset = offset;
    ip->i_flag |= 1;
    printf("Write %d bytes.\n", total);
    return total;
}