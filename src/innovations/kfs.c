#include "filesystem.h"

/* ========================================
   KFS 核心数据结构
   ======================================== */

/* 热点文件缓存 - 存储高频访问文件的元数据 */
static struct hot_file_entry hot_file_cache[HOT_FILE_CACHE_SIZE];
static int hot_cache_initialized = 0;  /* 标记缓存是否已初始化 */

/* Per-user access scores exported to JSON. This is not the system KFS index. */
static struct hot_file_entry user_hot_stats[HOT_FILE_CACHE_SIZE];
static int user_hot_stats_uid = -2;
static int user_hot_stats_loaded = 0;

static unsigned int kfs_user_bit(int uid) {
    for (int i = 0; i < USERNUM; i++) {
        if (user[i].u_uid == uid) return 1u << i;
    }
    return 0;
}

/* 内存文件映射表 - 记录当前在KFS中的文件列表 */
static char memory_file_map[HOT_FILE_CACHE_SIZE][DIRSIZ];
static int memory_file_count = 0;

/* 每个热点文件在KFS数据区占用的块数 */
#define KFS_HOT_FILE_BLOCKS ((KFS_TOTAL_BLKS - 3) / HOT_FILE_CACHE_SIZE)
#define KFS_RAM_BLOCK_CACHE_SIZE 8  /* RAM块缓存大小 */

/* RAM块缓存条目 - 用于快速访问热点文件的数据块 */
struct kfs_ram_block_entry {
    unsigned short ino;         /* 文件的inode号 */
    int block_index;           /* 文件内的块索引 */
    int valid;                 /* 条目是否有效 */
    unsigned long last_access; /* 最后访问时间戳（LRU用） */
    unsigned char data[BLOCKSIZ];  /* 块数据副本 */
};

static struct kfs_ram_block_entry kfs_ram_block_cache[KFS_RAM_BLOCK_CACHE_SIZE];
static unsigned long kfs_ram_access_clock = 0;  /* 访问时钟，用于LRU替换 */
static unsigned short ai_selected_inodes[HOT_FILE_CACHE_SIZE];  /* AI选择的热点文件 */
static int ai_selected_inode_count = 0;

/* ========================================
   一、RAM块缓存管理模块
   ======================================== */

/**
 * 清空RAM块缓存
 * 重置所有缓存条目，将访问时钟归零
 */
static void kfs_clear_ram_block_cache(void) {
    memset(kfs_ram_block_cache, 0, sizeof(kfs_ram_block_cache));
    kfs_ram_access_clock = 0;
}

/**
 * 使指定文件的所有RAM块缓存失效
 * 当文件被修改或删除时调用，确保缓存一致性
 * 
 * @param ino 要失效缓存的文件inode号
 */
static void kfs_invalidate_ram_blocks(unsigned short ino) {
    for (int i = 0; i < KFS_RAM_BLOCK_CACHE_SIZE; i++) {
        if (kfs_ram_block_cache[i].valid && kfs_ram_block_cache[i].ino == ino) {
            memset(&kfs_ram_block_cache[i], 0, sizeof(kfs_ram_block_cache[i]));
        }
    }
}

/**
 * 从RAM缓存中获取数据块
 * 实现LRU算法：每次命中时更新访问时间
 * 
 * @param ino 文件inode号
 * @param block_index 文件内的块索引
 * @param buf 输出缓冲区，用于存储块数据
 * @return 1表示缓存命中，0表示未命中
 */
static int kfs_get_ram_block(unsigned short ino, int block_index, unsigned char *buf) {
    for (int i = 0; i < KFS_RAM_BLOCK_CACHE_SIZE; i++) {
        struct kfs_ram_block_entry *entry = &kfs_ram_block_cache[i];
        if (entry->valid && entry->ino == ino && entry->block_index == block_index) {
            entry->last_access = ++kfs_ram_access_clock;  /* 更新访问时间 */
            memcpy(buf, entry->data, BLOCKSIZ);
            return 1;
        }
    }
    return 0;
}

/**
 * 检查文件是否被AI选择为热点文件
 * 
 * @param ino 文件inode号
 * @return 1表示是AI选择的，0表示不是
 */
static int kfs_is_ai_selected(unsigned short ino) {
    for (int i = 0; i < ai_selected_inode_count; i++) {
        if (ai_selected_inodes[i] == ino) return 1;
    }
    return 0;
}

/**
 * 记录AI选择的热点文件
 * 
 * @param ino AI选择的文件inode号
 */
static void kfs_remember_ai_selection(unsigned short ino) {
    if (ino == 0 || kfs_is_ai_selected(ino) ||
        ai_selected_inode_count >= HOT_FILE_CACHE_SIZE) {
        return;
    }
    ai_selected_inodes[ai_selected_inode_count++] = ino;
}

/**
 * 计算RAM块的保留评分
 * 评分综合考虑AI热度、文件热度和访问时间
 * 评分越高，该块越应该被保留在缓存中
 * 
 * @param entry RAM块缓存条目
 * @return 保留评分，分数越高越应该保留
 */
static float kfs_ram_retention_score(const struct kfs_ram_block_entry *entry) {
    float ai_score = 0.0f;
    unsigned long age = kfs_ram_access_clock - entry->last_access;

    /* 获取文件的AI热度评分 */
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == entry->ino) {
            kfs_calculate_scores(&hot_file_cache[i]);
            ai_score = hot_file_cache[i].short_term_score * 0.65f
                     + hot_file_cache[i].long_term_score * 0.35f;
            break;
        }
    }

    /* AI热度优先；访问时间仅用于打破平局和淘汰过期块 */
    return (kfs_is_ai_selected(entry->ino) ? 1000.0f : 0.0f)  /* AI选择的文件加1000分 */
         + ai_score
         - (float)(age > 100 ? 100 : age) * 0.25f;
}

/**
 * 将数据块存入RAM缓存
 * 使用智能替换策略：优先保留AI选择的文件和热文件
 * 
 * @param ino 文件inode号
 * @param block_index 文件内的块索引
 * @param buf 要存储的块数据
 */
