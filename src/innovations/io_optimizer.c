#include "filesystem.h"

static struct workload_analyzer workload;
static int workload_initialized = 0;

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

static void get_io_stats_path(char *path, size_t size) {
    if (cur_uid != -1) {
        snprintf(path, size, "debug_memory/users/%d/io_stats.json", cur_uid);
    } else {
        snprintf(path, size, "debug_memory/io_stats.json");
    }
}

static const char* workload_type_to_text(WorkloadType type) {
    if (type == WORKLOAD_SEQUENTIAL) return "sequential";
    if (type == WORKLOAD_RANDOM) return "random";
    if (type == WORKLOAD_STREAM) return "stream";
    return "unknown";
}

static WorkloadType workload_type_from_text(const char *line) {
    if (strstr(line, "sequential")) return WORKLOAD_SEQUENTIAL;
    if (strstr(line, "random")) return WORKLOAD_RANDOM;
    if (strstr(line, "stream")) return WORKLOAD_STREAM;
    return WORKLOAD_UNKNOWN;
}

static int json_line_int(const char *line) {
    const char *colon = strchr(line, ':');
    if (!colon) return 0;
    return atoi(colon + 1);
}

static void reset_block_history(struct file_io_history *fh) {
    for (int i = 0; i < WORKLOAD_HISTORY; i++) {
        fh->block_history[i] = -1;
    }
}

static void analyze_file_history(struct file_io_history *fh, int *seq_count, int *rand_count, int *total) {
    *seq_count = 0;
    *rand_count = 0;
    *total = 0;

    for (int i = 1; i < WORKLOAD_HISTORY; i++) {
        int prev_idx = (fh->history_idx - i - 1 + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        int curr_idx = (fh->history_idx - i + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        int prev_blk = fh->block_history[prev_idx];
        int curr_blk = fh->block_history[curr_idx];

        if (prev_blk < 0 || curr_blk < 0) continue;

        int diff = curr_blk - prev_blk;
        if (diff == 1) {
            (*seq_count)++;
        } else if (abs(diff) > 1) {
            (*rand_count)++;
        }
        (*total)++;
    }
}

static void load_io_stats_from_file(const char *path) {
    FILE *f = fopen(path, "r");
    char line[512];
    struct file_io_history pending;
    int in_file = 0;
    int loaded_files = 0;

    if (!f) return;

    memset(&workload, 0, sizeof(workload));
    memset(&pending, 0, sizeof(pending));
    reset_block_history(&pending);
    pending.last_block = -1;
    pending.prefetch_window = 3;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"cache_hits\"")) {
            workload.cache_hits = json_line_int(line);
        } else if (strstr(line, "\"cache_misses\"")) {
            workload.cache_misses = json_line_int(line);
        } else if (strstr(line, "    {") && strstr(line, "\"ino\"") == NULL) {
            memset(&pending, 0, sizeof(pending));
            reset_block_history(&pending);
            pending.last_block = -1;
            pending.prefetch_window = 3;
            pending.current_type = WORKLOAD_UNKNOWN;
            in_file = 1;
        }

        if (in_file) {
            if (strstr(line, "\"ino\"")) {
                pending.ino = (unsigned short)json_line_int(line);
            } else if (strstr(line, "\"total_reads\"")) {
                pending.total_reads = json_line_int(line);
            } else if (strstr(line, "\"type\"")) {
                pending.current_type = workload_type_from_text(line);
            } else if (strstr(line, "\"current_prefetch_window\"")) {
                pending.prefetch_window = json_line_int(line);
                if (pending.prefetch_window < 1) pending.prefetch_window = 1;
                if (pending.prefetch_window > 10) pending.prefetch_window = 10;
            } else if (strstr(line, "\"last_block\"")) {
                pending.last_block = json_line_int(line);
            } else if (strstr(line, "}")) {
                if (pending.ino != 0 && loaded_files < MAX_INODES) {
                    workload.files[loaded_files++] = pending;
                }
                in_file = 0;
            }
        }
    }

    fclose(f);
    workload.file_count = loaded_files;
}

