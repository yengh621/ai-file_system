#include "filesystem.h"

/* 内存中的 KFS 数据 - 只保留热点文件缓存 */
static struct hot_file_entry hot_file_cache[HOT_FILE_CACHE_SIZE];
static int hot_cache_initialized = 0;

/* 内存内容文件 - 记录当前内存中的文件 */
static char memory_file_map[HOT_FILE_CACHE_SIZE][DIRSIZ];
static int memory_file_count = 0;

/* === 一、KFS 持久化 === */

/* 保存 KFS 到磁盘 - 只保存热点文件 */
void kfs_save_to_disk() {
    unsigned char buf[BLOCKSIZ];
    int i;
    
    printf("Saving KFS to disk...\n");
    
    /* 1. 保存 KFS 头部（简化版） */
    memset(buf, 0, BLOCKSIZ);
    struct kfs_disk_header *hdr = (struct kfs_disk_header*)buf;
    strncpy(hdr->magic, "KFS_V1", 7);
    hdr->version = 1;
    hdr->tag_count = 0; /* 不再使用分类 */
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
    
    /* 3. 保存内存内容文件 */
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

/* 从磁盘加载 KFS */
void kfs_load_from_disk() {
    unsigned char buf[BLOCKSIZ];
    int i;
    
    printf("Loading KFS from disk...\n");
    
    /* 1. 加载 KFS 头部 */
    bread(KFS_TAGS_BLK, buf);
    struct kfs_disk_header *hdr = (struct kfs_disk_header*)buf;
    
    if (strncmp(hdr->magic, "KFS_V1", 6) != 0) {
        printf("No valid KFS found on disk. Initializing new KFS.\n");
    }
    
    /* 2. 加载热点文件索引 */
    bread(KFS_HOT_CACHE_BLK, buf);
    struct hot_file_entry *hot_entries = (struct hot_file_entry*)buf;
    for (i = 0; i < HOT_FILE_CACHE_SIZE && (i + 1) * sizeof(struct hot_file_entry) <= BLOCKSIZ; i++) {
        hot_file_cache[i] = hot_entries[i];
    }
    
    /* 3. 加载内存内容文件 */
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

/* === 二、热点文件存储和读取 === */

/* 将热点文件存储到 KFS 磁盘 */
int kfs_store_hot_file(char *filename, unsigned short ino) {
    struct inode *ip = iget(ino);
    if (ip == NULL) {
        printf("File not found: %s\n", filename);
        return 0;
    }
    
    unsigned long file_size = ip->i_din.di_size;
    int num_blocks = (file_size + BLOCKSIZ - 1) / BLOCKSIZ;
    
    /* 找到空的热点条目 */
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
    
    /* 分配 KFS 数据块 */
    int start_blk = KFS_HOT_DATA_BLK + hot_idx * 10; /* 每个文件预留10块 */
    if (start_blk + num_blocks > KFS_START + KFS_TOTAL_BLKS) {
        iput(ip);
        printf("KFS data area full\n");
        return 0;
    }
    
    /* 复制文件内容到 KFS 数据区 */
    unsigned char block_buf[BLOCKSIZ];
    for (int i = 0; i < num_blocks; i++) {
        int bn = bmap(ip, i);
        if (bn == 0) break;
        bread(bn, block_buf);
        bwrite(start_blk + i, block_buf);
    }
    
    /* 更新热点条目 */
    strncpy(hot_file_cache[hot_idx].filename, filename, DIRSIZ - 1);
    hot_file_cache[hot_idx].ino = ino;
    hot_file_cache[hot_idx].stored_in_kfs = 1;
    hot_file_cache[hot_idx].kfs_data_start_blk = start_blk;
    hot_file_cache[hot_idx].kfs_data_blk_count = num_blocks;
    
    /* 更新内存内容文件 */
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
    return 1;
}

/* 从 KFS 读取热点文件 */
int kfs_read_hot_file(char *filename, unsigned char *buf) {
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].stored_in_kfs && 
            strcmp(hot_file_cache[i].filename, filename) == 0) {
            printf("✓ Reading %s from KFS (fast path)\n", filename);
            return 1; /* 这里可以扩展为实际读取 */
        }
    }
    return 0;
}

/* 检查文件是否是热点文件 */
int kfs_is_file_hot(char *filename) {
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].stored_in_kfs && 
            strcmp(hot_file_cache[i].filename, filename) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 更新内存内容文件 */
