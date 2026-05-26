#include "filesystem.h"

struct user user[USERNUM];
int cur_uid = -1;
unsigned short cur_dir = 1;

/* 辅助函数：在指定目录中创建新目录 */
static void create_dir_in(struct inode *parent, char *name, int uid, int gid, int mode) {
    unsigned long filesize = parent->i_din.di_size;
    struct direct dir;
    int i, slot = -1;
    
    for (i = 0; i < (filesize + 15) / 16; i++) {
        int bn = bmap(parent, i / 32);
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (strcmp(dir.d_name, name) == 0) {
            return;
        }
        if (slot == -1 && dir.d_ino == 0) slot = i;
    }
    
    int ino = ialloc();
    if (ino == 0) return;
    
    struct inode *ip = iget(ino);
    ip->i_din.di_mode = mode;
    ip->i_din.di_uid = uid;
    ip->i_din.di_gid = gid;
    ip->i_din.di_nlink = 2;
    ip->i_din.di_size = 32;
    
    int bn = balloc();
    if (bn == 0) {
        iput(ip);
        ifree(ino);
        return;
    }
    ip->i_din.di_addr[0] = bn;
    
    memset(block_buf, 0, BLOCKSIZ);
    struct direct *dirp = (struct direct*)block_buf;
    strcpy(dirp[0].d_name, ".");
    dirp[0].d_ino = ino;
    strcpy(dirp[1].d_name, "..");
    dirp[1].d_ino = parent->i_ino;
    bwrite(bn, block_buf);
    
    if (slot == -1) slot = (filesize + 15) / 16;
    bn = bmap(parent, slot / 32);
    if (bn == 0) {
        iput(ip);
        ifree(ino);
        return;
    }
    
    bread(bn, block_buf);
    dirp = (struct direct*)(block_buf + (slot % 32) * 16);
    strncpy(dirp->d_name, name, DIRSIZ - 1);
    dirp->d_name[DIRSIZ - 1] = '\0';
    dirp->d_ino = ino;
    bwrite(bn, block_buf);
    
    if ((slot + 1) * 16 > filesize) {
        parent->i_din.di_size = (slot + 1) * 16;
    }
    
    iput(ip);
    parent->i_din.di_nlink++;
}