static void kfs_put_ram_block(
    unsigned short ino,
    int block_index,
    const unsigned char *buf
) {
    int replace_idx = -1;
    float lowest_retention = 0.0f;

    /* 查找替换位置 */
    for (int i = 0; i < KFS_RAM_BLOCK_CACHE_SIZE; i++) {
        struct kfs_ram_block_entry *entry = &kfs_ram_block_cache[i];
        
        /* 检查是否已存在相同块 */
        if (entry->valid && entry->ino == ino && entry->block_index == block_index) {
            replace_idx = i;
            break;
        }
        /* 找到空条目 */
        if (!entry->valid) {
            replace_idx = i;
            break;
        }
        /* 计算保留评分，找到评分最低的条目 */
        float retention = kfs_ram_retention_score(entry);
        if (replace_idx == -1 || retention < lowest_retention) {
            replace_idx = i;
            lowest_retention = retention;
        }
    }

    if (replace_idx < 0) return;

    /* 更新缓存条目 */
    kfs_ram_block_cache[replace_idx].ino = ino;
    kfs_ram_block_cache[replace_idx].block_index = block_index;
    kfs_ram_block_cache[replace_idx].valid = 1;
    kfs_ram_block_cache[replace_idx].last_access = ++kfs_ram_access_clock;
    memcpy(kfs_ram_block_cache[replace_idx].data, buf, BLOCKSIZ);
}

/* ========================================
   二、辅助函数和JSON解析模块
   ======================================== */

/**
 * 确保AI统计目录存在
 * 创建debug_memory及其子目录结构
 */
static void ensure_ai_stats_dirs(void) {
    char command[256];

    if (cur_uid != -1) {
#ifdef _WIN32
        snprintf(command, sizeof(command), "mkdir debug_memory\\users\\%d >NUL 2>NUL", cur_uid);
#else
        snprintf(command, sizeof(command), "mkdir -p debug_memory/users/%d >/dev/null 2>&1", cur_uid);
#endif
    } else {
#ifdef _WIN32
        snprintf(command, sizeof(command), "mkdir debug_memory >NUL 2>NUL");
#else
        snprintf(command, sizeof(command), "mkdir -p debug_memory >/dev/null 2>&1");
#endif
    }
    system(command);
}

/**
 * 获取KFS统计JSON文件路径
 * 根据当前用户返回对应的文件路径
 * 
 * @param path 输出路径缓冲区
 * @param size 缓冲区大小
 */
static void get_kfs_stats_path(char *path, size_t size) {
    if (cur_uid != -1) {
        snprintf(path, size, "debug_memory/users/%d/kfs_stats.json", cur_uid);
    } else {
        snprintf(path, size, "debug_memory/kfs_stats.json");
    }
}

/**
 * 从JSON行中解析整数值
 * 格式: "key": value
 * 
 * @param line JSON行文本
 * @return 解析出的整数值
 */
static int json_line_int(const char *line) {
    const char *colon = strchr(line, ':');
    if (!colon) return 0;
    return atoi(colon + 1);
}

/**
 * 从JSON行中解析浮点数值
 * 格式: "key": value
 * 
 * @param line JSON行文本
 * @return 解析出的浮点数值
 */
static float json_line_float(const char *line) {
    const char *colon = strchr(line, ':');
    if (!colon) return 0.0f;
    return (float)atof(colon + 1);
}

/**
 * 从JSON行中解析字符串值
 * 格式: "key": "value"
 * 
 * @param line JSON行文本
 * @param out 输出字符串缓冲区
 * @param out_size 缓冲区大小
 */
static void json_line_string(const char *line, char *out, size_t out_size) {
    const char *colon = strchr(line, ':');
    const char *start;
    const char *end;
    size_t len;

    if (!colon || out_size == 0) return;
    start = strchr(colon, '"');
    if (!start) return;
    start++;
    end = strchr(start, '"');
    if (!end) return;
    len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

/**
 * 获取路径的文件名部分
 * 
 * @param path 文件路径
 * @return 文件名部分（最后一个/之后的内容）
 */
static const char *kfs_path_basename(const char *path) {
    const char *slash;

    if (path == NULL) return "";
    slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

/**
 * 从JSON行中解析布尔值
 * 
 * @param line JSON行文本
 * @return 1表示true，0表示false
 */
static int json_bool_value(const char *line) {
    const char *colon = strchr(line, ':');
    return colon != NULL && strstr(colon, "true") != NULL;
}

/* ========================================
   三、KFS统计数据加载模块
   ======================================== */

/**
 * 从JSON文件加载KFS统计数据
 * 解析热点文件信息并加载到缓存中
 * 
 * @param path JSON文件路径
 */
static void load_kfs_stats_from_file(const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    struct hot_file_entry pending;  /* 临时存储正在解析的条目 */
    int in_file = 0;  /* 标记是否正在解析一个文件对象 */
    int loaded = 0;  /* 已加载的文件计数 */

    if (!f) return;

    /* 清空缓存 */
    memset(user_hot_stats, 0, sizeof(user_hot_stats));
    memset(&pending, 0, sizeof(pending));

    /* 逐行解析JSON */
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "    {")) {
            /* 开始解析新的文件对象 */
            memset(&pending, 0, sizeof(pending));
            in_file = 1;
            continue;
        }

        if (!in_file) continue;

        /* 解析各个字段 */
        if (strstr(line, "\"filename\"")) {
            json_line_string(line, pending.filename, sizeof(pending.filename));
        } else if (strstr(line, "\"ino\"")) {
            pending.ino = (unsigned short)json_line_int(line);
        } else if (strstr(line, "\"access_count\"")) {
            pending.access_count = json_line_int(line);
        } else if (strstr(line, "\"short_term_score\"")) {
            pending.short_term_score = json_line_float(line);
        } else if (strstr(line, "\"long_term_score\"")) {
            pending.long_term_score = json_line_float(line);
        } else if (strstr(line, "\"stored_in_kfs\"")) {
            pending.stored_in_kfs = strstr(line, "true") != NULL;
        } else if (strstr(line, "\"first_access\"")) {
            pending.first_access = (unsigned long)json_line_int(line);
        } else if (strstr(line, "\"last_access\"")) {
            pending.last_access = (unsigned long)json_line_int(line);
        } else if (strstr(line, "\"kfs_data_start_blk\"")) {
            pending.kfs_data_start_blk = json_line_int(line);
        } else if (strstr(line, "\"kfs_data_blk_count\"")) {
            pending.kfs_data_blk_count = json_line_int(line);
        } else if (strstr(line, "}")) {
            /* 结束解析，保存条目 */
            if (pending.filename[0] != '\0' && pending.ino != 0 && loaded < HOT_FILE_CACHE_SIZE) {
                if (pending.first_access == 0) pending.first_access = (unsigned long)time(NULL);
                if (pending.last_access == 0) pending.last_access = pending.first_access;
                user_hot_stats[loaded++] = pending;
            }
            in_file = 0;
        }
    }

    fclose(f);
}