void kfs_update_memory_map() {
    memory_file_count = 0;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].stored_in_kfs) {
            strncpy(memory_file_map[memory_file_count], hot_file_cache[i].filename, DIRSIZ - 1);
            memory_file_count++;
        }
    }
    kfs_save_to_disk();
}

/* 显示内存内容文件 */
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

/* === 三、KFS 初始化 === */

void init_kfs() {
    if (hot_cache_initialized) {
        printf("KFS already initialized.\n");
        return;
    }
    
    /* 先尝试从磁盘加载 */
    kfs_load_from_disk();
    
    /* 从 AI 学习参数加载热点文件 */
    kfs_load_hot_files_from_ai();
    
    printf("=== KFS 智能文件系统初始化完成 ===\n");
    printf("KFS disk area: blocks %d-%d\n", KFS_START, KFS_START + KFS_TOTAL_BLKS - 1);
    hot_cache_initialized = 1;
}

/* === 四、热点缓存函数 === */

unsigned short kfs_hot_cache_lookup(char *filename) {
    if (!hot_cache_initialized) {
        memset(hot_file_cache, 0, sizeof(hot_file_cache));
        hot_cache_initialized = 1;
    }
    
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count > 0 &&
            strcmp(hot_file_cache[i].filename, filename) == 0) {
            /* 命中！更新访问记录 */
            hot_file_cache[i].access_count++;
            hot_file_cache[i].last_access = (unsigned long)time(NULL);
            printf("✓ KFS hot cache hit: %s (ino:%d)\n", filename, hot_file_cache[i].ino);
            
            /* 每10次访问尝试从AI加载更新 */
            if (hot_file_cache[i].access_count % 10 == 0) {
                kfs_load_hot_files_from_ai();
            }
            
            return hot_file_cache[i].ino;
        }
    }
    
    return 0;
}

void kfs_hot_cache_update(char *filename, unsigned short ino) {
    if (!hot_cache_initialized) {
        memset(hot_file_cache, 0, sizeof(hot_file_cache));
        hot_cache_initialized = 1;
    }
    
    /* 检查是否已在缓存中 */
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (strcmp(hot_file_cache[i].filename, filename) == 0) {
            hot_file_cache[i].access_count++;
            hot_file_cache[i].last_access = (unsigned long)time(NULL);
            /* 导出更新后的 KFS 统计给 AI */
            export_kfs_stats_to_ai();
            return;
        }
    }
    
    /* 不在缓存中，找位置插入 */
    int replace_idx = -1;
    
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count == 0) {
            replace_idx = i;
            break;
        }
    }
    
    /* 插入新条目 */
    if (replace_idx != -1) {
        strncpy(hot_file_cache[replace_idx].filename, filename, DIRSIZ - 1);
        hot_file_cache[replace_idx].ino = ino;
        hot_file_cache[replace_idx].access_count = 1;
        hot_file_cache[replace_idx].last_access = (unsigned long)time(NULL);
        hot_file_cache[replace_idx].first_access = (unsigned long)time(NULL);
        hot_file_cache[replace_idx].stored_in_kfs = 0;
        hot_file_cache[replace_idx].short_term_score = 0;
        hot_file_cache[replace_idx].long_term_score = 0;
        printf("✓ KFS hot cache updated: %s added\n", filename);
        /* 导出更新后的 KFS 统计给 AI */
        export_kfs_stats_to_ai();
    }
}

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

/* 计算文件的长短期评分 */
void kfs_calculate_scores(struct hot_file_entry *entry) {
    time_t now = time(NULL);
    unsigned long time_since_first = (unsigned long)now - entry->first_access;
    unsigned long time_since_last = (unsigned long)now - entry->last_access;
    
    /* 短期评分 - 最近访问频率 (0-100) */
    float recency_factor = 1.0f;
    if (time_since_last > 3600) {  /* 1小时前 */
        recency_factor = 1.0f / (1.0f + time_since_last / 3600.0f);
    }
    entry->short_term_score = entry->access_count * recency_factor * 10;
    
    /* 长期评分 - 长期活跃度 (0-100) */
    float longevity_factor = 1.0f;
    if (time_since_first > 86400) {  /* 1天前 */
        longevity_factor = (float)entry->access_count / (time_since_first / 86400.0f + 1);
    }
    entry->long_term_score = (float)entry->access_count * 5 + longevity_factor * 2;
    
    /* 归一化到 0-100 */
    if (entry->short_term_score > 100) entry->short_term_score = 100;
    if (entry->long_term_score > 100) entry->long_term_score = 100;
}

