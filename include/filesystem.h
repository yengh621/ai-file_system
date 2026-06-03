#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define BLOCKSIZ 512
#define DINODESIZ 32
#define DINODEBLK 32
#define FILEBLK 512
#define NICFREE 50
#define NICINOD 50
#define NADDR 10
#define USERNUM 8
#define DIRSIZ 14
#define DIRNUM 128
#define SYSOPENFILE 40
#define NOFILE 20
#define NHINO 128
#define WORKLOAD_HISTORY 64
#define BEHAVIOR_HISTORY 128
#define MAX_INODES 512
#define BLOCK_SIZE 512

#define SUPERBLOCK 1
#define DINODESTART 2
#define KFS_START (DINODESTART + DINODEBLK)   /* KFS 开始块 */
#define KFS_TAGS_BLK KFS_START               /* KFS 分类标签块 */
#define KFS_HOT_CACHE_BLK (KFS_START + 1)   /* 热点文件索引块 */
#define KFS_MEMORY_MAP_BLK (KFS_START + 2)  /* 内存内容文件块 */
#define KFS_HOT_DATA_BLK (KFS_START + 3)    /* 热点文件数据起始块 */
#define KFS_TOTAL_BLKS 32                   /* KFS 总块数 */
#define DATASTART (KFS_START + KFS_TOTAL_BLKS)

/* 权限常量 */
#define S_IFMT   0170000  /* 文件类型掩码 */
#define S_IFDIR  0040000  /* 目录 */
#define S_IFREG  0100000  /* 普通文件 */
#define S_IFLNK  0120000  /* 符号链接 */
#define S_IRUSR  0000400  /* 所有者读 */
#define S_IWUSR  0000200  /* 所有者写 */
#define S_IXUSR  0000100  /* 所有者执行 */
#define S_IRGRP  0000040  /* 组读 */
#define S_IWGRP  0000020  /* 组写 */
#define S_IXGRP  0000010  /* 组执行 */
#define S_IROTH  0000004  /* 其他读 */
#define S_IWOTH  0000002  /* 其他写 */
#define S_IXOTH  0000001  /* 其他执行 */
#define S_IRWXU  (S_IRUSR | S_IWUSR | S_IXUSR)
#define S_IRWXG  (S_IRGRP | S_IWGRP | S_IXGRP)
#define S_IRWXO  (S_IROTH | S_IWOTH | S_IXOTH)

/* 打开模式 */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

/* 权限检查类型 */
#define R_OK 0x4
#define W_OK 0x2
#define X_OK 0x1


struct filsys {
    unsigned short s_isize;
    unsigned short s_fsize;
    unsigned short s_nfree;
    unsigned short s_free[NICFREE];
    unsigned short s_ninode;
    unsigned short s_inode[NICINOD];
    char s_flock;
    char s_ilock;
    char s_fmod;
    char s_ronly;
    unsigned short s_time[2];
    unsigned short ai_context_blk_num;
    unsigned char ai_mount_flag;
    unsigned short ai_log_inode_no;
};

#define ROOTDEV 0

struct dinode {
    unsigned short di_mode;
    short di_uid;
    short di_gid;
    short di_nlink;
    unsigned long di_size;
    unsigned short di_addr[NADDR];
    unsigned short di_atime[2];
    unsigned short di_mtime[2];
};

struct inode {
    struct dinode i_din;
    unsigned short i_ino;
    unsigned short i_count;
    char i_flag;
    char i_lock;
    struct inode *i_forw;
    struct inode *i_back;
    unsigned long i_offset;
};

struct direct {
    char d_name[DIRSIZ];
    unsigned short d_ino;
};

struct user {
    char u_name[DIRSIZ];
    char u_passwd[DIRSIZ];
    unsigned short u_uid;
    unsigned short u_gid;
};

struct file {
    char f_flag;
    char f_count;
    struct inode *f_inode;
    unsigned long f_offset;
    char f_name[DIRSIZ];  /* 保存文件名用于 KFS */
};

/* 文件锁状态 */
#define LOCK_NONE 0
#define LOCK_READ 1
#define LOCK_WRITE 2

struct file_lock {
    unsigned short ino;      /* 被锁的 inode 号 */
    int lock_type;           /* 锁类型：读或写 */
    int owner_uid;           /* 锁所有者 UID */
    int read_count;          /* 读锁计数 */
};