/**
 * 从磁盘加载KFS统计数据
 * 优先加载用户目录下的文件，如果失败则加载全局文件
 */
static void load_kfs_stats_from_disk(void) {
    char path[256];

    memset(user_hot_stats, 0, sizeof(user_hot_stats));
    get_kfs_stats_path(path, sizeof(path));
    load_kfs_stats_from_file(path);
    user_hot_stats_uid = cur_uid;
    user_hot_stats_loaded = 1;
}

static void ensure_user_hot_stats_loaded(void) {
    if (!user_hot_stats_loaded || user_hot_stats_uid != cur_uid) {
        load_kfs_stats_from_disk();
    }
}

static void sync_user_stats_kfs_state(void) {
    ensure_user_hot_stats_loaded();

    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        int system_idx = -1;

        if (user_hot_stats[i].access_count == 0) continue;
        for (int j = 0; j < HOT_FILE_CACHE_SIZE; j++) {
            if (hot_file_cache[j].ino == user_hot_stats[i].ino &&
                hot_file_cache[j].stored_in_kfs) {
                system_idx = j;
                break;
            }
        }

        user_hot_stats[i].stored_in_kfs = system_idx >= 0;
        user_hot_stats[i].kfs_data_start_blk =
            system_idx >= 0 ? hot_file_cache[system_idx].kfs_data_start_blk : 0;
        user_hot_stats[i].kfs_data_blk_count =
            system_idx >= 0 ? hot_file_cache[system_idx].kfs_data_blk_count : 0;
    }
}

static void record_user_hot_stat(const char *filename, unsigned short ino) {
    int replace_idx = -1;
    unsigned long now = (unsigned long)time(NULL);

    if (filename == NULL || filename[0] == '\0' || ino == 0 || cur_uid == -1) {
        return;
    }

    ensure_user_hot_stats_loaded();
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (user_hot_stats[i].ino == ino) {
            user_hot_stats[i].access_count++;
            user_hot_stats[i].last_access = now;
            strncpy(user_hot_stats[i].filename, filename, DIRSIZ - 1);
            user_hot_stats[i].filename[DIRSIZ - 1] = '\0';
            return;
        }
        if (replace_idx < 0 && user_hot_stats[i].access_count == 0) {
            replace_idx = i;
        }
    }

    if (replace_idx < 0) {
        for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
            if (replace_idx < 0 ||
                user_hot_stats[i].last_access < user_hot_stats[replace_idx].last_access) {
                replace_idx = i;
            }
        }
    }

    memset(&user_hot_stats[replace_idx], 0, sizeof(user_hot_stats[replace_idx]));
    strncpy(user_hot_stats[replace_idx].filename, filename, DIRSIZ - 1);
    user_hot_stats[replace_idx].filename[DIRSIZ - 1] = '\0';
    user_hot_stats[replace_idx].ino = ino;
    user_hot_stats[replace_idx].access_count = 1;
    user_hot_stats[replace_idx].first_access = now;
    user_hot_stats[replace_idx].last_access = now;
}

/* ========================================
   四、KFS持久化模块
   ======================================== */

/**
 * 保存KFS到磁盘
 * 存储三部分数据：
 * 1. KFS头部信息
 * 2. 热点文件索引
 * 3. 内存文件映射表
 */
void kfs_save_to_disk() {
    unsigned char buf[BLOCKSIZ];
    int i;

    printf("Saving KFS to disk...\n");

    /* 1. 保存KFS头部 */
    memset(buf, 0, BLOCKSIZ);
    struct kfs_disk_header *hdr = (struct kfs_disk_header*)buf;
    strncpy(hdr->magic, "KFS_V3", 7);  /* 魔数，用于验证 */
    hdr->version = 3;
    hdr->tag_count = 0;  /* 不再使用分类 */
    hdr->hot_file_count = HOT_FILE_CACHE_SIZE;
    hdr->last_update = (unsigned long)time(NULL);
    bwrite(KFS_TAGS_BLK, buf);

    /* 2. 保存热点文件索引 */
    memset(buf, 0, BLOCKSIZ);
    struct hot_file_entry *hot_entries = (struct hot_file_entry*)buf;
    for (i = 0; i < HOT_FILE_CACHE_SIZE && (i + 1) * sizeof(struct hot_file_entry) <= BLOCKSIZ; i++) {
        hot_entries[i] = hot_file_cache[i];
    }
    bwrite(KFS_HOT_CACHE_BLK, buf);

    /* 3. 保存内存内容文件映射 */
    memset(buf, 0, BLOCKSIZ);
    int *count_ptr = (int*)buf;
    *count_ptr = memory_file_count;
    char *name_ptr = (char*)(buf + sizeof(int));
    for (i = 0; i < memory_file_count; i++) {
        strncpy(name_ptr + i * DIRSIZ, memory_file_map[i], DIRSIZ - 1);
    }
    bwrite(KFS_MEMORY_MAP_BLK, buf);

    printf("KFS saved to disk successfully.\n");
}