/* 从 learned_params.json 加载 AI 选择的热点文件并更新 KFS */
void kfs_load_hot_files_from_ai() {
    char params_path[256];
    snprintf(params_path, sizeof(params_path),
             "debug_memory/users/%d/agent/memory/long_term/learned_params.json",
             cur_uid);
    FILE* f = fopen(params_path, "r");
    if (!f) {
        return; /* 文件不存在或无法打开 */
    }
    
    printf("Loading AI-selected hot files from learned_params.json...\n");
    
    char line[512];
    int in_hot_files = 0;
    int stored_count = 0;
    
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
            
            /* 查找 filename 字段 */
            char* filename_start = strstr(line, "\"filename\":");
            if (filename_start) {
                /* 简单解析 - 提取文件名 */
                char filename[DIRSIZ] = {0};
                unsigned short ino = 0;
                
                /* 提取 filename */
                filename_start = strchr(filename_start, '\"');
                if (filename_start) {
                    filename_start++;
                    char* filename_end = strchr(filename_start, '\"');
                    if (filename_end) {
                        int len = filename_end - filename_start;
                        if (len < DIRSIZ) {
                            strncpy(filename, filename_start, len);
                            filename[len] = '\0';
                        }
                    }
                }
                
                /* 提取 ino */
                char* ino_start = strstr(line, "\"ino\":");
                if (ino_start) {
                    ino = (unsigned short)atoi(ino_start + 6);
                }
                
                /* 如果有 filename 和 ino，尝试存储到 KFS */
                if (filename[0] != '\0' && ino != 0) {
                    /* 先检查是否已经存储了 */
                    int already_stored = 0;
                    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
                        if (hot_file_cache[i].ino == ino && hot_file_cache[i].stored_in_kfs) {
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
            }
        }
    }
    
    fclose(f);
    
    if (stored_count > 0) {
        printf("AI hot file update complete. %d new files stored in KFS.\n", stored_count);
        kfs_save_to_disk();
    } else {
        printf("No new hot files to store from AI selection.\n");
    }
}

/* 导出 KFS 热点文件数据到 JSON，供 AI Agent 使用 */
void export_kfs_stats_to_ai() {
    FILE* f = fopen("debug_memory/kfs_stats.json", "w");
    if (!f) return;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"hot_files\": [\n");
    
    int first = 1;
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].access_count == 0) continue;
        
        kfs_calculate_scores(&hot_file_cache[i]);
        
        if (!first) fprintf(f, ",\n");
        first = 0;
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"filename\": \"%s\",\n", hot_file_cache[i].filename);
        fprintf(f, "      \"ino\": %d,\n", hot_file_cache[i].ino);
        fprintf(f, "      \"access_count\": %d,\n", hot_file_cache[i].access_count);
        fprintf(f, "      \"short_term_score\": %.1f,\n", hot_file_cache[i].short_term_score);
        fprintf(f, "      \"long_term_score\": %.1f,\n", hot_file_cache[i].long_term_score);
        fprintf(f, "      \"stored_in_kfs\": %s\n", hot_file_cache[i].stored_in_kfs ? "true" : "false");
        fprintf(f, "    }");
    }
    
    fprintf(f, "\n  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* 列出虚拟目录（简化实现） */
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

/* 根据 inode 判断是否是热点文件 */
int kfs_is_file_hot_by_ino(int ino) {
    for (int i = 0; i < HOT_FILE_CACHE_SIZE; i++) {
        if (hot_file_cache[i].ino == ino && hot_file_cache[i].access_count > 0) {
            return 1;
        }
    }
    return 0;
}

/* 显示文件标签（简化实现） */
void kfs_show_tags(int ino) {
    printf("Tags for inode %d:\n", ino);
    printf("  Type: File\n");
    printf("  Hot: %s\n", kfs_is_file_hot_by_ino(ino) ? "Yes" : "No");
}

/* AI 选择热点文件 */
void kfs_ai_select_hot_files() {
    kfs_load_hot_files_from_ai();
    printf("AI selection complete. Hot files loaded from learned_params.json.\n");
}
