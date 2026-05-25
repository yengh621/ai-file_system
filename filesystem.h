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
#define KFS_MAX_TAGS 16
#define KFS_MAX_CAT 32
#define WORKLOAD_HISTORY 64
#define BEHAVIOR_HISTORY 128

#define SUPERBLOCK 1
#define DINODESTART 2
#define DATASTART (DINODESTART + DINODEBLK)

/* 权限常量 */
#define S_IFMT   0170000  /* 文件类型掩码 */
#define S_IFDIR  0040000  /* 目录 */
#define S_IFREG  0100000  /* 普通文件 */
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

/* AI 资源类型 */
#define AI_RESOURCE_NORMAL 0
#define AI_RESOURCE_MEMORY 1
#define AI_RESOURCE_TOOL 2
#define AI_RESOURCE_LOG 3

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
    /* AI 扩展字段 */
    unsigned short ai_context_blk_num;
    char ai_mount_flag;
    unsigned short ai_log_inode_no;
};

struct dinode {
    unsigned short di_mode;
    short di_uid;
    short di_gid;
    short di_nlink;
    unsigned long di_size;
    unsigned short di_addr[NADDR];
    unsigned short di_atime[2];
    unsigned short di_mtime[2];
    /* AI 扩展字段 */
    char ai_resource_type;
    unsigned long context_expire;
    char audit_flag;
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

/* AI 记忆结构体 */
struct ai_memory {
    unsigned short ino;
    int uid;
    char content[256];
    unsigned long timestamp;
    char is_long_term;
};

/* AI 工具映射结构体 */
struct ai_tool {
    char name[DIRSIZ];
    char script_path[64];
    int uid;
    char executable;
};

/* AI 审计日志结构体 */
struct ai_audit_log {
    int uid;
    unsigned long timestamp;
    char action[32];
    char resource_path[64];
    char result[32];
};

/* === 二、组织层创新：KFS 智能文件分类 === */
typedef enum {
    KFS_TYPE_CODE,
    KFS_TYPE_DOC,
    KFS_TYPE_IMAGE,
    KFS_TYPE_DATA,
    KFS_TYPE_OTHER,
    KFS_TYPE_MAX
} KFSType;

typedef enum {
    KFS_TIME_TODAY,
    KFS_TIME_WEEK,
    KFS_TIME_MONTH,
    KFS_TIME_OLD,
    KFS_TIME_MAX
} KFSTime;

/* 文件标签结构体 */
struct kfs_tag {
    unsigned short ino;
    char tags[KFS_MAX_TAGS][32];
    int tag_count;
    KFSType type;
    KFSTime time_cat;
    unsigned long last_access;
    unsigned long created;
};

/* 虚拟目录项 */
struct kfs_vdir_entry {
    char name[DIRSIZ];
    unsigned short ino;
};

/* === 三、性能层创新：AI 自适应 I/O 优化 === */
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

/* Workload 分析器 */
struct workload_analyzer {
    struct io_request history[WORKLOAD_HISTORY];
    int history_idx;
    WorkloadType current_type;
    int prefetch_window;  /* 预取窗口大小 */
    int cache_hits;
    int cache_misses;
};

/* === 四、安全层创新：智能行为异常检测 === */
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
int check_permission(struct inode *ip, int mode);
int is_directory(struct inode *ip);
int is_empty_directory(struct inode *ip);
int lock_file(unsigned short ino, int lock_type);
void unlock_file(unsigned short ino, int lock_type);
void init_file_locks();

/* AI 相关函数声明 */
void init_agent_dirs();
void add_memory(char *content, char is_long_term);
void list_memories();
void call_ai_tool(char *tool_name, char *args);
void log_ai_action(char *action, char *resource_path, char *result);
void cleanup_expired_memories();
void nlp_interact(char *text);

/* === 组织层创新：KFS 函数声明 === */
void init_kfs();
void kfs_classify_file(char *filename, unsigned short ino);
void kfs_list_virtual_dir(char *vdir_path);
void kfs_show_tags(unsigned short ino);

/* === 性能层创新：AI I/O 优化函数声明 === */
void init_workload_analyzer();
void record_io_request(unsigned short ino, int block_no, int is_read);
void analyze_workload();
int get_prefetch_window();
void show_io_stats();

/* === 安全层创新：行为检测函数声明 === */
void init_security_system();
void record_user_action(char *action, char *target);
int detect_anomaly(char *action, char *target);
void show_user_profile();
void show_security_events();

#endif
