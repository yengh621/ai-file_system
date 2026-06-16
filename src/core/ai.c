#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* === 集成层：多智能体系统 === */

void init_integration() {
    printf("=== 初始化多智能体集成层 ===\n");
    printf("🤖 可用智能体:\n");
    printf("   - Recorder (记录)\n");
    printf("   - Analyzer (分析 + GLM)\n");
    printf("   - KFS Agent (文件分类优化)\n");
    printf("   - IO Agent (I/O 优化)\n");
    printf("   - Security Agent (安全优化)\n");
    printf("   - Integrator (整合)\n");
    printf("✅ 集成层初始化完成\n\n");
}

void integration_set_user(int uid) {
    init_workload_analyzer();
    printf("=== 激活多智能体系统 ===\n");
    printf("👤 用户 ID: %d\n", uid);
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "python -m ai.cli set_user %d 2>&1", uid);
    
    FILE *pipe = popen(cmd, "r");
    if (pipe != NULL) {
        char output[256];
        while (fgets(output, sizeof(output), pipe) != NULL) {
            printf("%s", output);
        }
        pclose(pipe);
    }
    
    printf("\n🤖 启动后台分析守护进程...\n");
    snprintf(cmd, sizeof(cmd), "python -m ai.cli start_daemon %d 2>&1", uid);
    pipe = popen(cmd, "r");
    if (pipe != NULL) {
        char output[256];
        while (fgets(output, sizeof(output), pipe) != NULL) {
            printf("%s", output);
        }
        pclose(pipe);
    }
    
    printf("\n🤖 多智能体系统正在学习您的行为...\n");
    printf("   登录时立即分析，之后每10分钟自动分析\n\n");
}

void integration_clear_user() {
    if (cur_uid == -1) {
        printf("没有活动用户\n");
        return;
    }
    
    printf("=== 停止多智能体系统 ===\n");
    
    printf("🤖 停止后台分析守护进程...\n");
    char cmd[] = "python -m ai.cli stop_daemon 2>&1";
    FILE *pipe = popen(cmd, "r");
    if (pipe != NULL) {
        char output[256];
        while (fgets(output, sizeof(output), pipe) != NULL) {
            printf("%s", output);
        }
        pclose(pipe);
    }
    
    pipe = popen("python -m ai.cli clear_user 2>&1", "r");
    if (pipe != NULL) {
        char output[256];
        while (fgets(output, sizeof(output), pipe) != NULL) {
            printf("%s", output);
        }
        pclose(pipe);
    }
    
    printf("\n");
}

void integration_record_operation(char *operation, char *path) {
    if (cur_uid == -1) return;
    
    char cmd[512];
    if (path) {
            snprintf(cmd, sizeof(cmd), "python -m ai.cli record_for_user %d %s \"%s\" 2>&1",
                     cur_uid, operation, path);
        } else {
            snprintf(cmd, sizeof(cmd), "python -m ai.cli record_for_user %d %s 2>&1",
                     cur_uid, operation);
        }
    
    FILE *pipe = popen(cmd, "r");
    if (pipe != NULL) {
        pclose(pipe);
    }
}

void integration_apply_optimization() {
    if (cur_uid == -1) {
        printf("请先登录\n");
        return;
    }
    
    printf("=== 多智能体协同优化 ===\n");
    printf("🤖 正在分析并应用优化...\n\n");
    
    char cmd[] = "python -m ai.cli get_optimization 2>&1";
    
    FILE *pipe = popen(cmd, "r");
    if (pipe == NULL) {
        printf("调用优化失败\n");
        return;
    }
    
    char output[2048];
    if (fgets(output, sizeof(output), pipe) != NULL) {
        int prefetch_window = 3;
        int delete_threshold = 5;
        int modify_threshold = 10;
        
        char *prefetch_ptr = strstr(output, "\"prefetch_window\":");
        if (prefetch_ptr != NULL) {
            prefetch_window = atoi(prefetch_ptr + 18);
        }
        
        char *delete_ptr = strstr(output, "\"delete_threshold\":");
        if (delete_ptr != NULL) {
            delete_threshold = atoi(delete_ptr + 19);
        }
        
        char *modify_ptr = strstr(output, "\"modify_threshold\":");
        if (modify_ptr != NULL) {
            modify_threshold = atoi(modify_ptr + 19);
        }
        
        printf("🤖 应用优化配置:\n");
        printf("   - I/O 预取窗口: %d\n", prefetch_window);
        printf("   - 删除安全阈值: %d\n", delete_threshold);
        printf("   - 修改安全阈值: %d\n\n", modify_threshold);
        
        set_prefetch_window(prefetch_window);
        set_security_thresholds(delete_threshold, modify_threshold);
    }
    
    pclose(pipe);
    printf("\n✅ 优化已全部应用！\n\n");
}

void integration_show_suggestions() {
    if (cur_uid == -1) {
        printf("请先登录\n");
        return;
    }
    
    printf("=== 最新分析建议 ===\n");
    
    char cmd[] = "python -m ai.cli get_last_analysis 2>&1";
    
    FILE *pipe = popen(cmd, "r");
    if (pipe != NULL) {
        char output[4096];
        while (fgets(output, sizeof(output), pipe) != NULL) {
            printf("%s", output);
        }
        pclose(pipe);
    }
    
    printf("\n");
}

void integration_start_session() {
    printf("=== 开始优化会话 ===\n");
    printf("正在加载学习参数...\n");
    kfs_load_hot_files_from_ai();
    printf("会话已启动\n");
}

void integration_end_session() {
    printf("=== 结束优化会话 ===\n");
    kfs_save_to_disk();
    printf("会话已结束，数据已保存\n");
}
