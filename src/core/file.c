#include "filesystem.h"
struct file sysopenfile[SYSOPENFILE];
int u_ofile[NOFILE];
// 创建文件
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
    if (detect_anomaly("create", name)) {
        iput(dip);
        return;
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
    ip->i_din.di_gid = get_current_user_gid();
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
    kfs_hot_cache_update(name, (unsigned short)ino);
    printf("Create successful.\n");
}
// 删除文件
void delete(char *name) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    
    /* 安全异常检测 */
    if (detect_anomaly("delete", name)) {
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

    kfs_remove_file(name, ino);
    
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
        ip->i_din.di_mode = 0;
        ip->i_din.di_size = 0;
        ip->i_flag |= 1; // 标记为脏数据
        ifree(ino);
    }
    iput(ip);
    iput(dip);
    printf("Delete successful.\n");
}
// 打开文件
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
    if (mode == O_WRONLY || mode == O_RDWR || mode == O_APPEND) {
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
    strncpy(sysopenfile[i].f_name, name, DIRSIZ - 1);
    sysopenfile[i].f_name[DIRSIZ - 1] = '\0';
    u_ofile[fd] = i;
    kfs_hot_cache_update(name, ip->i_ino);
    printf("Open successful, fd = %d\n", fd);
    return fd;
}

/* Open a KFS virtual entry by its stable inode identity. */
int open_inode(unsigned short ino, int mode, const char *display_name) {
    struct inode *ip;
    int lock_type;
    int i;
    int fd;

    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    if (mode != O_RDONLY) {
        printf("KFS virtual files are read-only.\n");
        return -1;
    }

    ip = iget(ino);
    if (ip == NULL || ip->i_din.di_mode == 0) {
        if (ip != NULL) iput(ip);
        printf("File not found.\n");
        return -1;
    }
    if (is_directory(ip)) {
        printf("Is directory.\n");
        iput(ip);
        return -1;
    }
    if ((mode == O_RDONLY || mode == O_RDWR) && check_permission(ip, R_OK) != 0) {
        printf("Permission denied.\n");
        iput(ip);
        return -1;
    }
    if ((mode == O_WRONLY || mode == O_RDWR || mode == O_APPEND) &&
        check_permission(ip, W_OK) != 0) {
        printf("Permission denied.\n");
        iput(ip);
        return -1;
    }

    lock_type = (mode == O_RDONLY) ? LOCK_READ : LOCK_WRITE;
    if (lock_file(ip->i_ino, lock_type) != 0) {
        iput(ip);
        return -1;
    }

    for (i = 0; i < SYSOPENFILE; i++) {
        if (sysopenfile[i].f_count == 0) break;
    }
    for (fd = 0; fd < NOFILE; fd++) {
        if (u_ofile[fd] == -1) break;
    }
    if (i >= SYSOPENFILE || fd >= NOFILE) {
        printf("Too many open files.\n");
        unlock_file(ip->i_ino, lock_type);
        iput(ip);
        return -1;
    }

    sysopenfile[i].f_flag = mode;
    sysopenfile[i].f_count = 1;
    sysopenfile[i].f_inode = ip;
    sysopenfile[i].f_offset = 0;
    if (display_name != NULL && display_name[0] != '\0') {
        strncpy(sysopenfile[i].f_name, display_name, DIRSIZ - 1);
        sysopenfile[i].f_name[DIRSIZ - 1] = '\0';
    } else {
        snprintf(sysopenfile[i].f_name, DIRSIZ, "ino:%u", ino);
    }
    u_ofile[fd] = i;
    kfs_hot_cache_update(sysopenfile[i].f_name, ip->i_ino);
    printf("Open successful, fd = %d\n", fd);
    return fd;
}
// 关闭文件
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
// 读取文件
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
        int len = BLOCKSIZ - (offset % BLOCKSIZ);
        if (len > count) len = count;
        
        /* KFS 热点文件优先走 KFS 数据区。 */
        if (!kfs_read_hot_file_block(f->f_name, ip->i_ino, lbn, block_buf)) {
            int bn = bmap(ip, lbn);
            if (bn == 0) break;
            if (!get_prefetched_block(ip->i_ino, lbn, block_buf)) {
            /* 缓存未命中，读磁盘 */
                bread(bn, block_buf);
            }
        }
        
        memcpy(buf + total, block_buf + (offset % BLOCKSIZ), len);
        total += len;
        offset += len;
        count -= len;
        
        /* 记录 I/O 请求（触发该文件的预取） */
        record_io_request(ip->i_ino, lbn, 1);
        
    }
    f->f_offset = offset;
    if (total > 0) {
        kfs_hot_cache_update(f->f_name, ip->i_ino);
    }
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

    if (f->f_flag == O_APPEND) {
        f->f_offset = ip->i_din.di_size; // 跳到文件末尾
    }

    if (count > 0) {
        kfs_invalidate_file(f->f_name, ip->i_ino);
    }

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
        record_io_request(ip->i_ino, lbn, 0);
        total += len;
        offset += len;
        count -= len;
    }
    if (offset > ip->i_din.di_size) {
        ip->i_din.di_size = offset;
    }
    f->f_offset = offset;
    ip->i_flag |= 1;
    if (total > 0) {
        kfs_hot_cache_update(f->f_name, ip->i_ino);
    }
    printf("Write %d bytes.\n", total);
    return total;
}

