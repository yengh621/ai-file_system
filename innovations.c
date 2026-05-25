#include "filesystem.h"

/* === 全局变量 === */
static struct kfs_tag kfs_tags[FILEBLK];  /* 文件标签存储 */
static int kfs_initialized = 0;

static struct workload_analyzer workload;
static int workload_initialized = 0;

static struct user_profile profiles[USERNUM];
static struct security_event security_log[64];
static int security_log_idx = 0;
static int security_initialized = 0;

/* === 二、组织层创新：KFS 智能文件分类 === */

/* 获取文件扩展名 */
static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    return dot ? dot + 1 : "";
}

/* 根据扩展名判断文件类型 */
static KFSType determine_file_type(const char* filename) {
    const char* ext = get_extension(filename);
    
    /* 代码文件 */
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0 ||
        strcmp(ext, "py") == 0 || strcmp(ext, "java") == 0 ||
        strcmp(ext, "cpp") == 0 || strcmp(ext, "js") == 0) {
        return KFS_TYPE_CODE;
    }
    
    /* 文档文件 */
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "doc") == 0 ||
        strcmp(ext, "pdf") == 0 || strcmp(ext, "md") == 0) {
        return KFS_TYPE_DOC;
    }
    
    /* 图片文件 */
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0 ||
        strcmp(ext, "gif") == 0 || strcmp(ext, "bmp") == 0) {
        return KFS_TYPE_IMAGE;
    }
    
    /* 数据文件 */
    if (strcmp(ext, "dat") == 0 || strcmp(ext, "bin") == 0 ||
        strcmp(ext, "csv") == 0 || strcmp(ext, "json") == 0) {
        return KFS_TYPE_DATA;
    }
    
    return KFS_TYPE_OTHER;
}

/* 根据时间判断分类 */
static KFSTime determine_time_cat(unsigned long created) {
    time_t now = time(NULL);
    unsigned long diff = (unsigned long)now - created;
    
    if (diff < 86400) return KFS_TIME_TODAY;        /* 1天 */
    if (diff < 604800) return KFS_TIME_WEEK;       /* 7天 */
    if (diff < 2592000) return KFS_TIME_MONTH;     /* 30天 */
    return KFS_TIME_OLD;
}

/* 初始化 KFS */
void init_kfs() {
    if (kfs_initialized) {
        printf("KFS already initialized.\n");
        return;
    }
    
    memset(kfs_tags, 0, sizeof(kfs_tags));
    
    /* 在 format 时已经创建了 /kfs 目录，这里只做初始化 */
    printf("=== KFS 智能文件系统初始化完成 ===\n");
    printf("虚拟目录结构:\n");
    printf("  /kfs/type/    - 按类型分类\n");
    printf("  /kfs/time/    - 按时间分类\n");
    printf("  /kfs/project/ - 按项目分类\n");
    kfs_initialized = 1;
}

/* 自动分类文件 */
void kfs_classify_file(char *filename, unsigned short ino) {
    if (!kfs_initialized) init_kfs();
    
    struct kfs_tag* tag = &kfs_tags[ino % FILEBLK];
    tag->ino = ino;
    tag->type = determine_file_type(filename);
    tag->created = (unsigned long)time(NULL);
    tag->last_access = tag->created;
    tag->time_cat = determine_time_cat(tag->created);
    tag->tag_count = 0;
    
    /* 添加标签 */
    const char* type_names[] = {"code", "document", "image", "data", "other"};
    strncpy(tag->tags[tag->tag_count++], type_names[tag->type], 31);
    
    /* 简单的项目分类（基于文件名关键词） */
    if (strstr(filename, "os") || strstr(filename, "file")) {
        strncpy(tag->tags[tag->tag_count++], "os_course", 31);
    }
    if (strstr(filename, "ai") || strstr(filename, "agent")) {
        strncpy(tag->tags[tag->tag_count++], "ai_design", 31);
    }
    
    printf("✓ 文件 '%s' 已自动分类:\n", filename);
    printf("  - 类型: %s\n", type_names[tag->type]);
    printf("  - 时间: %s\n", 
           tag->time_cat == KFS_TIME_TODAY ? "today" :
           tag->time_cat == KFS_TIME_WEEK ? "this_week" :
           tag->time_cat == KFS_TIME_MONTH ? "recent_30days" : "old");
    printf("  - 标签: ");
    for (int i = 0; i < tag->tag_count; i++) {
        printf("%s ", tag->tags[i]);
    }
    printf("\n");
}

