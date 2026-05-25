#include "filesystem.h"

int main(void) {
    int i;
    for (i = 0; i < NHINO; i++) inode[i] = NULL;
    for (i = 0; i < NOFILE; i++) u_ofile[i] = -1;
    init_file_locks();
    load_vdisk();
    char cmd[64], arg1[256], arg2[256];
    while (1) {
        printf("$ ");
        scanf("%s", cmd);
        if (strcmp(cmd, "login") == 0) {
            login();
        } else if (strcmp(cmd, "logout") == 0) {
            logout();
        } else if (strcmp(cmd, "format") == 0) {
            format();
        } else if (strcmp(cmd, "create") == 0) {
            scanf("%s", arg1);
            create(arg1);
        } else if (strcmp(cmd, "delete") == 0) {
            scanf("%s", arg1);
            delete(arg1);
        } else if (strcmp(cmd, "open") == 0) {
            scanf("%s %s", arg1, arg2);
            int mode = O_RDONLY;
            if (strcmp(arg2, "r") == 0) mode = O_RDONLY;
            else if (strcmp(arg2, "w") == 0) mode = O_WRONLY;
            else if (strcmp(arg2, "rw") == 0) mode = O_RDWR;
            open(arg1, mode);
        } else if (strcmp(cmd, "close") == 0) {
            int fd;
            scanf("%d", &fd);
            close(fd);
        } else if (strcmp(cmd, "read") == 0) {
            int fd, count;
            scanf("%d %d", &fd, &count);
            unsigned char *buf = (unsigned char*)malloc(count + 1);
            int n = read(fd, buf, count);
            if (n > 0) {
                buf[n] = '\0';
                printf("Content: %s\n", buf);
            }
            free(buf);
        } else if (strcmp(cmd, "write") == 0) {
            int fd;
            scanf("%d %s", &fd, arg1);
            write(fd, (unsigned char*)arg1, strlen(arg1));
        } else if (strcmp(cmd, "mkdir") == 0) {
            scanf("%s", arg1);
            mkdir(arg1);
        } else if (strcmp(cmd, "rmdir") == 0) {
            scanf("%s", arg1);
            rmdir(arg1);
        } else if (strcmp(cmd, "chmod") == 0) {
            unsigned short mode;
            scanf("%s %ho", arg1, &mode);
            chmod(arg1, mode);
        } else if (strcmp(cmd, "chdir") == 0) {
            scanf("%s", arg1);
            chdir(arg1);
        } else if (strcmp(cmd, "dir") == 0) {
            dir();
        } else if (strcmp(cmd, "exit") == 0) {
            save_vdisk();
            break;
        } else if (strcmp(cmd, "add_memory") == 0) {
            char type[10];
            scanf("%s %[^\n]", type, arg1);
            char is_long = (strcmp(type, "long") == 0) ? 1 : 0;
            add_memory(arg1, is_long);
        } else if (strcmp(cmd, "list_memories") == 0) {
            list_memories();
        } else if (strcmp(cmd, "call_tool") == 0) {
            scanf("%s %[^\n]", arg1, arg2);
            call_ai_tool(arg1, arg2);
        } else if (strcmp(cmd, "init_agent") == 0) {
            init_agent_dirs();
        } else if (strcmp(cmd, "cleanup_memories") == 0) {
            cleanup_expired_memories();
        } else if (strcmp(cmd, "nlp") == 0) {
            /* 读取自然语言输入 */
            getchar(); /* 清除换行符 */
            fgets(arg1, 256, stdin);
            /* 去除末尾换行符 */
            size_t len = strlen(arg1);
            if (len > 0 && arg1[len - 1] == '\n') {
                arg1[len - 1] = '\0';
            }
            nlp_interact(arg1);
        } else if (strcmp(cmd, "init_kfs") == 0) {
            init_kfs();
        } else if (strcmp(cmd, "kfs_list") == 0) {
            scanf("%[^\n]", arg1);
            kfs_list_virtual_dir(arg1);
        } else if (strcmp(cmd, "kfs_tags") == 0) {
            int ino;
            scanf("%d", &ino);
            kfs_show_tags(ino);
        } else if (strcmp(cmd, "init_io_opt") == 0) {
            init_workload_analyzer();
        } else if (strcmp(cmd, "io_stats") == 0) {
            show_io_stats();
        } else if (strcmp(cmd, "init_security") == 0) {
            init_security_system();
        } else if (strcmp(cmd, "user_profile") == 0) {
            show_user_profile();
        } else if (strcmp(cmd, "security_log") == 0) {
            show_security_events();
        } else if (strcmp(cmd, "help") == 0) {
            printf("Available commands:\n");
            printf("  login - Login to the system\n");
            printf("  logout - Logout from the system\n");
            printf("  format - Format the virtual disk\n");
            printf("  create <name> - Create a file\n");
            printf("  delete <name> - Delete a file\n");
            printf("  open <name> <mode> - Open a file (r/w/rw)\n");
            printf("  close <fd> - Close a file\n");
            printf("  read <fd> <count> - Read from file\n");
            printf("  write <fd> <data> - Write to file\n");
            printf("  mkdir <name> - Create a directory\n");
            printf("  rmdir <name> - Remove a directory\n");
            printf("  chmod <name> <mode> - Change file permissions\n");
            printf("  chdir <name> - Change current directory\n");
            printf("  dir - List directory contents\n");
            printf("  add_memory <short/long> <content> - Add a memory\n");
            printf("  list_memories - List all memories\n");
            printf("  call_tool <tool> <args> - Call an AI tool\n");
            printf("  nlp <text> - Natural language interaction\n");
            printf("  init_agent - Initialize agent directories\n");
            printf("  cleanup_memories - Clean up expired memories\n");
            printf("\n=== 创新功能 ===\n");
            printf("  [KFS 智能文件分类]\n");
            printf("    init_kfs - Initialize KFS system\n");
            printf("    kfs_list <vdir> - List virtual directory\n");
            printf("    kfs_tags <ino> - Show file tags\n");
            printf("  [AI I/O 优化]\n");
            printf("    init_io_opt - Initialize I/O optimizer\n");
            printf("    io_stats - Show I/O statistics\n");
            printf("  [安全异常检测]\n");
            printf("    init_security - Initialize security system\n");
            printf("    user_profile - Show user behavior profile\n");
            printf("    security_log - Show security events\n");
            printf("\n  help - Show this help message\n");
            printf("  exit - Exit the system\n");
            printf("\nNLP Examples:\n");
            printf("  nlp 记住这个重要的信息\n");
            printf("  nlp 查看记忆\n");
            printf("  nlp 总结：这是一段很长的文本...\n");
            printf("  nlp 创建文件 test\n");
        } else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }
    return 0;
}