static void free_all_file_blocks(struct inode *ip) {
    unsigned short single[BLOCKSIZ / sizeof(unsigned short)];
    unsigned short first_level[BLOCKSIZ / sizeof(unsigned short)];
    unsigned short second_level[BLOCKSIZ / sizeof(unsigned short)];

    for (int i = 0; i < 6; i++) {
        if (ip->i_din.di_addr[i] != 0) {
            bfree(ip->i_din.di_addr[i]);
            ip->i_din.di_addr[i] = 0;
        }
    }

    if (ip->i_din.di_addr[6] != 0) {
        bread(ip->i_din.di_addr[6], (unsigned char *)single);
        for (int i = 0; i < BLOCKSIZ / (int)sizeof(unsigned short); i++) {
            if (single[i] != 0) {
                bfree(single[i]);
            }
        }
        bfree(ip->i_din.di_addr[6]);
        ip->i_din.di_addr[6] = 0;
    }

    if (ip->i_din.di_addr[7] != 0) {
        bread(ip->i_din.di_addr[7], (unsigned char *)first_level);
        for (int i = 0; i < BLOCKSIZ / (int)sizeof(unsigned short); i++) {
            if (first_level[i] == 0) {
                continue;
            }
            bread(first_level[i], (unsigned char *)second_level);
            for (int j = 0; j < BLOCKSIZ / (int)sizeof(unsigned short); j++) {
                if (second_level[j] != 0) {
                    bfree(second_level[j]);
                }
            }
            bfree(first_level[i]);
        }
        bfree(ip->i_din.di_addr[7]);
        ip->i_din.di_addr[7] = 0;
    }
}

int truncate_file(int fd, unsigned long size) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    if (fd < 0 || fd >= NOFILE || u_ofile[fd] == -1) {
        printf("Invalid fd.\n");
        return -1;
    }

    struct file *f = &sysopenfile[u_ofile[fd]];
    if (f->f_flag == O_RDONLY) {
        printf("File not open for writing.\n");
        return -1;
    }
    if (size != 0) {
        printf("Only truncating to size 0 is supported.\n");
        return -1;
    }

    struct inode *ip = f->f_inode;
    kfs_invalidate_file(f->f_name, ip->i_ino);
    free_all_file_blocks(ip);
    ip->i_din.di_size = 0;
    ip->i_flag |= 1;
    f->f_offset = 0;
    printf("Truncate successful, fd = %d, size = 0\n", fd);
    return 0;
}

// 文件定位
int seek_file(int fd, unsigned long offset) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return -1;
    }
    if (fd < 0 || fd >= NOFILE || u_ofile[fd] == -1) {
        printf("Invalid fd.\n");
        return -1;
    }

    sysopenfile[u_ofile[fd]].f_offset = offset;
    printf("Seek successful, fd = %d, offset = %lu\n", fd, offset);
    return 0;
}