/* === 二、性能层创新：AI 自适应 I/O 优化 === */
typedef enum {
    WORKLOAD_SEQUENTIAL,  /* 顺序读大文件 */
    WORKLOAD_RANDOM,      /* 随机读小文件 */
    WORKLOAD_STREAM,      /* 多媒体流 */
    WORKLOAD_UNKNOWN
} WorkloadType;

/* I/O 请求记录 */
struct io_request {
    unsigned short ino;
    int block_no;
    int is_read;
    unsigned long timestamp;
};

/* 预取缓存项 */
struct prefetch_cache_entry {
    unsigned short ino;  /* 文件 inode */
    int block_no;        /* 块号 */
    unsigned char data[BLOCK_SIZE];  /* 数据 */
    int valid;           /* 是否有效 */
    unsigned long timestamp;  /* 加载时间 */
};

/* 预取缓存大小 */
#define PREFETCH_CACHE_SIZE 32

/* KFS 热点文件缓存大小 */
#define HOT_FILE_CACHE_SIZE 16

/* 热点文件缓存项（增强版 - 含长短期记忆） */
struct hot_file_entry {
    char filename[DIRSIZ];
    unsigned short ino;
    int access_count;
    unsigned long last_access;
    float long_term_score;    /* 长期评分 */
    float short_term_score;   /* 短期评分 */
    unsigned long first_access; /* 首次访问时间 */
    int stored_in_kfs;         /* 是否已存储在 KFS 磁盘 */
    int kfs_data_start_blk;    /* KFS 数据起始块 */
    int kfs_data_blk_count;    /* KFS 数据块数 */
};

/* KFS 磁盘头部结构 */
struct kfs_disk_header {
    char magic[8];            /* 魔数："KFS_V1" */
    int version;              /* 版本 */
    int tag_count;            /* 分类标签数量 */
    int hot_file_count;       /* 热点文件数量 */
    unsigned long last_update; /* 最后更新时间 */
};

/* 单个文件的 IO 历史 */
struct file_io_history {
    unsigned short ino;              /* 文件 inode */
    int block_history[WORKLOAD_HISTORY]; /* 访问过的块号 */
    int history_idx;
    WorkloadType current_type;       /* 该文件的工作负载类型 */
    int prefetch_window;             /* 该文件的预取窗口 */
    int last_block;                  /* 最后访问的块 */
    int total_reads;                 /* 总读取次数 */
};

/* 全局 Workload 分析器 */
struct workload_analyzer {
    struct file_io_history files[MAX_INODES]; /* 每个文件的独立历史 */
    int file_count;
    
    /* 全局预取缓存（按块存） */
    struct prefetch_cache_entry cache[PREFETCH_CACHE_SIZE];
    int cache_idx;
    int cache_hits;
    int cache_misses;
};

/* === 三、安全层创新：智能行为异常检测 === */
/* 用户行为记录 */
struct user_action {
    char action[32];
    unsigned long timestamp;
    char target[64];
};

/* 用户行为画像 */
struct user_profile {
    int uid;
    struct user_action history[BEHAVIOR_HISTORY];
    int history_idx;
    
    /* 正常行为统计 */
    int avg_delete_per_min;
    int avg_modify_per_min;
    char usual_dirs[16][64];
    int usual_dir_count;
    
    /* 异常检测阈值 */
    int delete_threshold;
    int modify_threshold;
};

/* 安全事件 */
struct security_event {
    int uid;
    char event_type[32];
    char description[256];
    unsigned long timestamp;
    int blocked;
};

extern struct file_lock file_locks[SYSOPENFILE];

extern struct filsys sb;
extern struct inode *inode[NHINO];
extern struct user user[USERNUM];
extern struct file sysopenfile[SYSOPENFILE];
extern int cur_uid;
extern unsigned short cur_dir;
extern int u_ofile[NOFILE];
extern unsigned char block_buf[BLOCKSIZ];