void format(void) {
    int i, j;
    memset(block_buf, 0, BLOCKSIZ);
    for (i = 0; i < DATASTART + FILEBLK; i++) {
        bwrite(i, block_buf);
    }
    sb.s_isize = DINODEBLK;
    sb.s_fsize = DATASTART + FILEBLK;
    sb.s_nfree = NICFREE;
    for (i = 0; i < NICFREE; i++) {
        sb.s_free[i] = DATASTART + FILEBLK - 1 - i;
    }
    for (i = NICFREE; i < FILEBLK - 1; i += NICFREE) {
        int blkno = DATASTART + FILEBLK - 1 - i;
        unsigned short n = (FILEBLK - 1 - i >= NICFREE) ? NICFREE : FILEBLK - 1 - i;
        memcpy(block_buf, &n, sizeof(unsigned short));
        for (j = 0; j < n; j++) {
            ((unsigned short*)block_buf)[j + 1] = blkno - 1 - j;
        }
        bwrite(blkno, block_buf);
    }
    sb.s_ninode = NICINOD;
    for (i = 0; i < NICINOD; i++) {
        sb.s_inode[i] = 32 - i;
    }
    /* 初始化 AI 相关字段 */
    sb.ai_context_blk_num = 0;
    sb.ai_mount_flag = 0;
    sb.ai_log_inode_no = 0;
    sb.s_fmod = 1;
    memcpy(block_buf + 2, &sb, sizeof(sb));
    bwrite(SUPERBLOCK, block_buf);
    struct inode *root_dir = iget(1);
    root_dir->i_din.di_mode = S_IFDIR | 0755;
    root_dir->i_din.di_uid = 0;
    root_dir->i_din.di_gid = 0;
    root_dir->i_din.di_nlink = 1;
    root_dir->i_din.di_size = 32;
    int bn = balloc();
    root_dir->i_din.di_addr[0] = bn;
    memset(block_buf, 0, BLOCKSIZ);
    struct direct *dir = (struct direct*)block_buf;
    strcpy(dir[0].d_name, ".");
    dir[0].d_ino = 1;
    strcpy(dir[1].d_name, "..");
    dir[1].d_ino = 1;
    bwrite(bn, block_buf);
    
    /* 初始化用户：root + 其他用户 */
    strcpy(user[0].u_name, "root");
    strcpy(user[0].u_passwd, "123456");
    user[0].u_uid = 0;
    user[0].u_gid = 0;
    
    for (i = 1; i < USERNUM; i++) {
        sprintf(user[i].u_name, "usr%d", i);
        strcpy(user[i].u_passwd, "123456");
        user[i].u_uid = 99 + i;
        user[i].u_gid = 100;
    }
    
    /* 创建 usr 目录 */
    create_dir_in(root_dir, "usr", 0, 0, S_IFDIR | 0755);
    
    /* 创建每个用户的家目录 */
    iput(root_dir);
    struct inode *usr_dir = namei("/usr");
    if (usr_dir != NULL) {
        for (i = 1; i < USERNUM; i++) {
            create_dir_in(usr_dir, user[i].u_name, user[i].u_uid, user[i].u_gid, S_IFDIR | 0755);
        }
        iput(usr_dir);
    }
    
    /* 创建 AI Agent 目录结构 */
    struct inode *root_dir2 = iget(1);
    if (root_dir2 != NULL) {
        create_dir_in(root_dir2, "agent", 0, 0, S_IFDIR | 0755);
        iput(root_dir2);
        
        struct inode *agent_dir = namei("/agent");
        if (agent_dir != NULL) {
            create_dir_in(agent_dir, "memory", 0, 0, S_IFDIR | 0755);
            create_dir_in(agent_dir, "tools", 0, 0, S_IFDIR | 0755);
            create_dir_in(agent_dir, "log", 0, 0, S_IFDIR | 0755);
            
            struct inode *memory_dir = namei("/agent/memory");
            if (memory_dir != NULL) {
                create_dir_in(memory_dir, "short_term", 0, 0, S_IFDIR | 0777);
                create_dir_in(memory_dir, "long_term", 0, 0, S_IFDIR | 0777);
                iput(memory_dir);
            }
            
            iput(agent_dir);
        }
    }
    
    /* 创建 KFS 智能文件系统目录结构 */
    struct inode *root_dir3 = iget(1);
    if (root_dir3 != NULL) {
        create_dir_in(root_dir3, "kfs", 0, 0, S_IFDIR | 0755);
        iput(root_dir3);
        
        struct inode *kfs_dir = namei("/kfs");
        if (kfs_dir != NULL) {
            create_dir_in(kfs_dir, "type", 0, 0, S_IFDIR | 0755);
            create_dir_in(kfs_dir, "time", 0, 0, S_IFDIR | 0755);
            create_dir_in(kfs_dir, "project", 0, 0, S_IFDIR | 0755);
            
            struct inode *type_dir = namei("/kfs/type");
            if (type_dir != NULL) {
                create_dir_in(type_dir, "code", 0, 0, S_IFDIR | 0755);
                create_dir_in(type_dir, "document", 0, 0, S_IFDIR | 0755);
                create_dir_in(type_dir, "image", 0, 0, S_IFDIR | 0755);
                create_dir_in(type_dir, "data", 0, 0, S_IFDIR | 0755);
                iput(type_dir);
            }
            
            struct inode *time_dir = namei("/kfs/time");
            if (time_dir != NULL) {
                create_dir_in(time_dir, "today", 0, 0, S_IFDIR | 0755);
                create_dir_in(time_dir, "this_week", 0, 0, S_IFDIR | 0755);
                create_dir_in(time_dir, "recent_30days", 0, 0, S_IFDIR | 0755);
                iput(time_dir);
            }
            
            iput(kfs_dir);
        }
    }
    
    sb.ai_mount_flag = 1;
    printf("Format completed.\n");
}

void load_vdisk(void) {
    FILE *fp = fopen("vdisk.bin", "rb");
    if (fp == NULL) {
        printf("Virtual disk not found, formatting...\n");
        format();
        return;
    }
    bread(SUPERBLOCK, block_buf);
    memcpy(&sb, block_buf + 2, sizeof(sb));
    strcpy(user[0].u_name, "root");
    strcpy(user[0].u_passwd, "123456");
    user[0].u_uid = 0;
    user[0].u_gid = 0;
    for (int i = 1; i < USERNUM; i++) {
        sprintf(user[i].u_name, "usr%d", i);
        strcpy(user[i].u_passwd, "123456");
        user[i].u_uid = 99 + i;
        user[i].u_gid = 100;
    }
}

void save_vdisk(void) {
    if (sb.s_fmod) {
        bread(SUPERBLOCK, block_buf);
        memcpy(block_buf + 2, &sb, sizeof(sb));
        bwrite(SUPERBLOCK, block_buf);
        sb.s_fmod = 0;
    }
}

