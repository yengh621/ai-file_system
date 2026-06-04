#include "filesystem.h"

static struct inode* lookup_path_nofollow(char *path) {
    char parent_path[256];
    char filename[DIRSIZ + 1];
    char *last_slash = strrchr(path, '/');
    struct inode *dir_inode;

    if (last_slash == NULL) {
        dir_inode = iget(cur_dir);
        strncpy(filename, path, DIRSIZ);
        filename[DIRSIZ] = '\0';
    } else {
        size_t parent_len = (size_t)(last_slash - path);
        if (parent_len == 0) {
            strcpy(parent_path, "/");
        } else {
            if (parent_len >= sizeof(parent_path)) parent_len = sizeof(parent_path) - 1;
            memcpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
        }
        strncpy(filename, last_slash + 1, DIRSIZ);
        filename[DIRSIZ] = '\0';
        filename[DIRSIZ] = '\0';
        dir_inode = namei(parent_path);
    }

    if (dir_inode == NULL || !is_directory(dir_inode)) {
        if (dir_inode != NULL) iput(dir_inode);
        return NULL;
    }

    for (int i = 0; i < DIRNUM; i++) {
        int blkno = bmap(dir_inode, i / (BLOCKSIZ / sizeof(struct direct)));
        if (blkno == 0) break;

        unsigned char buf[BLOCKSIZ];
        bread(blkno, buf);
        struct direct *dir_ptr = (struct direct *)buf;

        for (int j = 0; j < BLOCKSIZ / sizeof(struct direct); j++) {
            if (dir_ptr[j].d_ino == 0) continue;
            if (strcmp(dir_ptr[j].d_name, filename) == 0) {
                struct inode *ip = iget(dir_ptr[j].d_ino);
                iput(dir_inode);
                return ip;
            }
        }
    }

    iput(dir_inode);
    return NULL;
}

