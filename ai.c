#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 初始化 AI 目录（在系统启动时调用） */
void init_agent_dirs() {
    if (sb.ai_mount_flag) {
        printf("Agent directories already mounted.\n");
        return;
    }
    
    printf("Initializing agent directories...\n");
    
    /* 检查 /agent 目录是否存在 */
    struct inode *agent_dir = namei("/agent");
    if (agent_dir == NULL) {
        printf("Agent directory not found, please format the disk.\n");
        return;
    }
    iput(agent_dir);
    
    sb.ai_mount_flag = 1;
    printf("Agent directories initialized successfully.\n");
}

/* 添加记忆 */
void add_memory(char *content, char is_long_term) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    
    char dir_path[64];
    char filename[64];
    
    /* 确定存储目录 */
    if (is_long_term) {
        strcpy(dir_path, "/agent/memory/long_term");
    } else {
        strcpy(dir_path, "/agent/memory/short_term");
    }
    
    /* 生成唯一文件名 */
    time_t t = time(NULL);
    sprintf(filename, "mem_%d_%ld.txt", cur_uid, (long)t);
    
    /* 保存当前目录 */
    unsigned short saved_dir = cur_dir;
    
    /* 切换到目标目录 */
    struct inode *target_dir = namei(dir_path);
    if (target_dir == NULL) {
        printf("Memory directory not found.\n");
        return;
    }
    cur_dir = target_dir->i_ino;
    iput(target_dir);
    
    /* 创建文件 */
    create(filename);
    
    /* 打开文件 */
    int fd = open(filename, O_WRONLY);
    if (fd == -1) {
        cur_dir = saved_dir;
        return;
    }
    
    /* 写入内容 */
    write(fd, (unsigned char*)content, strlen(content));
    close(fd);
    
    /* 恢复原目录 */
    cur_dir = saved_dir;
    
    printf("Memory added successfully.\n");
    log_ai_action("add_memory", filename, "success");
}

/* 列出所有记忆 */
void list_memories() {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    
    printf("=== Short-term memories ===\n");
    /* 保存当前目录 */
    unsigned short saved_dir = cur_dir;
    
    /* 切换到短期记忆目录 */
    struct inode *short_dir = namei("/agent/memory/short_term");
    if (short_dir != NULL) {
        cur_dir = short_dir->i_ino;
        iput(short_dir);
        dir();
    }
    
    printf("\n=== Long-term memories ===\n");
    /* 切换到长期记忆目录 */
    struct inode *long_dir = namei("/agent/memory/long_term");
    if (long_dir != NULL) {
        cur_dir = long_dir->i_ino;
        iput(long_dir);
        dir();
    }
    
    /* 恢复原目录 */
    cur_dir = saved_dir;
}

/* 调用 AI 工具 */
void call_ai_tool(char *tool_name, char *args) {
    if (cur_uid == -1) {
        printf("Not logged in.\n");
        return;
    }
    
    printf("Calling AI tool: %s\n", tool_name);
    printf("Arguments: %s\n", args);
    
    /* 构建 Python 命令 */
    char cmd[256];
    sprintf(cmd, "python agent_tools.py %s %d %s", tool_name, cur_uid, args);
    
    /* 执行 Python 脚本 */
    int result = system(cmd);
    
    if (result == 0) {
        printf("AI tool executed successfully.\n");
        log_ai_action("call_tool", tool_name, "success");
    } else {
        printf("AI tool execution failed.\n");
        log_ai_action("call_tool", tool_name, "failed");
    }
}

/* 记录 AI 操作日志 */
void log_ai_action(char *action, char *resource_path, char *result) {
    if (cur_uid == -1) {
        return;
    }
    
    char log_path[64] = "/agent/log";
    char filename[64];
    
    /* 保存当前目录 */
    unsigned short saved_dir = cur_dir;
    
    /* 切换到日志目录 */
    struct inode *log_dir = namei(log_path);
    if (log_dir == NULL) {
        return;
    }
    cur_dir = log_dir->i_ino;
    iput(log_dir);
    
    /* 生成日志文件名（按日期） */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    sprintf(filename, "log_%04d%02d%02d.txt", 
            tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
    
    /* 检查日志文件是否存在 */
    struct inode *log_file = namei(filename);
    if (log_file == NULL) {
        /* 文件不存在，创建它 */
        create(filename);
    } else {
        iput(log_file);
    }
    
    /* 打开文件（追加模式） */
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        cur_dir = saved_dir;
        return;
    }
    
    /* 移动到文件末尾 */
    struct file *f = &sysopenfile[u_ofile[fd]];
    f->f_offset = f->f_inode->i_din.di_size;
    
    /* 构建日志内容 */
    char log_entry[256];
    sprintf(log_entry, "[%ld] UID:%d ACTION:%s RESOURCE:%s RESULT:%s\n",
            (long)t, cur_uid, action, resource_path, result);
    
    /* 写入日志 */
    write(fd, (unsigned char*)log_entry, strlen(log_entry));
    close(fd);
    
    /* 恢复原目录 */
    cur_dir = saved_dir;
}