/* 列出虚拟目录 */
void kfs_list_virtual_dir(char *vdir_path) {
    if (!kfs_initialized) {
        printf("KFS not initialized. Run 'init_kfs' first.\n");
        return;
    }
    
    printf("=== 虚拟目录: %s ===\n", vdir_path);
    
    /* 遍历所有文件标签，找出匹配的 */
    int found = 0;
    for (int i = 0; i < FILEBLK; i++) {
        if (kfs_tags[i].ino == 0) continue;
        
        int match = 0;
        if (strstr(vdir_path, "/kfs/type/")) {
            const char* type_name = strrchr(vdir_path, '/') + 1;
            const char* type_names[] = {"code", "document", "image", "data", "other"};
            if (strcmp(type_name, type_names[kfs_tags[i].type]) == 0) {
                match = 1;
            }
        } else if (strstr(vdir_path, "/kfs/time/")) {
            const char* time_name = strrchr(vdir_path, '/') + 1;
            const char* time_names[] = {"today", "this_week", "recent_30days", "old"};
            if (strcmp(time_name, time_names[kfs_tags[i].time_cat]) == 0) {
                match = 1;
            }
        } else {
            /* 检查标签匹配 */
            for (int t = 0; t < kfs_tags[i].tag_count; t++) {
                if (strstr(vdir_path, kfs_tags[i].tags[t])) {
                    match = 1;
                    break;
                }
            }
        }
        
        if (match) {
            /* 这里应该根据 ino 查找文件名，简化处理 */
            printf("  [ino:%d]\n", kfs_tags[i].ino);
            found++;
        }
    }
    
    if (found == 0) {
        printf("  (空目录)\n");
    }
}

/* 显示文件标签 */
void kfs_show_tags(unsigned short ino) {
    if (!kfs_initialized) {
        printf("KFS not initialized.\n");
        return;
    }
    
    struct kfs_tag* tag = &kfs_tags[ino % FILEBLK];
    if (tag->ino != ino) {
        printf("文件未分类或不存在.\n");
        return;
    }
    
    const char* type_names[] = {"code", "document", "image", "data", "other"};
    const char* time_names[] = {"today", "this_week", "recent_30days", "old"};
    
    printf("=== 文件 [ino:%d] 分类信息 ===\n", ino);
    printf("类型: %s\n", type_names[tag->type]);
    printf("时间: %s\n", time_names[tag->time_cat]);
    printf("标签: ");
    for (int i = 0; i < tag->tag_count; i++) {
        printf("%s ", tag->tags[i]);
    }
    printf("\n");
}

/* === 三、性能层创新：AI 自适应 I/O 优化 === */

/* 初始化 workload 分析器 */
void init_workload_analyzer() {
    if (workload_initialized) {
        printf("Workload analyzer already initialized.\n");
        return;
    }
    
    memset(&workload, 0, sizeof(workload));
    workload.prefetch_window = 3;  /* 默认预取 3 块 */
    workload.current_type = WORKLOAD_UNKNOWN;
    
    printf("=== AI 自适应 I/O 优化初始化完成 ===\n");
    workload_initialized = 1;
}

/* 记录 I/O 请求 */
void record_io_request(unsigned short ino, int block_no, int is_read) {
    if (!workload_initialized) init_workload_analyzer();
    
    workload.history[workload.history_idx].ino = ino;
    workload.history[workload.history_idx].block_no = block_no;
    workload.history[workload.history_idx].is_read = is_read;
    workload.history[workload.history_idx].timestamp = (unsigned long)time(NULL);
    workload.history_idx = (workload.history_idx + 1) % WORKLOAD_HISTORY;
}

