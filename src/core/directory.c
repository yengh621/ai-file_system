#include "filesystem.h"

struct inode* namei(char *path) {
    char name[DIRSIZ];
    int i, j, len;
    struct inode *ip;
    if (path[0] == '/') {
        ip = iget(1);
        j = 1;
    } else {
        ip = iget(cur_dir);
        j = 0;
    }
    if (ip == NULL) return NULL;
    while (1) {
        while (path[j] == '/') j++;
        if (path[j] == '\0') return ip;
        len = 0;
        while (path[j] != '/' && path[j] != '\0' && len < DIRSIZ) {
            name[len++] = path[j++];
        }
        name[len] = '\0';
        if (len == 0) {
            iput(ip);
            return NULL;
        }
        if ((ip->i_din.di_mode & 040000) == 0) {
            iput(ip);
            return NULL;
        }
        unsigned long filesize = ip->i_din.di_size;
        struct direct dir;
        int found = 0;
        for (i = 0; i < (filesize + 15) / 16; i++) {
            int bn = bmap(ip, i / 32);
            if (bn == 0) break;
            bread(bn, block_buf);
            memcpy(&dir, block_buf + (i % 32) * 16, 16);
            if (strcmp(dir.d_name, name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            iput(ip);
            return NULL;
        }
        iput(ip);
        ip = iget(dir.d_ino);
        if (ip == NULL) return NULL;
    }
}

void dir(void) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    struct inode *ip = iget(cur_dir);
    if (ip == NULL) {
        printf("Directory not found.\n");
        return;
    }
    unsigned long filesize = ip->i_din.di_size;
    struct direct dir;
    int i;
    printf("Directory contents:\n");
    for (i = 0; i < (filesize + 15) / 16; i++) {
        int bn = bmap(ip, i / 32);
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (dir.d_ino != 0) {
            printf("%s (ino: %d)\n", dir.d_name, dir.d_ino);
        }
    }
    iput(ip);
}
