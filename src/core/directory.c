#include "filesystem.h"

/* 从 inode 直接读取符号链接内容（外部声明） */
extern int readlink_inode(struct inode *ip, char *buf, int bufsize);

struct inode* namei(char *path) {
    char name[DIRSIZ];
    int i, j, len;
    struct inode *ip;
    int link_count = 0;  /* 防止链接循环 */
    char current_path[512]; /* 保存当前路径 */
    
    /* 复制初始路径 */
    strncpy(current_path, path, sizeof(current_path)-1);
    current_path[sizeof(current_path)-1] = '\0';

search_start:
    if (current_path[0] == '/') {
        ip = iget(1);
        j = 1;
    } else {
        ip = iget(cur_dir);
        j = 0;
    }
    if (ip == NULL) return NULL;

    while (1) {
        /* 跳过开头的斜杠 */
        while (current_path[j] == '/') j++;
        if (current_path[j] == '\0') {
            return ip;
        }

        /* 提取下一个路径组件 */
        len = 0;
        while (current_path[j] != '/' && current_path[j] != '\0' && len < DIRSIZ - 1) {
            name[len++] = current_path[j++];
        }
        name[len] = '\0';

        if (len == 0) {
            iput(ip);
            return NULL;
        }

        /* 检查当前 inode 是否是目录 */
        if (!is_directory(ip)) {
            iput(ip);
            return NULL;
        }

        /* 在目录中查找名字 */
        unsigned long filesize = ip->i_din.di_size;
        struct direct dir;
        int found = 0;
        int found_ino = 0;

        for (i = 0; i < (filesize + 15) / 16; i++) {
            int bn = bmap(ip, i / 32);
            if (bn == 0) break;
            bread(bn, block_buf);
            memcpy(&dir, block_buf + (i % 32) * 16, 16);
            if (strcmp(dir.d_name, name) == 0) {
                found = 1;
                found_ino = dir.d_ino;
                break;
            }
        }

        if (!found) {
            iput(ip);
            return NULL;
        }

        /* 获取下一个 inode */
        struct inode *next_ip = iget(found_ino);
        iput(ip);

        if (next_ip == NULL) {
            return NULL;
        }

        /* 检查是否是符号链接 */
        if (is_link(next_ip)) {
            link_count++;
            if (link_count > 8) {
                printf("namei: too many symbolic links\n");
                iput(next_ip);
                return NULL;
            }

            /* 读取链接内容 */
            char link_target[DIRSIZ + 1];
            int read_len = readlink_inode(next_ip, link_target, sizeof(link_target));
            iput(next_ip);

            if (read_len <= 0) {
                return NULL;
            }

            /* 构建新的路径并重新开始搜索 */
            if (link_target[0] == '/') {
                strncpy(current_path, link_target, sizeof(current_path)-1);
            } else {
                /* 相对路径：组合当前路径和链接 */
                /* 这里简化处理，直接使用链接内容作为新路径 */
                strncpy(current_path, link_target, sizeof(current_path)-1);
            }
            goto search_start;
        }

        ip = next_ip;
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
            struct inode *fip = iget(dir.d_ino);
            if (fip != NULL) {
                char type_char = ' ';
                char link_info[64] = "";

                if (is_directory(fip)) {
                    type_char = 'd';
                } else if (is_link(fip)) {
                    type_char = 'l';
                    /* 直接从 inode 读取链接内容 */
                    char target[DIRSIZ + 1];
                    if (readlink_inode(fip, target, sizeof(target)) >= 0) {
                        snprintf(link_info, sizeof(link_info), " -> %s", target);
                    }
                } else {
                    type_char = '-';
                }

                printf("%c %s (ino: %d, links: %d)%s\n",
                       type_char, dir.d_name, dir.d_ino,
                       fip->i_din.di_nlink, link_info);
                iput(fip);
            } else {
                printf("- %s (ino: %d)\n", dir.d_name, dir.d_ino);
            }
        }
    }
    iput(ip);
}