static void load_io_stats_from_disk(void) {
    char path[256];

    get_io_stats_path(path, sizeof(path));
    load_io_stats_from_file(path);
    if (workload.file_count == 0 && workload.cache_hits == 0 && workload.cache_misses == 0) {
        load_io_stats_from_file("debug_memory/io_stats.json");
    }

    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino != 0) {
            workload.files[i].prefetch_window = load_file_prefetch_window_from_ai(workload.files[i].ino);
        }
    }
}

/* 导出全局和每个文件的 IO 统计数据到 JSON 文件，供 AI Agent 使用 */
void export_io_stats_to_ai() {
    char path[256];
    if (!workload_initialized) return;
    
    ensure_ai_stats_dirs();
    get_io_stats_path(path, sizeof(path));
    FILE* f = fopen(path, "w");
    if (!f) return;
    
    /* 统计全局信息 */
    int total_reads = 0;
    int seq_files = 0;
    int rand_files = 0;
    int avg_prefetch_window = 0;
    
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino == 0) continue;
        
        struct file_io_history* fh = &workload.files[i];
        total_reads += fh->total_reads;
        avg_prefetch_window += fh->prefetch_window;
        
        if (fh->current_type == WORKLOAD_SEQUENTIAL) seq_files++;
        else if (fh->current_type == WORKLOAD_RANDOM) rand_files++;
    }
    
    if (workload.file_count > 0) {
        avg_prefetch_window /= workload.file_count;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"cache_hits\": %d,\n", workload.cache_hits);
    fprintf(f, "  \"cache_misses\": %d,\n", workload.cache_misses);
    fprintf(f, "  \"total_prefetched_blocks\": %d,\n", workload.cache_hits + workload.cache_misses);
    fprintf(f, "  \"file_count\": %d,\n", workload.file_count);
    fprintf(f, "  \"sequential_files\": %d,\n", seq_files);
    fprintf(f, "  \"random_files\": %d,\n", rand_files);
    fprintf(f, "  \"average_prefetch_window\": %d,\n", avg_prefetch_window);
    fprintf(f, "  \"total_read_operations\": %d,\n", total_reads);
    fprintf(f, "  \"files\": [\n");
    
    int first_file = 1;
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino == 0) continue;
        
        struct file_io_history* fh = &workload.files[i];
        int seq_count = 0;
        int rand_count = 0;
        int transition_count = 0;
        
        if (!first_file) fprintf(f, ",\n");
        first_file = 0;
        
        const char* type_str = workload_type_to_text(fh->current_type);
        analyze_file_history(fh, &seq_count, &rand_count, &transition_count);
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"ino\": %d,\n", fh->ino);
        fprintf(f, "      \"total_reads\": %d,\n", fh->total_reads);
        fprintf(f, "      \"type\": \"%s\",\n", type_str);
        fprintf(f, "      \"sequential_transitions\": %d,\n", seq_count);
        fprintf(f, "      \"random_transitions\": %d,\n", rand_count);
        fprintf(f, "      \"transition_count\": %d,\n", transition_count);
        fprintf(f, "      \"current_prefetch_window\": %d,\n", fh->prefetch_window);
        fprintf(f, "      \"last_block\": %d\n", fh->last_block);
        fprintf(f, "    }");
    }
    
    fprintf(f, "\n  ]\n");
    fprintf(f, "}\n");
    fclose(f);
}

/* 获取或创建文件的 IO 历史 */
static struct file_io_history* get_file_history(unsigned short ino) {
    if (ino == 0) return NULL;
    
    /* 先找是否已有 */
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino == ino) {
            return &workload.files[i];
        }
    }
    
    /* 创建新的 */
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino == 0) {
            struct file_io_history* fh = &workload.files[i];
            fh->ino = ino;
            fh->history_idx = 0;
            fh->current_type = WORKLOAD_UNKNOWN;
            fh->prefetch_window = 3;
            fh->last_block = -1;
            fh->total_reads = 0;
            reset_block_history(fh);
            workload.file_count++;
            return fh;
        }
    }
    
    return NULL;
}

