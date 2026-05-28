#include "filesystem.h"

void print_block_status() {
    int block_used[512] = {0};  // 512个块，0=空闲，1=占用
    int blkno, i;

    // 首先，超级块和inode块是被占用的
    block_used[SUPERBLOCK] = 1;
    for (i = 0; i < DINODEBLK; i++) {
        block_used[DINODESTART + i] = 1;
    }

    // 扫描所有inode，找出被占用的块
    for (i = 1; i <= DINODEBLK * (BLOCKSIZ / DINODESIZ); i++) {
        struct dinode di;
        iget_inode(i, &di);

        if (di.di_mode == 0) continue;  // 空闲inode

        // 直接块
        for (int j = 0; j < 6; j++) {
            if (di.di_addr[j] != 0 && di.di_addr[j] < 512) {
                block_used[di.di_addr[j]] = 1;
            }
        }
        // 一级间接块
        if (di.di_addr[6] != 0 && di.di_addr[6] < 512) {
            block_used[di.di_addr[6]] = 1;
            bread(di.di_addr[6], block_buf);
            for (int j = 0; j < 128; j++) {
                int bn = ((unsigned short*)block_buf)[j];
                if (bn != 0 && bn < 512) {
                    block_used[bn] = 1;
                }
            }
        }
        // 二级间接块
        if (di.di_addr[7] != 0 && di.di_addr[7] < 512) {
            block_used[di.di_addr[7]] = 1;
            bread(di.di_addr[7], block_buf);
            for (int j = 0; j < 128; j++) {
                int bn1 = ((unsigned short*)block_buf)[j];
                if (bn1 != 0 && bn1 < 512) {
                    block_used[bn1] = 1;
                    bread(bn1, block_buf);
                    for (int k = 0; k < 128; k++) {
                        int bn2 = ((unsigned short*)block_buf)[k];
                        if (bn2 != 0 && bn2 < 512) {
                            block_used[bn2] = 1;
                        }
                    }
                }
            }
        }
    }

    // 输出块状态，格式 "BLOCK_STATUS:010101..."
    printf("BLOCK_STATUS:");
    for (i = 0; i < 512; i++) {
        printf("%d", block_used[i]);
    }
    printf("\n");
}

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
        } else if (strcmp(cmd, "init_integration") == 0) {
            init_integration();
        } else if (strcmp(cmd, "optimize") == 0) {
            integration_apply_optimization();
        } else if (strcmp(cmd, "suggestions") == 0) {
            integration_show_suggestions();
        } else if (strcmp(cmd, "analyze") == 0) {
            integration_show_suggestions();
        } else if (strcmp(cmd, "blocks") == 0) {
            print_block_status();
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
            printf("  blocks - Show block usage status\n");
            printf("  nlp <text> - Natural language interaction\n");
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
            printf("\n=== 集成层：记忆优化 ===\n");
            printf("  [记忆优化]\n");
            printf("    init_integration - Initialize integration layer\n");
            printf("    optimize - Apply optimization based on learning\n");
            printf("    suggestions - Show optimization suggestions\n");
            printf("  [行为分析]\n");
            printf("    analyze - Trigger behavior analysis\n");
            printf("\n  help - Show this help message\n");
            printf("  exit - Exit the system\n");
            printf("\nNLP Examples:\n");
            printf("  nlp 创建文件 test\n");
        } else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
    }
    return 0;
}