void login(void) {
    char name[DIRSIZ], passwd[DIRSIZ], home_path[64];
    printf("Username: ");
    scanf("%s", name);
    printf("Password: ");
    scanf("%s", passwd);
    for (int i = 0; i < USERNUM; i++) {
        if (strcmp(user[i].u_name, name) == 0 && strcmp(user[i].u_passwd, passwd) == 0) {
            cur_uid = user[i].u_uid;
            /* 切换到用户家目录 */
            if (cur_uid == 0) {
                /* root 在 / 目录 */
                cur_dir = 1;
            } else {
                sprintf(home_path, "/usr/%s", user[i].u_name);
                struct inode *home_dir = namei(home_path);
                if (home_dir != NULL) {
                    cur_dir = home_dir->i_ino;
                    iput(home_dir);
                } else {
                    cur_dir = 1;
                }
            }
            printf("Login successful.\n");
            /* 激活多智能体系统 */
            integration_set_user(cur_uid);
            return;
        }
    }
    printf("Login failed.\n");
}

void logout(void) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    /* 停止多智能体系统 */
    integration_clear_user();
    for (int i = 0; i < NOFILE; i++) {
        if (u_ofile[i] != -1) {
            struct file *f = &sysopenfile[u_ofile[i]];
            f->f_count--;
            if (f->f_count == 0) {
                iput(f->f_inode);
            }
            u_ofile[i] = -1;
        }
    }
    cur_uid = -1;
    printf("Logout successful.\n");
}

void mkdir(char *name) {
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
            printf("Directory exists.\n");
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
    ip->i_din.di_mode = S_IFDIR | 0755;
    ip->i_din.di_uid = cur_uid;
    ip->i_din.di_gid = 100;
    ip->i_din.di_nlink = 2;
    ip->i_din.di_size = 32;
    int bn = balloc();
    ip->i_din.di_addr[0] = bn;
    memset(block_buf, 0, BLOCKSIZ);
    struct direct *dirp = (struct direct*)block_buf;
    strcpy(dirp[0].d_name, ".");
    dirp[0].d_ino = ino;
    strcpy(dirp[1].d_name, "..");
    dirp[1].d_ino = cur_dir;
    bwrite(bn, block_buf);
    if (slot == -1) slot = (filesize + 15) / 16;
    bn = bmap(dip, slot / 32);
    if (bn == 0) {
        iput(ip);
        ifree(ino);
        iput(dip);
        return;
    }
    bread(bn, block_buf);
    dirp = (struct direct*)(block_buf + (slot % 32) * 16);
    strncpy(dirp->d_name, name, DIRSIZ - 1);
    dirp->d_name[DIRSIZ - 1] = '\0';
    dirp->d_ino = ino;
    bwrite(bn, block_buf);
    if ((slot + 1) * 16 > filesize) {
        dip->i_din.di_size = (slot + 1) * 16;
    }
    iput(ip);
    dip->i_din.di_nlink++;
    iput(dip);
    printf("Mkdir successful.\n");
}

void rmdir(char *name) {
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
        printf("Directory not found.\n");
        iput(dip);
        return;
    }
    
    struct inode *ip = iget(ino);
    
    /* 检查是否是目录 */
    if (!is_directory(ip)) {
        printf("Not a directory.\n");
        iput(ip);
        iput(dip);
        return;
    }
    
    /* 检查是否是空目录 */
    if (!is_empty_directory(ip)) {
        printf("Directory not empty.\n");
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
    
    /* 清理目录项 */
    int bn = bmap(dip, i / 32);
    bread(bn, block_buf);
    struct direct *dirp = (struct direct*)(block_buf + (i % 32) * 16);
    dirp->d_ino = 0;
    memset(dirp->d_name, 0, DIRSIZ);
    bwrite(bn, block_buf);
    
    /* 释放目录i节点和块 */
    for (int j = 0; j < (ip->i_din.di_size + BLOCKSIZ - 1) / BLOCKSIZ; j++) {
        int dbn = bmap(ip, j);
        if (dbn != 0) bfree(dbn);
    }
    ifree(ino);
    
    iput(ip);
    dip->i_din.di_nlink--;
    iput(dip);
    
    printf("Rmdir successful.\n");
}

void chdir(char *name) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    struct inode *ip = namei(name);
    if (ip == NULL) {
        printf("Directory not found.\n");
        return;
    }
    if (!is_directory(ip)) {
        printf("Not directory.\n");
        iput(ip);
        return;
    }
    
    /* 检查目录的执行权限 */
    if (check_permission(ip, X_OK) != 0) {
        printf("Permission denied.\n");
        iput(ip);
        return;
    }
    
    cur_dir = ip->i_ino;
    iput(ip);
    printf("Chdir successful.\n");
}