/* 分析 workload 类型 */
void analyze_workload() {
    if (!workload_initialized) return;
    
    int seq_count = 0;
    int rand_count = 0;
    int total = 0;
    
    /* 分析最近的请求模式 */
    for (int i = 1; i < WORKLOAD_HISTORY; i++) {
        int prev = (workload.history_idx - i - 1 + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        int curr = (workload.history_idx - i + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        
        if (workload.history[prev].ino == 0 || workload.history[curr].ino == 0) continue;
        
        if (workload.history[prev].ino == workload.history[curr].ino) {
            int diff = workload.history[curr].block_no - workload.history[prev].block_no;
            if (diff == 1) {
                seq_count++;  /* 顺序访问 */
            } else if (abs(diff) > 1) {
                rand_count++;  /* 随机访问 */
            }
        }
        total++;
    }
    
    if (total < 10) {
        workload.current_type = WORKLOAD_UNKNOWN;
        return;
    }
    
    /* 决策逻辑 */
    if (seq_count > rand_count * 2) {
        workload.current_type = WORKLOAD_SEQUENTIAL;
        workload.prefetch_window = 10;  /* 增大预取窗口 */
    } else if (rand_count > seq_count * 2) {
        workload.current_type = WORKLOAD_RANDOM;
        workload.prefetch_window = 0;  /* 关闭预取 */
    } else {
        workload.current_type = WORKLOAD_UNKNOWN;
        workload.prefetch_window = 3;
    }
}

/* 获取当前预取窗口大小 */
int get_prefetch_window() {
    analyze_workload();
    return workload.prefetch_window;
}

/* 显示 I/O 统计 */
void show_io_stats() {
    if (!workload_initialized) {
        printf("Workload analyzer not initialized.\n");
        return;
    }
    
    const char* type_names[] = {"Sequential", "Random", "Stream", "Unknown"};
    
    printf("=== AI I/O 优化统计 ===\n");
    printf("Current Workload: %s\n", type_names[workload.current_type]);
    printf("Prefetch Window: %d blocks\n", workload.prefetch_window);
    printf("Cache Hits: %d\n", workload.cache_hits);
    printf("Cache Misses: %d\n", workload.cache_misses);
    
    if (workload.current_type == WORKLOAD_SEQUENTIAL) {
        printf("✓ 检测到大文件顺序读，已增大预取窗口以提升带宽利用率\n");
    } else if (workload.current_type == WORKLOAD_RANDOM) {
        printf("✓ 检测到小文件随机读，已关闭预取以节省内存\n");
    }
}

/* === 四、安全层创新：智能行为异常检测 === */

/* 初始化安全系统 */
void init_security_system() {
    if (security_initialized) {
        printf("Security system already initialized.\n");
        return;
    }
    
    memset(profiles, 0, sizeof(profiles));
    memset(security_log, 0, sizeof(security_log));
    
    /* 设置默认阈值 */
    for (int i = 0; i < USERNUM; i++) {
        profiles[i].uid = i;
        profiles[i].delete_threshold = 5;   /* 每分钟最多删除 5 个 */
        profiles[i].modify_threshold = 10;  /* 每分钟最多修改 10 个 */
    }
    
    printf("=== AI 智能安全系统初始化完成 ===\n");
    printf("异常检测阈值:\n");
    printf("  - 每分钟删除阈值: 5 文件\n");
    printf("  - 每分钟修改阈值: 10 文件\n");
    security_initialized = 1;
}

/* 记录用户操作 */
void record_user_action(char *action, char *target) {
    if (cur_uid < 0 || cur_uid >= USERNUM) return;
    if (!security_initialized) init_security_system();
    
    struct user_profile* profile = &profiles[cur_uid];
    strncpy(profile->history[profile->history_idx].action, action, 31);
    strncpy(profile->history[profile->history_idx].target, target, 63);
    profile->history[profile->history_idx].timestamp = (unsigned long)time(NULL);
    profile->history_idx = (profile->history_idx + 1) % BEHAVIOR_HISTORY;
}

/* 检测异常行为 */
int detect_anomaly(char *action, char *target) {
    if (cur_uid < 0 || cur_uid >= USERNUM) return 0;
    if (!security_initialized) init_security_system();
    
    struct user_profile* profile = &profiles[cur_uid];
    unsigned long now = (unsigned long)time(NULL);
    int is_anomaly = 0;
    char description[256] = "";
    
    /* 统计最近 1 分钟的删除操作 */
    int recent_deletes = 0;
    int recent_modifies = 0;
    
    for (int i = 0; i < BEHAVIOR_HISTORY; i++) {
        if (profile->history[i].timestamp == 0) continue;
        if (now - profile->history[i].timestamp < 60) {
            if (strcmp(profile->history[i].action, "delete") == 0) {
                recent_deletes++;
            } else if (strcmp(profile->history[i].action, "write") == 0 ||
                       strcmp(profile->history[i].action, "chmod") == 0) {
                recent_modifies++;
            }
        }
    }
    
    /* 检查删除阈值 */
    if (strcmp(action, "delete") == 0 && recent_deletes >= profile->delete_threshold) {
        snprintf(description, 256, "检测到短时间内大量删除操作: %d 次/分钟", recent_deletes);
        is_anomaly = 1;
    }
    
    /* 检查修改阈值 */
    if ((strcmp(action, "write") == 0 || strcmp(action, "chmod") == 0) &&
        recent_modifies >= profile->modify_threshold) {
        snprintf(description, 256, "检测到短时间内大量修改操作: %d 次/分钟", recent_modifies);
        is_anomaly = 1;
    }
    
    /* 记录安全事件 */
    if (is_anomaly) {
        printf("\n⚠️  安全警告: %s\n", description);
        printf("是否继续操作? (y/n): ");
        
        /* 记录安全事件 */
        strncpy(security_log[security_log_idx].event_type, "ANOMALY", 31);
        strncpy(security_log[security_log_idx].description, description, 255);
        security_log[security_log_idx].uid = cur_uid;
        security_log[security_log_idx].timestamp = now;
        security_log[security_log_idx].blocked = 0;  /* 默认不阻止，仅警告 */
        security_log_idx = (security_log_idx + 1) % 64;
        
        /* 简单的确认机制 */
        char confirm;
        scanf(" %c", &confirm);
        if (confirm != 'y' && confirm != 'Y') {
            printf("操作已取消.\n");
            return 1;  /* 阻止操作 */
        }
    }
    
    record_user_action(action, target);
    return 0;
}

/* 显示用户行为画像 */
void show_user_profile() {
    if (cur_uid < 0 || cur_uid >= USERNUM) {
        printf("请先登录.\n");
        return;
    }
    if (!security_initialized) init_security_system();
    
    struct user_profile* profile = &profiles[cur_uid];
    printf("=== 用户行为画像 [UID:%d] ===\n", cur_uid);
    printf("删除阈值: %d/分钟\n", profile->delete_threshold);
    printf("修改阈值: %d/分钟\n", profile->modify_threshold);
    
    /* 统计最近操作 */
    int recent_actions = 0;
    unsigned long now = (unsigned long)time(NULL);
    for (int i = 0; i < BEHAVIOR_HISTORY; i++) {
        if (profile->history[i].timestamp && 
            now - profile->history[i].timestamp < 3600) {
            recent_actions++;
        }
    }
    printf("最近 1 小时操作数: %d\n", recent_actions);
}

/* 显示安全事件日志 */
void show_security_events() {
    if (!security_initialized) {
        printf("Security system not initialized.\n");
        return;
    }
    
    printf("=== 安全事件日志 ===\n");
    int has_events = 0;
    
    for (int i = 0; i < 64; i++) {
        if (security_log[i].timestamp == 0) continue;
        
        has_events = 1;
        printf("[%lu] UID:%d - %s: %s\n",
               security_log[i].timestamp,
               security_log[i].uid,
               security_log[i].event_type,
               security_log[i].description);
    }
    
    if (!has_events) {
        printf("  (无安全事件)\n");
    }
}