/* 创建硬链接 */
int link(char *oldpath, char *newpath) {
    struct inode *old_inode;
    struct inode *dir_inode;
    struct direct dirent;
    int i;
    
    /* 查找源文件 */
    old_inode = namei(oldpath);
    if (old_inode == NULL) {
        printf("link: %s not found\n", oldpath);
        return -1;
    }
    
    /* 检查源文件是否是目录（硬链接不允许链接目录） */
    if (is_directory(old_inode)) {
        printf("link: %s is a directory (hard links to directories not allowed)\n", oldpath);
        iput(old_inode);
        return -1;
    }
    
    /* 查找目标文件所在目录 */
    char dirname[DIRSIZ + 1];
    char filename[DIRSIZ + 1];
    char *last_slash = strrchr(newpath, '/');
    
    if (last_slash == NULL) {
        /* 当前目录 */
        dir_inode = iget(cur_dir);
        strncpy(filename, newpath, DIRSIZ);
        filename[DIRSIZ] = '\0';
    } else {
        /* 分离目录和文件名 */
        strncpy(dirname, newpath, last_slash - newpath);
        dirname[last_slash - newpath] = '\0';
        strncpy(filename, last_slash + 1, DIRSIZ);
        
        dir_inode = namei(dirname);
        if (dir_inode == NULL) {
            printf("link: %s not found\n", dirname);
            iput(old_inode);
            return -1;
        }
    }
    
    /* 检查目标文件是否已存在 */
    int found = 0;
    for (i = 0; i < DIRNUM; i++) {
        int blkno = bmap(dir_inode, i / (BLOCKSIZ / sizeof(struct direct)));
        if (blkno == 0) break;
        
        unsigned char buf[BLOCKSIZ];
        bread(blkno, buf);
        struct direct *dir_ptr = (struct direct *)buf;
        
        for (int j = 0; j < BLOCKSIZ / sizeof(struct direct); j++) {
            if (dir_ptr[j].d_ino == 0) continue;
            if (strcmp(dir_ptr[j].d_name, filename) == 0) {
                found = 1;
                break;
            }
        }
        
        if (found) break;
    }
    
    if (found) {
        printf("link: %s already exists\n", newpath);
        iput(old_inode);
        iput(dir_inode);
        return -1;
    }
    
    /* 查找空闲目录项 */
    int free_blk = -1;
    int free_idx = -1;
    
    for (i = 0; i < DIRNUM; i++) {
        int blkno = bmap(dir_inode, i / (BLOCKSIZ / sizeof(struct direct)));
        if (blkno == 0) {
            /* 需要分配新块 */
            blkno = balloc();
            if (blkno == 0) {
                printf("link: no space left on device\n");
                iput(old_inode);
                iput(dir_inode);
                return -1;
            }
            /* 更新 inode 的地址 */
            int addr_idx = i / (BLOCKSIZ / sizeof(struct direct));
            if (addr_idx >= NADDR) {
                printf("link: file too large\n");
                bfree(blkno);
                iput(old_inode);
                iput(dir_inode);
                return -1;
            }
            dir_inode->i_din.di_addr[addr_idx] = blkno;
            /* 清空新块 */
            unsigned char buf[BLOCKSIZ];
            memset(buf, 0, BLOCKSIZ);
            bwrite(blkno, buf);
        }
        
        unsigned char buf[BLOCKSIZ];
        bread(blkno, buf);
        struct direct *dir_ptr = (struct direct *)buf;
        
        for (int j = 0; j < BLOCKSIZ / sizeof(struct direct); j++) {
            if (dir_ptr[j].d_ino == 0) {
                free_blk = blkno;
                free_idx = j;
                goto found_free;
            }
        }
    }
    
found_free:
    if (free_blk == -1) {
        printf("link: no space in directory\n");
        iput(old_inode);
        iput(dir_inode);
        return -1;
    }
    
    /* 添加目录项 */
    unsigned char buf[BLOCKSIZ];
    bread(free_blk, buf);
    struct direct *dir_ptr = (struct direct *)buf;
    
    strncpy(dir_ptr[free_idx].d_name, filename, DIRSIZ - 1);
    dir_ptr[free_idx].d_name[DIRSIZ - 1] = '\0';
    dir_ptr[free_idx].d_ino = old_inode->i_ino;
    bwrite(free_blk, buf);

    int entries_per_block = BLOCKSIZ / sizeof(struct direct);
    unsigned long dir_slot = (unsigned long)(i / entries_per_block) * entries_per_block + free_idx;
    unsigned long required_size = (dir_slot + 1) * sizeof(struct direct);
    if (required_size > dir_inode->i_din.di_size) {
        dir_inode->i_din.di_size = required_size;
    }
    
    /* 增加链接计数 */
    old_inode->i_din.di_nlink++;
    iput_inode(old_inode->i_ino, &old_inode->i_din);
    
    printf("link: %s -> %s (hard link)\n", newpath, oldpath);
    
    iput(old_inode);
    iput(dir_inode);
    return 0;
}

/* 创建符号链接 */
int symlink(char *oldpath, char *newpath) {
    /* 创建一个新文件，文件内容是目标路径 */
    create(newpath);
    int fd = open(newpath, O_RDWR);
    if (fd < 0) {
        printf("symlink: failed to create %s\n", newpath);
        return -1;
    }
    
    /* 写入链接内容 */
    write(fd, (unsigned char *)oldpath, strlen(oldpath));
    close(fd);
    
    /* 修改 inode 类型为符号链接 */
    struct inode *ip = namei(newpath);
    if (ip != NULL) {
        ip->i_din.di_mode = S_IFLNK | 0777;
        iput_inode(ip->i_ino, &ip->i_din);
        iput(ip);
    }
    
    printf("symlink: %s -> %s (symbolic link)\n", newpath, oldpath);
    return 0;
}

/* 从 inode 直接读取符号链接内容 */
int readlink_inode(struct inode *ip, char *buf, int bufsize) {
    if (!is_link(ip)) return -1;
    
    int size = ip->i_din.di_size;
    if (size > bufsize - 1) size = bufsize - 1;
    
    int remaining = size;
    int offset = 0;
    while (remaining > 0) {
        int lbn = offset / BLOCKSIZ;
        int bn = bmap(ip, lbn);
        if (bn == 0) break;
        
        bread(bn, block_buf);
        int len = BLOCKSIZ - (offset % BLOCKSIZ);
        if (len > remaining) len = remaining;
        
        memcpy(buf + offset, block_buf + (offset % BLOCKSIZ), len);
        offset += len;
        remaining -= len;
    }
    
    buf[offset] = '\0';
    return offset;
}

