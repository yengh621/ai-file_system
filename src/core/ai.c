#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 自然语言交互（GLM 集成）*/
void nlp_interact(char *text) {
    if (cur_uid == -1) {
        printf("请先登录\n");
        return;
    }
    
    printf("=== 智能助手 (GLM 支持) ===\n");
    printf("正在理解: %s\n", text);
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "python ai/glm_integration.py \"%s\" 2>&1", text);
    
    FILE *pipe = popen(cmd, "r");
    if (pipe == NULL) {
        printf("调用 GLM 失败\n");
        return;
    }
    
    char output[1024];
    char exec_cmd[256] = "";
    int has_exec = 0;
    
    while (fgets(output, sizeof(output), pipe) != NULL) {
        if (strncmp(output, "EXEC:", 5) == 0) {
            strncpy(exec_cmd, output + 5, sizeof(exec_cmd) - 1);
            char *newline = strchr(exec_cmd, '\n');
            if (newline) *newline = '\0';
            has_exec = 1;
        } else {
            printf("%s", output);
        }
    }
    
    pclose(pipe);
    
    if (has_exec && strlen(exec_cmd) > 0) {
        if (strcmp(exec_cmd, "unknown") != 0) {
            printf("\n========================================\n");
            printf("正在执行: %s\n", exec_cmd);
            printf("========================================\n\n");
            
            char cmd_name[64], arg1[256], arg2[256];
            int n = sscanf(exec_cmd, "%s %s %[^\n]", cmd_name, arg1, arg2);
            
            int cmd_success = 0;
            if (n >= 1) {
                if (strcmp(cmd_name, "create") == 0 && n >= 2) {
                    create(arg1);
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "delete") == 0 && n >= 2) {
                    delete(arg1);
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "mkdir") == 0 && n >= 2) {
                    mkdir(arg1);
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "rmdir") == 0 && n >= 2) {
                    rmdir(arg1);
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "chdir") == 0 && n >= 2) {
                    chdir(arg1);
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "dir") == 0) {
                    dir();
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "help") == 0) {
                    printf("可用命令查看 help\n");
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "start_session") == 0) {
                    integration_start_session();
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "end_session") == 0) {
                    integration_end_session();
                    cmd_success = 1;
                } else if (strcmp(cmd_name, "optimize") == 0) {
                    integration_apply_optimization();
                    cmd_success = 1;
                }
            }
            
            if (cmd_success) {
                printf("\n✅ 命令已成功执行\n");
            }
        }
    } else {
        printf("\n未能识别，请尝试更明确的表述，或检查 config.json 中的 API 密钥\n");
    }
}

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
            snprintf(cmd, sizeof(cmd), "python -m ai.cli record %s \"%s\" 2>&1", 
                     operation, path);
        } else {
            snprintf(cmd, sizeof(cmd), "python -m ai.cli record %s 2>&1", operation);
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