/**
 * 从磁盘加载KFS
 * 验证魔数后加载三部分数据
 */
void kfs_load_from_disk() {
    unsigned char buf[BLOCKSIZ];
    int i;

    printf("Loading KFS from disk...\n");
    kfs_clear_ram_block_cache();  /* 清空RAM缓存 */

    /* 1. 加载KFS头部并验证 */
    bread(KFS_TAGS_BLK, buf);
    struct kfs_disk_header *hdr = (struct kfs_disk_header*)buf;

    if (strncmp(hdr->magic, "KFS_V1", 6) != 0 &&
        strncmp(hdr->magic, "KFS_V2", 6) != 0 &&
        strncmp(hdr->magic, "KFS_V3", 6) != 0) {
        /* 没有有效的KFS数据，初始化新的 */
        printf("No valid KFS found on disk. Initializing new KFS.\n");
        memset(hot_file_cache, 0, sizeof(hot_file_cache));
        memset(memory_file_map, 0, sizeof(memory_file_map));
        memory_file_count = 0;
        return;
    }

    if (hdr->version < 3) {
        printf("Legacy KFS index detected. Rebuilding it from AI selections.\n");
        memset(hot_file_cache, 0, sizeof(hot_file_cache));
        memset(memory_file_map, 0, sizeof(memory_file_map));
        memory_file_count = 0;
        return;
    }

    /* 2. 加载热点文件索引 */
    bread(KFS_HOT_CACHE_BLK, buf);
    memset(hot_file_cache, 0, sizeof(hot_file_cache));
    struct hot_file_entry *hot_entries = (struct hot_file_entry*)buf;
    for (i = 0; i < HOT_FILE_CACHE_SIZE &&
                (i + 1) * sizeof(struct hot_file_entry) <= BLOCKSIZ; i++) {
        hot_file_cache[i] = hot_entries[i];
    }

    /* 3. 加载内存文件映射表 */
    bread(KFS_MEMORY_MAP_BLK, buf);
    int *count_ptr = (int*)buf;
    memory_file_count = *count_ptr;
    char *name_ptr = (char*)(buf + sizeof(int));
    for (i = 0; i < memory_file_count; i++) {
        strncpy(memory_file_map[i], name_ptr + i * DIRSIZ, DIRSIZ - 1);
        memory_file_map[i][DIRSIZ - 1] = '\0';
    }

    printf("KFS loaded from disk successfully.\n");
}

/* ========================================
   五、热点文件存储和读取模块
   ======================================== */

/**
 * 将文件存储到KFS
 * 从原文件系统复制数据块到KFS专用区域
 * 
 * @param filename 文件名
 * @param ino 文件inode号
 * @return 成功返回1，失败返回0
 */
int kfs_store_hot_file(char *filename, unsigned short ino) {
    struct inode *ip = iget(ino);
    if (ip == NULL) {
        printf("File not found: %s\n", filename);
        return 0;
    }
    
    unsigned long file_size = ip->i_din.di_size;
    int num_blocks = (file_size + BLOCKSIZ - 1) / BLOCKSIZ;  /* 计算需要的块数 */
    
    /* 找到空的或已有的热点条目 */
    int hot_idx = -1;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == ino || hot_file_cache[i].ino == 0) {
            hot_idx = i;
            break;
        }
    }
    
    if (hot_idx == -1) {
        iput(ip);
        printf("KFS hot cache full\n");
        return 0;
    }
    
    if (KFS_HOT_FILE_BLOCKS <= 0) {
        iput(ip);
        printf("KFS data area is too small\n");
        return 0;
    }

    /* 分配KFS数据块 - 每个热点文件有固定的槽位 */
    int start_blk = KFS_HOT_DATA_BLK + hot_idx * KFS_HOT_FILE_BLOCKS;
    if (num_blocks > KFS_HOT_FILE_BLOCKS || start_blk + num_blocks > KFS_START + KFS_TOTAL_BLKS) {
        iput(ip);
        printf("KFS data area full or file too large for KFS slot\n");
        return 0;
    }

    /* 使该文件的RAM缓存失效 */
    kfs_invalidate_ram_blocks(ino);
    
    /* 从原文件系统复制数据块到KFS数据区 */
    unsigned char block_buf[BLOCKSIZ];
    for (int i = 0; i < num_blocks; i++) {
        int bn = bmap(ip, i);  /* 获取原文件的物理块号 */
        if (bn == 0) break;
        bread(bn, block_buf);  /* 读取原块 */
        bwrite(start_blk + i, block_buf);  /* 写入KFS */
        kfs_put_ram_block(ino, i, block_buf);  /* 读取到的数据块同步进入RAM */
    }
    
    /* 更新热点条目信息 */
    strncpy(hot_file_cache[hot_idx].filename, filename, DIRSIZ - 1);
    hot_file_cache[hot_idx].ino = ino;
    hot_file_cache[hot_idx].access_count++;
    if (hot_file_cache[hot_idx].first_access == 0) {
        hot_file_cache[hot_idx].first_access = (unsigned long)time(NULL);
    }
    hot_file_cache[hot_idx].last_access = (unsigned long)time(NULL);
    hot_file_cache[hot_idx].stored_in_kfs = 1;  /* 标记已存入KFS */
    hot_file_cache[hot_idx].kfs_data_start_blk = start_blk;  /* KFS起始块 */
    hot_file_cache[hot_idx].kfs_data_blk_count = num_blocks;  /* 块数 */
    hot_file_cache[hot_idx].selected_user_mask |= kfs_user_bit(cur_uid);
    
    /* 更新内存内容文件映射 */
    int mem_idx = -1;
    for (int i = 0; i < memory_file_count; i++) {
        if (strcmp(memory_file_map[i], filename) == 0) {
            mem_idx = i;
            break;
        }
    }
    if (mem_idx == -1 && memory_file_count < HOT_FILE_CACHE_SIZE) {
        strncpy(memory_file_map[memory_file_count], filename, DIRSIZ - 1);
        memory_file_count++;
    }
    
    iput(ip);
    printf("File %s stored in KFS, %d blocks\n", filename, num_blocks);
    kfs_save_to_disk();  /* 持久化到磁盘 */
    export_kfs_stats_to_ai();  /* 导出统计给AI */
    return 1;
}