/* 清理过期记忆 */
void cleanup_expired_memories() {
    if (cur_uid != 0 && cur_uid != -1) {
        printf("Only root can clean up expired memories.\n");
        return;
    }
    
    printf("Cleaning up expired short-term memories...\n");
    printf("This function would remove memories older than configured time.\n");
    /* 这里可以实现更复杂的过期记忆清理逻辑 */
}

/* 自然语言交互 */
void nlp_interact(char *text) {
    if (cur_uid == -1) {
        printf("Please login first.\n");
        return;
    }
    
    printf("=== 智能助手 ===\n");
    printf("正在理解: %s\n", text);
    printf("-" * 40);
    
    /* 构建命令并调用 Python 解析器 */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "python nlp_parser.py \"%s\" 2>&1", text);
    
    /* 执行解析器并捕获输出 */
    FILE *pipe = popen(cmd, "r");
    if (pipe == NULL) {
        printf("Failed to initialize NLP parser.\n");
        return;
    }
    
    char output[1024];
    char exec_cmd[256] = "";
    int has_exec = 0;
    
    /* 读取输出 */
    while (fgets(output, sizeof(output), pipe) != NULL) {
        /* 检查是否是 EXEC: 开头的命令 */
        if (strncmp(output, "EXEC:", 5) == 0) {
            strncpy(exec_cmd, output + 5, sizeof(exec_cmd) - 1);
            /* 去除换行符 */
            char *newline = strchr(exec_cmd, '\n');
            if (newline) *newline = '\0';
            has_exec = 1;
        } else {
            /* 显示解析过程 */
            printf("%s", output);
        }
    }
    
    int status = pclose(pipe);
    
    if (has_exec && strlen(exec_cmd) > 0) {
        if (strcmp(exec_cmd, "unknown") != 0) {
            printf("\n" "=" * 40 "\n");
            printf("正在执行: %s\n", exec_cmd);
            printf("=" * 40 "\n\n");
            
            /* 解析并执行命令 */
            char cmd_name[64], arg1[256], arg2[256];
            int n = sscanf(exec_cmd, "%s %s %[^\n]", cmd_name, arg1, arg2);
            
            if (n >= 1) {
                if (strcmp(cmd_name, "add_memory") == 0 && n >= 3) {
                    char is_long = (strcmp(arg1, "long") == 0) ? 1 : 0;
                    add_memory(arg2, is_long);
                } else if (strcmp(cmd_name, "list_memories") == 0) {
                    list_memories();
                } else if (strcmp(cmd_name, "call_tool") == 0 && n >= 3) {
                    call_ai_tool(arg1, arg2);
                } else if (strcmp(cmd_name, "create") == 0 && n >= 2) {
                    create(arg1);
                } else if (strcmp(cmd_name, "delete") == 0 && n >= 2) {
                    delete(arg1);
                } else if (strcmp(cmd_name, "mkdir") == 0 && n >= 2) {
                    mkdir(arg1);
                } else if (strcmp(cmd_name, "rmdir") == 0 && n >= 2) {
                    rmdir(arg1);
                } else if (strcmp(cmd_name, "chdir") == 0 && n >= 2) {
                    chdir(arg1);
                } else if (strcmp(cmd_name, "dir") == 0) {
                    dir();
                } else if (strcmp(cmd_name, "help") == 0) {
                    /* help 命令在 main 中，这里简单输出 */
                    printf("Available commands:\n");
                    printf("  nlp <text> - Natural language interaction\n");
                }
            }
        }
    } else {
        printf("\n未能识别您的指令，请尝试更明确的表述。\n");
    }
}
