#include "filesystem.h"

static struct user_profile profiles[USERNUM];
static struct security_event security_log[64];
static int security_log_idx = 0;
static int security_initialized = 0;

void set_security_thresholds(int delete_thresh, int modify_thresh) {
    if (!security_initialized) init_security_system();
    
    for (int i = 0; i < USERNUM; i++) {
        profiles[i].delete_threshold = delete_thresh;
        profiles[i].modify_threshold = modify_thresh;
    }
    
    printf("安全阈值已更新:\n");
    printf("  - 删除阈值: %d 文件/分钟\n", delete_thresh);
    printf("  - 修改阈值: %d 文件/分钟\n", modify_thresh);
}

void set_delete_threshold(int threshold) {
    if (!security_initialized) init_security_system();
    
    for (int i = 0; i < USERNUM; i++) {
        profiles[i].delete_threshold = threshold;
    }
    printf("删除安全阈值已设置为: %d\n", threshold);
}

void set_modify_threshold(int threshold) {
    if (!security_initialized) init_security_system();
    
    for (int i = 0; i < USERNUM; i++) {
        profiles[i].modify_threshold = threshold;
    }
    printf("修改安全阈值已设置为: %d\n", threshold);
}

void init_security_system() {
    if (security_initialized) {
        printf("Security system already initialized.\n");
        return;
    }
    
    memset(profiles, 0, sizeof(profiles));
    memset(security_log, 0, sizeof(security_log));
    
    for (int i = 0; i < USERNUM; i++) {
        profiles[i].uid = i;
        profiles[i].delete_threshold = 5;
        profiles[i].modify_threshold = 10;
    }
    
    printf("=== AI 智能安全系统初始化完成 ===\n");
    printf("异常检测阈值:\n");
    printf("  - 每分钟删除阈值: 5 文件\n");
    printf("  - 每分钟修改阈值: 10 文件\n");
    security_initialized = 1;
}

void record_user_action(char *action, char *target) {
    if (cur_uid < 0 || cur_uid >= USERNUM) return;
    if (!security_initialized) init_security_system();
    
    struct user_profile* profile = &profiles[cur_uid];
    strncpy(profile->history[profile->history_idx].action, action, 31);
    strncpy(profile->history[profile->history_idx].target, target, 63);
    profile->history[profile->history_idx].timestamp = (unsigned long)time(NULL);
    profile->history_idx = (profile->history_idx + 1) % BEHAVIOR_HISTORY;
}

int detect_anomaly(char *action, char *target) {
    if (cur_uid < 0 || cur_uid >= USERNUM) return 0;
    if (!security_initialized) init_security_system();
    
    struct user_profile* profile = &profiles[cur_uid];
    unsigned long now = (unsigned long)time(NULL);
    int is_anomaly = 0;
    char description[256] = "";
    
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
    
    if (strcmp(action, "delete") == 0 && recent_deletes >= profile->delete_threshold) {
        snprintf(description, 256, "检测到短时间内大量删除操作: %d 次/分钟", recent_deletes);
        is_anomaly = 1;
    }
    
    if ((strcmp(action, "write") == 0 || strcmp(action, "chmod") == 0) &&
        recent_modifies >= profile->modify_threshold) {
        snprintf(description, 256, "检测到短时间内大量修改操作: %d 次/分钟", recent_modifies);
        is_anomaly = 1;
    }
    
    if (is_anomaly) {
        printf("\n⚠️  安全警告: %s\n", description);
        printf("是否继续操作? (y/n): ");
        
        strncpy(security_log[security_log_idx].event_type, "ANOMALY", 31);
        strncpy(security_log[security_log_idx].description, description, 255);
        security_log[security_log_idx].uid = cur_uid;
        security_log[security_log_idx].timestamp = now;
        security_log[security_log_idx].blocked = 0;
        security_log_idx = (security_log_idx + 1) % 64;
        
        char confirm;
        scanf(" %c", &confirm);
        if (confirm != 'y' && confirm != 'Y') {
            printf("操作已取消.\n");
            return 1;
        }
    }
    
    record_user_action(action, target);
    return 0;
}

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