/* 读取符号链接内容 */
int readlink(char *path, char *buf, int bufsize) {
    struct inode *ip = lookup_path_nofollow(path);
    if (ip == NULL) {
        printf("readlink: %s not found\n", path);
        return -1;
    }
    
    /* 检查是否是符号链接 */
    if (!is_link(ip)) {
        printf("readlink: %s is not a symbolic link\n", path);
        iput(ip);
        return -1;
    }
    
    int size = readlink_inode(ip, buf, bufsize);
    iput(ip);
    return size;
}

/* 删除链接 */
int fs_unlink(char *path) {
    struct inode *ip = lookup_path_nofollow(path);
    if (ip == NULL) {
        printf("unlink: %s not found\n", path);
        return -1;
    }
    
    /* 如果是目录，不能删除 */
    if (is_directory(ip)) {
        printf("unlink: %s is a directory\n", path);
        iput(ip);
        return -1;
    }
    
    /* 查找并删除目录项 */
    char dirname[DIRSIZ + 1];
    char filename[DIRSIZ + 1];
    char *last_slash = strrchr(path, '/');
    
    if (last_slash == NULL) {
        strncpy(dirname, ".", DIRSIZ);
        dirname[DIRSIZ] = '\0';
        strncpy(filename, path, DIRSIZ);
        filename[DIRSIZ] = '\0';
    } else {
        strncpy(dirname, path, last_slash - path);
        dirname[last_slash - path] = '\0';
        strncpy(filename, last_slash + 1, DIRSIZ);
        filename[DIRSIZ] = '\0';
    }
    
    struct inode *dir_ip = namei(dirname);
    if (dir_ip == NULL) {
        iput(ip);
        return -1;
    }
    
    int found = 0;
    int blkno = 0;
    int idx = 0;
    
    for (int i = 0; i < DIRNUM; i++) {
        blkno = bmap(dir_ip, i / (BLOCKSIZ / sizeof(struct direct)));
        if (blkno == 0) break;
        
        unsigned char buf[BLOCKSIZ];
        bread(blkno, buf);
        struct direct *dir_ptr = (struct direct *)buf;
        
        for (int j = 0; j < BLOCKSIZ / sizeof(struct direct); j++) {
            if (dir_ptr[j].d_ino == 0) continue;
            if (strcmp(dir_ptr[j].d_name, filename) == 0) {
                idx = j;
                found = 1;
                goto found_dirent;
            }
        }
    }
    
found_dirent:
    if (!found) {
        printf("unlink: %s not found\n", path);
        iput(ip);
        iput(dir_ip);
        return -1;
    }
    
    /* 删除目录项 */
    unsigned char buf[BLOCKSIZ];
    bread(blkno, buf);
    struct direct *dir_ptr = (struct direct *)buf;
    dir_ptr[idx].d_ino = 0;
    bwrite(blkno, buf);
    
    /* 减少链接计数 */
    ip->i_din.di_nlink--;
    
    /* 如果链接计数为 0，删除文件 */
    if (ip->i_din.di_nlink <= 0) {
        /* 释放数据块 */
        for (int i = 0; i < NADDR; i++) {
            if (ip->i_din.di_addr[i] != 0) {
                bfree(ip->i_din.di_addr[i]);
                ip->i_din.di_addr[i] = 0;
            }
        }
        ip->i_din.di_mode = 0;
        ip->i_din.di_size = 0;
        ip->i_flag |= 1;
        printf("unlink: %s deleted (last link)\n", path);
    } else {
        iput_inode(ip->i_ino, &ip->i_din);
        printf("unlink: %s removed (links remaining: %d)\n", path, ip->i_din.di_nlink);
    }
    
    int freed_ino = ip->i_ino;
    int should_free_inode = ip->i_din.di_nlink <= 0;
    iput(ip);
    if (should_free_inode) {
        ifree(freed_ino);
    }
    iput(dir_ip);
    return 0;
}

/* 判断是否是符号链接 */
int is_link(struct inode *ip) {
    if (ip == NULL) return 0;
    return (ip->i_din.di_mode & S_IFMT) == S_IFLNK;
}

/* 解析符号链接（将被 namei 使用） */
struct inode* resolve_link(struct inode *ip, int *err) {
    *err = 0;
    return ip; /* 解析将在 namei 中完成 */
}