/* 从 learned_params.json 加载某个文件的预取窗口参数 */
int load_file_prefetch_window_from_ai(unsigned short ino) {
    char params_path[256];
    snprintf(params_path, sizeof(params_path),
             "debug_memory/users/%d/agent/memory/long_term/learned_params.json",
             cur_uid);
    FILE* f = fopen(params_path, "r");
    if (!f) {
        return 3; /* 默认值 */
    }
    
    char line[512];
    int window = 3;
    
    /* 先找到 file_prefetch_windows 开始的位置 */
    int in_windows = 0;
    while (fgets(line, sizeof(line), f)) {
        if (!in_windows && strstr(line, "file_prefetch_windows")) {
            in_windows = 1;
            continue;
        }
        
        if (in_windows) {
            /* 检查是否找到该 inode */
            char ino_str[32];
            sprintf(ino_str, "\"%d\":", ino);
            char* match = strstr(line, ino_str);
            if (match) {
                char* num_start = strchr(match, ':');
                if (num_start) {
                    window = atoi(num_start + 1);
                    if (window < 1) window = 1;
                    if (window > 10) window = 10;
                    fclose(f);
                    return window;
                }
            }
            
            /* 检查是否到达结束 */
            if (strstr(line, "}")) {
                break;
            }
        }
    }
    
    fclose(f);
    return 3; /* 未找到，返回默认值 */
}

/* 设置全局预取窗口 */
void set_prefetch_window(int window) {
    if (!workload_initialized) init_workload_analyzer();
    
    /* 应用到所有已记录的文件 */
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino != 0) {
            workload.files[i].prefetch_window = window;
        }
    }
    
    printf("全局 I/O 预取窗口已设置为: %d\n", window);
}

/* 初始化 */
void init_workload_analyzer() {
    if (workload_initialized) {
        printf("Workload analyzer already initialized.\n");
        return;
    }
    
    memset(&workload, 0, sizeof(workload));
    printf("=== AI 自适应 I/O 优化初始化完成 (Per-File) ===\n");
    printf("现在每个文件有独立的 IO 历史和预取窗口\n");
    workload_initialized = 1;
    load_io_stats_from_disk();
    export_io_stats_to_ai();
}