void bread(int blkno, unsigned char *buf);
void bwrite(int blkno, unsigned char *buf);
void iget_inode(int ino, struct dinode *di);
void iput_inode(int ino, struct dinode *di);
struct inode* iget(int ino);
void iput(struct inode *p);
int ialloc(void);
void ifree(int ino);
int balloc(void);
void bfree(int blkno);
int bmap(struct inode *ip, int lbn);
struct inode* namei(char *path);
void format(void);
void load_vdisk(void);
void save_vdisk(void);
void login(void);
void logout(void);
int get_current_user_gid(void);
int find_user_index_by_name(char *name);
void create(char *name);
void delete(char *name);
int open(char *name, int mode);
void close(int fd);
int read(int fd, unsigned char *buf, int count);
int write(int fd, unsigned char *buf, int count);
void mkdir(char *name);
void chdir(char *name);
void dir(void);
void rmdir(char *name);
void chmod(char *name, unsigned short mode);
void grant(char *path, char *username, int writable);
int check_permission(struct inode *ip, int mode);
int is_directory(struct inode *ip);
int is_empty_directory(struct inode *ip);
int lock_file(unsigned short ino, int lock_type);
void unlock_file(unsigned short ino, int lock_type);
void init_file_locks();

/* === KFS 热点文件缓存函数声明 === */
void init_kfs();
unsigned short kfs_hot_cache_lookup(char *filename);
void kfs_hot_cache_update(char *filename, unsigned short ino);
void kfs_hot_cache_show();

/* KFS 持久化 */
void kfs_save_to_disk();
void kfs_load_from_disk();

/* KFS AI 长短期记忆 */
void kfs_calculate_scores(struct hot_file_entry *entry);
void kfs_load_hot_files_from_ai();

/* KFS 热点文件存储 */
int kfs_store_hot_file(char *filename, unsigned short ino);
int kfs_read_hot_file(char *filename, unsigned char *buf);
int kfs_read_hot_file_block(char *filename, int block_index, unsigned char *buf);
int kfs_is_file_hot(char *filename);
void kfs_remove_file(char *filename, unsigned short ino);

/* KFS 内存内容文件 */
void kfs_update_memory_map();
void kfs_show_memory_map();
void kfs_print_directory_entries();

/* KFS 导出给 AI */
void export_kfs_stats_to_ai();

/* KFS 虚拟目录和标签 */
void kfs_list_virtual_dir(char *vdir);
void kfs_show_tags(int ino);
void kfs_ai_select_hot_files();

/* === 性能层创新：AI I/O 优化函数声明 === */
void init_workload_analyzer();
void record_io_request(unsigned short ino, int block_no, int is_read);
int get_prefetch_window_for_file(unsigned short ino);
void show_io_stats();
void set_prefetch_window(int window);
void export_io_stats_to_ai();
int load_file_prefetch_window_from_ai(unsigned short ino);

/* 预取缓存函数（Per-File） */
int get_prefetched_block(unsigned short ino, int block_no, unsigned char *buf);

/* === 安全层创新：行为检测函数声明 === */
void init_security_system();
void record_user_action(char *action, char *target);
int detect_anomaly(char *action, char *target);
void show_user_profile();
void show_security_events();
void set_security_thresholds(int delete_thresh, int modify_thresh);
void set_delete_threshold(int threshold);
void set_modify_threshold(int threshold);

/* 获取块使用状态 */
void print_block_status();

/* === 链接系统 === */
int link(char *oldpath, char *newpath);              /* 创建硬链接 */
int symlink(char *oldpath, char *newpath);           /* 创建符号链接 */
int readlink(char *path, char *buf, int bufsize);    /* 读取符号链接内容 */
int fs_unlink(char *path);                           /* 删除链接 */
int is_link(struct inode *ip);                       /* 判断是否是符号链接 */
int readlink_inode(struct inode *ip, char *buf, int bufsize); /* 从 inode 读取链接 */
struct inode* resolve_link(struct inode *ip, int *err);  /* 解析符号链接 */
int copy_file_command(char *source_path, char *target_path);
int move_file_command(char *source_path, char *target_path);
int rename_path_command(char *source_path, char *target_path);

/* === 集成层：记忆优化集成 === */
void init_integration();
void integration_set_user(int uid);
void integration_clear_user();
void integration_record_operation(char *operation, char *path);
void integration_apply_optimization();
void integration_show_suggestions();
void integration_start_session();
void integration_end_session();

#endif