/**
 * 从KFS读取热点文件的第0块（简化接口）
 * 
 * @param filename 文件名
 * @param buf 输出缓冲区
 * @return 成功返回1，失败返回0
 */
int kfs_read_hot_file(char *filename, unsigned char *buf) {
    return kfs_read_hot_file_block(filename, 0, 0, buf);
}

/**
 * 从KFS读取热点文件的指定块
 * 优先从RAM缓存读取，未命中则从KFS读取并更新缓存
 * 
 * @param filename 文件名
 * @param ino 文件inode号（可选，为0则用文件名匹配）
 * @param block_index 块索引
 * @param buf 输出缓冲区
 * @return 成功返回1，失败返回0
 */
int kfs_read_hot_file_block(
    char *filename,
    unsigned short ino,
    int block_index,
    unsigned char *buf
) {
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        hot_cache_initialized = 1;
    }

    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        /* 匹配文件：可以通过ino或文件名 */
        int identity_matches = ino != 0
            ? hot_file_cache[i].ino == ino
            : strcmp(hot_file_cache[i].filename, filename) == 0;
            
        if (hot_file_cache[i].stored_in_kfs && identity_matches) {
            /* 检查块索引范围 */
            if (block_index < 0 || block_index >= hot_file_cache[i].kfs_data_blk_count) {
                return 0;
            }
            
            if (buf) {
                /* 先尝试从RAM缓存读取 */
                if (kfs_get_ram_block(hot_file_cache[i].ino, block_index, buf)) {
                    printf("Reading %s block %d from KFS (fast path) [RAM hit]\n",
                           filename, block_index);
                    return 1;
                }
                /* RAM未命中，从KFS读取并更新缓存 */
                bread(hot_file_cache[i].kfs_data_start_blk + block_index, buf);
                kfs_put_ram_block(hot_file_cache[i].ino, block_index, buf);
            }
            
            printf("✓ Reading %s block %d from KFS (fast path)\n", filename, block_index);
            return 1;
        }
    }
    return 0;
}

/**
 * 检查文件是否已存储在KFS中
 * 
 * @param filename 文件名
 * @return 已存储返回1，否则返回0
 */
int kfs_is_file_hot(char *filename) {
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].stored_in_kfs && 
            strcmp(hot_file_cache[i].filename, filename) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * 从KFS中移除文件
 * 
 * @param filename 文件名
 * @param ino 文件inode号
 */
void kfs_remove_file(char *filename, unsigned short ino) {
    int changed = 0;
    int stats_changed = 0;

    /* 确保已初始化 */
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        load_kfs_stats_from_disk();
        hot_cache_initialized = 1;
    }

    /* 使RAM缓存失效 */
    kfs_invalidate_ram_blocks(ino);

    /* 清除缓存条目 */
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count > 0 &&
            (strcmp(hot_file_cache[i].filename, filename) == 0 || hot_file_cache[i].ino == ino)) {
            memset(&hot_file_cache[i], 0, sizeof(hot_file_cache[i]));
            changed = 1;
        }
    }

    ensure_user_hot_stats_loaded();
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (user_hot_stats[i].ino == ino ||
            (filename != NULL &&
             strcmp(user_hot_stats[i].filename, filename) == 0)) {
            memset(&user_hot_stats[i], 0, sizeof(user_hot_stats[i]));
            stats_changed = 1;
        }
    }

    if (changed) {
        kfs_update_memory_map();  /* 更新内存映射 */
    } else if (stats_changed) {
        export_kfs_stats_to_ai();
    }
}

/**
 * 使文件在KFS中的存储标记失效
 * 保留元数据，但标记为不在KFS中
 * 
 * @param filename 文件名
 * @param ino 文件inode号
 */
void kfs_invalidate_file(char *filename, unsigned short ino) {
    int changed = 0;

    kfs_invalidate_ram_blocks(ino);  /* 使RAM缓存失效 */
    
    /* 清除KFS存储标记 */
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == ino ||
            (filename != NULL && strcmp(hot_file_cache[i].filename, filename) == 0)) {
            if (hot_file_cache[i].stored_in_kfs) {
                hot_file_cache[i].stored_in_kfs = 0;
                hot_file_cache[i].kfs_data_start_blk = 0;
                hot_file_cache[i].kfs_data_blk_count = 0;
                changed = 1;
            }
        }
    }

    if (changed) {
        kfs_update_memory_map();
    }
}

/**
 * 更新KFS中文件的名称
 * 
 * @param ino 文件inode号
 * @param new_name 新文件名
 */
void kfs_rename_file(unsigned short ino, char *new_name) {
    int changed = 0;
    int stats_changed = 0;

    if (new_name == NULL || new_name[0] == '\0') return;

    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        load_kfs_stats_from_disk();
        hot_cache_initialized = 1;
    }

    /* 更新文件名 */
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == ino &&
            strcmp(hot_file_cache[i].filename, new_name) != 0) {
            memset(hot_file_cache[i].filename, 0, sizeof(hot_file_cache[i].filename));
            strncpy(hot_file_cache[i].filename, new_name, DIRSIZ - 1);
            changed = 1;
        }
    }

    ensure_user_hot_stats_loaded();
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (user_hot_stats[i].ino == ino) {
            memset(user_hot_stats[i].filename, 0,
                   sizeof(user_hot_stats[i].filename));
            strncpy(user_hot_stats[i].filename, new_name, DIRSIZ - 1);
            user_hot_stats[i].access_count++;
            user_hot_stats[i].last_access = (unsigned long)time(NULL);
            stats_changed = 1;
        }
    }

    if (changed) {
        kfs_update_memory_map();
    } else if (stats_changed) {
        export_kfs_stats_to_ai();
    }
}