/* 记录 IO 请求（按文件） */
void record_io_request(unsigned short ino, int block_no, int is_read) {
    if (!workload_initialized) init_workload_analyzer();
    if (ino == 0) return;
    
    /* 从 AI 学习参数加载该文件的预取窗口 */
    int ai_window = load_file_prefetch_window_from_ai(ino);
    
    struct file_io_history* fh = get_file_history(ino);
    if (!fh) return;
    
    /* 记录块访问 */
    fh->block_history[fh->history_idx] = block_no;
    fh->history_idx = (fh->history_idx + 1) % WORKLOAD_HISTORY;
    fh->last_block = block_no;
    
    if (is_read) fh->total_reads++;
    
    int seq_count = 0;
    int rand_count = 0;
    int total = 0;

    analyze_file_history(fh, &seq_count, &rand_count, &total);

    if (total == 0) {
        fh->current_type = WORKLOAD_UNKNOWN;
        fh->prefetch_window = ai_window;
        export_io_stats_to_ai();
        return;
    }
    
    if (seq_count >= rand_count * 2 && seq_count > 0) {
        fh->current_type = WORKLOAD_SEQUENTIAL;
        fh->prefetch_window = ai_window;
    } else if (rand_count >= seq_count * 2 && rand_count > 0) {
        fh->current_type = WORKLOAD_RANDOM;
        fh->prefetch_window = ai_window;
    } else {
        fh->current_type = WORKLOAD_UNKNOWN;
        fh->prefetch_window = ai_window;
    }

    if (fh->prefetch_window < 1) fh->prefetch_window = 1;
    if (fh->prefetch_window > 10) fh->prefetch_window = 10;
    
    /* 预取下一个块 */
    if (fh->current_type == WORKLOAD_SEQUENTIAL && fh->prefetch_window > 0) {
        int start_blk = block_no + 1;
        for (int i = 0; i < fh->prefetch_window; i++) {
            int prefetch_blk = start_blk + i;
            if (prefetch_blk < 0) continue;
            
            /* 检查是否已在缓存 */
            int found = 0;
            for (int j = 0; j < PREFETCH_CACHE_SIZE; j++) {
                if (workload.cache[j].ino == ino && workload.cache[j].block_no == prefetch_blk) {
                    found = 1;
                    break;
                }
            }
            
            if (!found) {
                /* 通过 inode 映射读取真实文件块到缓存。 */
                unsigned char tmp[BLOCK_SIZE];
                struct inode *ip = iget(ino);
                if (ip == NULL) continue;
                int disk_blk = bmap(ip, prefetch_blk);
                iput(ip);
                if (disk_blk == 0) continue;
                bread(disk_blk, tmp);
                
                workload.cache[workload.cache_idx].ino = ino;
                workload.cache[workload.cache_idx].block_no = prefetch_blk;
                memcpy(workload.cache[workload.cache_idx].data, tmp, BLOCK_SIZE);
                workload.cache[workload.cache_idx].timestamp = (unsigned long)time(NULL);
                workload.cache_idx = (workload.cache_idx + 1) % PREFETCH_CACHE_SIZE;
            }
        }
    }
    
    /* 导出最新的 IO 数据给 AI Agent */
    export_io_stats_to_ai();
}

/* 获取特定文件的预取窗口 */
int get_prefetch_window_for_file(unsigned short ino) {
    if (!workload_initialized) return 3;
    
    struct file_io_history* fh = get_file_history(ino);
    if (!fh) return 3;
    
    return fh->prefetch_window;
}

/* 从预取缓存读取（优先）- 新版本返回 1 表示命中，0 表示未命中 */
int get_prefetched_block(unsigned short ino, int block_no, unsigned char *buf) {
    if (!workload_initialized) return 0;
    
    for (int i = 0; i < PREFETCH_CACHE_SIZE; i++) {
        if (workload.cache[i].ino == ino && workload.cache[i].block_no == block_no) {
            workload.cache_hits++;
            if (buf) memcpy(buf, workload.cache[i].data, BLOCK_SIZE);
            return 1;
        }
    }
    
    workload.cache_misses++;
    return 0;
}

/* 显示 IO 统计（每个文件） */
void show_io_stats() {
    if (!workload_initialized) {
        printf("Workload analyzer not initialized.\n");
        return;
    }
    
    printf("\n========== IO 统计 (Per-File) ==========\n");
    printf("总记录文件数: %d\n", workload.file_count);
    printf("预取缓存命中: %d\n", workload.cache_hits);
    printf("预取缓存未命中: %d\n", workload.cache_misses);
    printf("\n--- 各文件状态 ---\n");
    
    for (int i = 0; i < MAX_INODES; i++) {
        if (workload.files[i].ino == 0) continue;
        
        struct file_io_history* fh = &workload.files[i];
        const char* type_str = "未知";
        
        if (fh->current_type == WORKLOAD_SEQUENTIAL) type_str = "顺序";
        else if (fh->current_type == WORKLOAD_RANDOM) type_str = "随机";
        
        printf("  Inode %d:\n", fh->ino);
        printf("    工作负载: %s\n", type_str);
        printf("    预取窗口: %d 块\n", fh->prefetch_window);
        printf("    最后访问块: %d\n", fh->last_block);
        printf("    总读取次数: %d\n", fh->total_reads);
        printf("\n");
    }
    
    printf("========================================\n\n");
}