/**
 * 重置KFS运行时缓存
 * 清空所有数据，恢复到初始状态
 */
void kfs_reset_runtime_cache(void) {
    memset(hot_file_cache, 0, sizeof(hot_file_cache));
    memset(memory_file_map, 0, sizeof(memory_file_map));
    memory_file_count = 0;
    hot_cache_initialized = 0;
    memset(user_hot_stats, 0, sizeof(user_hot_stats));
    user_hot_stats_uid = -2;
    user_hot_stats_loaded = 0;
    memset(ai_selected_inodes, 0, sizeof(ai_selected_inodes));
    ai_selected_inode_count = 0;
    kfs_clear_ram_block_cache();
}

/**
 * 更新内存内容文件映射
 * 根据当前热点缓存重新构建映射表
 */
void kfs_update_memory_map() {
    memory_file_count = 0;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].stored_in_kfs) {
            strncpy(memory_file_map[memory_file_count], hot_file_cache[i].filename, DIRSIZ - 1);
            memory_file_count++;
        }
    }
    kfs_save_to_disk();
    export_kfs_stats_to_ai();
}

/**
 * 显示内存内容文件映射表
 */
void kfs_show_memory_map() {
    printf("=== KFS Memory Content Map ===\n");
    if (memory_file_count == 0) {
        printf("  (empty)\n");
    } else {
        for (int i = 0; i < memory_file_count; i++) {
            printf("[%2d] %s (in-KFS)\n", i + 1, memory_file_map[i]);
        }
    }
    printf("Total: %d files\n", memory_file_count);
}

/* ========================================
   六、KFS初始化模块
   ======================================== */

/**
 * 初始化KFS智能文件系统
 * 1. 从磁盘加载已有数据
 * 2. 从AI学习参数加载热点文件
 * 3. 导出统计给AI
 */
void init_kfs() {
    if (hot_cache_initialized) {
        printf("KFS already initialized.\n");
        return;
    }
    
    /* 先尝试从磁盘加载 */
    kfs_load_from_disk();
    load_kfs_stats_from_disk();
    
    /* 从AI学习参数加载热点文件 */
    kfs_load_hot_files_from_ai();
    
    printf("=== KFS 智能文件系统初始化完成 ===\n");
    printf("KFS disk area: blocks %d-%d\n", KFS_START, KFS_START + KFS_TOTAL_BLKS - 1);
    hot_cache_initialized = 1;
    export_kfs_stats_to_ai();
}

/* ========================================
   七、热点缓存管理模块
   ======================================== */

/**
 * 在热点缓存中查找文件
 * 命中则更新访问记录
 * 
 * @param filename 文件名
 * @return 找到返回文件inode，未找到返回0
 */
unsigned short kfs_hot_cache_lookup(char *filename) {
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        load_kfs_stats_from_disk();
        hot_cache_initialized = 1;
    }
    
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count > 0 &&
            strcmp(hot_file_cache[i].filename, filename) == 0) {
            /* 命中！更新访问记录 */
            hot_file_cache[i].access_count++;
            hot_file_cache[i].last_access = (unsigned long)time(NULL);
            printf("✓ KFS hot cache hit: %s (ino:%d)\n", filename, hot_file_cache[i].ino);
            record_user_hot_stat(filename, hot_file_cache[i].ino);
            export_kfs_stats_to_ai();
            
            /* 每10次访问尝试从AI加载更新 */
            if (hot_file_cache[i].access_count % 10 == 0) {
                kfs_load_hot_files_from_ai();
            }
            
            return hot_file_cache[i].ino;
        }
    }
    
    return 0;
}

/**
 * 更新热点缓存
 * 如果文件已存在则增加访问计数，否则添加新条目
 * 
 * @param filename 文件名
 * @param ino 文件inode号
 */
void kfs_hot_cache_update(char *filename, unsigned short ino) {
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        load_kfs_stats_from_disk();
        hot_cache_initialized = 1;
    }
    
    record_user_hot_stat(filename, ino);
    export_kfs_stats_to_ai();
}

/**
 * 显示热点缓存内容
 */
void kfs_hot_cache_show() {
    if (!hot_cache_initialized) {
        printf("KFS hot cache not initialized.\n");
        return;
    }
    
    printf("=== KFS Hot File Cache ===\n");
    int has_entries = 0;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count > 0) {
            kfs_calculate_scores(&hot_file_cache[i]);
            printf("[%2d] %s (ino:%d, access:%d, short:%.1f, long:%.1f, %s)\n",
                   i + 1, hot_file_cache[i].filename,
                   hot_file_cache[i].ino, hot_file_cache[i].access_count,
                   hot_file_cache[i].short_term_score, hot_file_cache[i].long_term_score,
                   hot_file_cache[i].stored_in_kfs ? "IN-KFS" : "NO-KFS");
            has_entries = 1;
        }
    }
    
    if (!has_entries) {
        printf("  (empty)\n");
    }
}

/**
 * 计算文件的长短期热度评分
 * 
 * @param entry 热点文件条目
 */
void kfs_calculate_scores(struct hot_file_entry *entry) {
    time_t now = time(NULL);
    unsigned long time_since_first = (unsigned long)now - entry->first_access;
    unsigned long time_since_last = (unsigned long)now - entry->last_access;
    
    /* 短期评分 - 最近访问频率 (0-100) */
    float recency_factor = 1.0f;
    if (time_since_last > 3600) {  /* 1小时前访问则衰减 */
        recency_factor = 1.0f / (1.0f + time_since_last / 3600.0f);
    }
    entry->short_term_score = entry->access_count * recency_factor * 10;
    
    /* 长期评分 - 长期活跃度 (0-100) */
    float longevity_factor = 1.0f;
    if (time_since_first > 86400) {  /* 超过1天则考虑活跃度衰减 */
        longevity_factor = (float)entry->access_count / (time_since_first / 86400.0f + 1);
    }
    entry->long_term_score = (float)entry->access_count * 5 + longevity_factor * 2;
    
    /* 归一化到 0-100 */
    if (entry->short_term_score > 100) entry->short_term_score = 100;
    if (entry->long_term_score > 100) entry->long_term_score = 100;
}

/* ========================================
   八、AI集成模块
   ======================================== */

/**
 * 从learned_params.json加载AI选择的热点文件
 * 解析JSON格式并将选中的文件存储到KFS中
 */
void kfs_load_hot_files_from_ai() {
    char params_path[256];
    snprintf(params_path, sizeof(params_path),
             "debug_memory/users/%d/agent/memory/long_term/learned_params.json",
             cur_uid);
    FILE* f = fopen(params_path, "r");
    if (!f) {
        return; /* 文件不存在或无法打开 */
    }
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        hot_cache_initialized = 1;
    }

    /*
     * The learned list is authoritative for this user. Remove stale
     * ownership first, then add the inodes present in the latest list.
     */
    unsigned int current_user_bit = kfs_user_bit(cur_uid);
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        hot_file_cache[i].selected_user_mask &= ~current_user_bit;
    }
    
    memset(ai_selected_inodes, 0, sizeof(ai_selected_inodes));
    ai_selected_inode_count = 0;
    printf("Loading AI-selected hot files from learned_params.json...\n");
    
    char line[512];
    int in_hot_files = 0;
    int in_object = 0;
    int stored_count = 0;
    char filename[DIRSIZ] = {0};
    char path[512] = {0};
    unsigned short ino = 0;
    
    /* 先找到 hot_files 开始的位置 */
    while (fgets(line, sizeof(line), f)) {
        if (!in_hot_files && strstr(line, "hot_files")) {
            in_hot_files = 1;
            continue;
        }
        
        if (in_hot_files) {
            /* 检查是否到达结束 */
            if (strstr(line, "]")) {
                break;
            }

            if (strstr(line, "{")) {
                memset(filename, 0, sizeof(filename));
                memset(path, 0, sizeof(path));
                ino = 0;
                in_object = 1;
            }

            if (!in_object) {
                continue;
            }

            /* 解析JSON字段 */
            if (strstr(line, "\"filename\"")) {
                json_line_string(line, filename, sizeof(filename));
            } else if (strstr(line, "\"path\"")) {
                json_line_string(line, path, sizeof(path));
            } else if (strstr(line, "\"ino\"")) {
                ino = (unsigned short)json_line_int(line);
            }

            if (strstr(line, "}")) {
                /* 如果有路径则解析出inode */
                if (path[0] != '\0') {
                    struct inode *resolved = namei(path);
                    if (resolved == NULL || is_directory(resolved)) {
                        if (resolved != NULL) iput(resolved);
                        printf("KFS AI path not found or not a file: %s\n", path);
                        in_object = 0;
                        continue;
                    }
                    ino = resolved->i_ino;
                    strncpy(filename, kfs_path_basename(path), DIRSIZ - 1);
                    filename[DIRSIZ - 1] = '\0';
                    iput(resolved);
                }

                /* 如果有 filename 和 ino，尝试存储到 KFS */
                if (filename[0] != '\0' && ino != 0) {
                    kfs_remember_ai_selection(ino);
                    /* 先检查是否已经存储了 */
                    int already_stored = 0;
                    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
                        if (hot_file_cache[i].ino == ino && hot_file_cache[i].stored_in_kfs) {
                            hot_file_cache[i].selected_user_mask |=
                                current_user_bit;
                            already_stored = 1;
                            break;
                        }
                    }
                    
                    if (!already_stored) {
                        if (kfs_store_hot_file(filename, ino)) {
                            stored_count++;
                            printf("✓ Stored %s (ino:%d) in KFS based on AI selection\n", filename, ino);
                        }
                    }
                }
                in_object = 0;
            }
        }
    }
    
    fclose(f);
    
    if (stored_count > 0) {
        printf("AI hot file update complete. %d new files stored in KFS.\n", stored_count);
        kfs_save_to_disk();
    } else {
        printf("No new hot files to store from AI selection.\n");
        kfs_save_to_disk();
    }
}

/**
 * 导出KFS热点文件数据到JSON
 * 供AI Agent学习使用
 */
void export_kfs_stats_to_ai() {
    char path[256];

    ensure_user_hot_stats_loaded();
    sync_user_stats_kfs_state();
    ensure_ai_stats_dirs();  /* 确保统计目录存在 */
    get_kfs_stats_path(path, sizeof(path));
    FILE* f = fopen(path, "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"hot_files\": [\n");
    
    int first = 1;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (user_hot_stats[i].access_count == 0) continue;
        
        kfs_calculate_scores(&user_hot_stats[i]);
        
        if (!first) fprintf(f, ",\n");
        first = 0;
        
        /* 导出所有相关信息 */
        fprintf(f, "    {\n");
        fprintf(f, "      \"filename\": \"%s\",\n", user_hot_stats[i].filename);
        fprintf(f, "      \"ino\": %d,\n", user_hot_stats[i].ino);
        fprintf(f, "      \"access_count\": %d,\n", user_hot_stats[i].access_count);
        fprintf(f, "      \"short_term_score\": %.1f,\n", user_hot_stats[i].short_term_score);
        fprintf(f, "      \"long_term_score\": %.1f,\n", user_hot_stats[i].long_term_score);
        fprintf(f, "      \"stored_in_kfs\": %s,\n", user_hot_stats[i].stored_in_kfs ? "true" : "false");
        fprintf(f, "      \"first_access\": %lu,\n", user_hot_stats[i].first_access);
        fprintf(f, "      \"last_access\": %lu,\n", user_hot_stats[i].last_access);
        fprintf(f, "      \"kfs_data_start_blk\": %d,\n", user_hot_stats[i].kfs_data_start_blk);
        fprintf(f, "      \"kfs_data_blk_count\": %d\n", user_hot_stats[i].kfs_data_blk_count);
        fprintf(f, "    }");
    }
    
    fprintf(f, "\n  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/**
 * 列出KFS中的虚拟目录
 * 
 * @param vdir 虚拟目录名称
 */
void kfs_list_virtual_dir(char *vdir) {
    printf("Listing virtual directory: %s\n", vdir);
    printf("Hot files in KFS:\n");
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count > 0) {
            printf("  %s (ino: %d, access: %d)\n", 
                   hot_file_cache[i].filename, 
                   hot_file_cache[i].ino,
                   hot_file_cache[i].access_count);
        }
    }
}

/**
 * 打印KFS目录条目
 */
static const char *kfs_username_for_uid(int uid) {
    if (uid == 0) return "root";
    for (int i = 0; i < USERNUM; i++) {
        if (user[i].u_uid == uid) return user[i].u_name;
    }
    return NULL;
}

static const char *kfs_display_basename(const char *name) {
    const char *slash;

    if (name == NULL) return "";
    slash = strrchr(name, '/');
    return slash != NULL ? slash + 1 : name;
}

static int kfs_find_user_inode_name(
    int uid,
    unsigned short ino,
    char *name,
    size_t name_size
) {
    const char *username = kfs_username_for_uid(uid);
    char home_path[64];
    struct inode *home;
    struct direct entry;
    unsigned long entry_count;

    if (name != NULL && name_size > 0) name[0] = '\0';
    if (username == NULL || uid == 0) return 0;
    snprintf(home_path, sizeof(home_path), "/usr/%s", username);
    home = namei(home_path);
    if (home == NULL || !is_directory(home)) {
        if (home != NULL) iput(home);
        return 0;
    }

    entry_count = (home->i_din.di_size + sizeof(struct direct) - 1) /
                  sizeof(struct direct);
    for (unsigned long i = 0; i < entry_count; i++) {
        int entries_per_block = BLOCKSIZ / sizeof(struct direct);
        int bn = bmap(home, (int)(i / entries_per_block));
        if (bn == 0) break;
        bread(bn, block_buf);
        memcpy(&entry,
               block_buf + (i % entries_per_block) * sizeof(struct direct),
               sizeof(struct direct));
        if (entry.d_ino == ino) {
            if (name != NULL && name_size > 0 &&
                strcmp(entry.d_name, ".") != 0 &&
                strcmp(entry.d_name, "..") != 0) {
                strncpy(name, entry.d_name, name_size - 1);
                name[name_size - 1] = '\0';
            }
            iput(home);
            return name == NULL || name[0] != '\0';
        }
    }

    iput(home);
    return 0;
}

static int kfs_find_any_inode_name(
    unsigned short ino,
    char *name,
    size_t name_size
) {
    for (int i = 0; i < USERNUM; i++) {
        if (kfs_find_user_inode_name(user[i].u_uid, ino, name, name_size)) {
            return 1;
        }
    }
    return 0;
}

void kfs_print_directory_entries() {
    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        hot_cache_initialized = 1;
    }

    for (int i = 0; i < USERNUM; i++) {
        if (user[i].u_name[0] == '\0') continue;
        printf("d %s (uid: %d) [KFS-USER]\n",
               user[i].u_name, user[i].u_uid);
    }
}

void kfs_print_user_directory_entries(int uid) {
    int repaired = 0;

    if (!hot_cache_initialized) {
        kfs_load_from_disk();
        hot_cache_initialized = 1;
    }

    printf("Directory contents:\n");
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        struct inode *ip;
        const char *display_name;
        char linked_name[DIRSIZ] = {0};
        char repaired_name[DIRSIZ] = {0};

        if (!hot_file_cache[i].stored_in_kfs ||
            hot_file_cache[i].filename[0] == '\0') {
            continue;
        }

        ip = iget(hot_file_cache[i].ino);
        if (ip == NULL || ip->i_din.di_mode == 0 || is_directory(ip)) {
            if (ip != NULL) iput(ip);
            continue;
        }
        if ((hot_file_cache[i].selected_user_mask & kfs_user_bit(uid)) == 0) {
            iput(ip);
            continue;
        }

        if (kfs_find_user_inode_name(uid, ip->i_ino,
                                     linked_name, sizeof(linked_name)) ||
            (uid == 0 &&
             kfs_find_any_inode_name(ip->i_ino,
                                     linked_name, sizeof(linked_name)))) {
            display_name = linked_name;
        } else {
            display_name = kfs_display_basename(hot_file_cache[i].filename);
        }

        if (strncmp(hot_file_cache[i].filename, "ino:", 4) == 0 &&
            display_name[0] != '\0') {
            strncpy(repaired_name, display_name, DIRSIZ - 1);
            memset(hot_file_cache[i].filename, 0,
                   sizeof(hot_file_cache[i].filename));
            strncpy(hot_file_cache[i].filename, repaired_name, DIRSIZ - 1);
            display_name = hot_file_cache[i].filename;
            repaired = 1;
        }
        printf("- %s (ino: %d, links: %d) [KFS]\n",
               display_name,
               hot_file_cache[i].ino,
               ip->i_din.di_nlink);
        iput(ip);
    }

    if (repaired) {
        kfs_update_memory_map();
    }
}

/**
 * 根据inode判断文件是否是热点文件
 * 
 * @param ino 文件inode号
 * @return 是热点文件返回1，否则返回0
 */
int kfs_is_file_hot_by_ino(int ino) {
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == ino && hot_file_cache[i].access_count > 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * 显示文件标签
 * 
 * @param ino 文件inode号
 */
void kfs_show_tags(int ino) {
    printf("Tags for inode %d:\n", ino);
    printf("  Type: File\n");
    printf("  Hot: %s\n", kfs_is_file_hot_by_ino(ino) ? "Yes" : "No");
}

/**
 * AI选择热点文件
 * 从AI学习参数中加载并更新热点文件
 */
void kfs_ai_select_hot_files() {
    kfs_load_hot_files_from_ai();
    printf("AI selection complete. Hot files loaded from learned_params.json.\n");
}
