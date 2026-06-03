#include "filesystem.h"

static void mark_data_block(int absolute_blkno, int block_used[FILEBLK]) {
    int data_index = absolute_blkno - DATASTART;
    if (data_index >= 0 && data_index < FILEBLK) {
        block_used[data_index] = 1;
    }
}

#if 0
void print_block_status() {
    int block_used[FILEBLK] = {0};
    int i;

    // 首先，超级块和inode块是被占用的
    // 扫描所有inode，找出被占用的块
    for (i = 1; i <= DINODEBLK * (BLOCKSIZ / DINODESIZ); i++) {
        struct dinode di;
        iget_inode(i, &di);

        if (di.di_mode == 0) continue;  // 空闲inode

        // 直接块
        for (int j = 0; j < 6; j++) {
            mark_data_block(di.di_addr[j], block_used);
        }
        // 一级间接块
        if (di.di_addr[6] != 0) {
            mark_data_block(di.di_addr[6], block_used);
            bread(di.di_addr[6], block_buf);
            for (int j = 0; j < 128; j++) {
                int bn = ((unsigned short*)block_buf)[j];
                mark_data_block(bn, block_used);
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

#endif

void print_block_status() {
    int block_used[FILEBLK] = {0};
    int inode_used[DINODEBLK * (BLOCKSIZ / DINODESIZ)] = {0};
    int i;

    for (i = 1; i <= DINODEBLK * (BLOCKSIZ / DINODESIZ); i++) {
        struct dinode di;
        iget_inode(i, &di);

        if (di.di_mode == 0) continue;
        inode_used[i - 1] = 1;

        for (int j = 0; j < 6; j++) {
            mark_data_block(di.di_addr[j], block_used);
        }

        if (di.di_addr[6] != 0) {
            mark_data_block(di.di_addr[6], block_used);
            bread(di.di_addr[6], block_buf);
            for (int j = 0; j < 128; j++) {
                mark_data_block(((unsigned short*)block_buf)[j], block_used);
            }
        }

        if (di.di_addr[7] != 0) {
            mark_data_block(di.di_addr[7], block_used);
            bread(di.di_addr[7], block_buf);
            for (int j = 0; j < 128; j++) {
                int bn1 = ((unsigned short*)block_buf)[j];
                if (bn1 != 0) {
                    mark_data_block(bn1, block_used);
                    bread(bn1, block_buf);
                    for (int k = 0; k < 128; k++) {
                        mark_data_block(((unsigned short*)block_buf)[k], block_used);
                    }
                }
            }
        }
    }

    printf("BLOCK_STATUS:");
    for (i = 0; i < FILEBLK; i++) {
        printf("%d", block_used[i]);
    }
    printf("\n");

    printf("INODE_STATUS:");
    for (i = 0; i < DINODEBLK * (BLOCKSIZ / DINODESIZ); i++) {
        printf("%d", inode_used[i]);
    }
    printf("\n");
}

int main(void) {
    int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    for (i = 0; i < NHINO; i++) inode[i] = NULL;
    for (i = 0; i < NOFILE; i++) u_ofile[i] = -1;
    init_file_locks();
    load_vdisk();
    char cmd[64], arg1[256], arg2[256];
    while (1) {
        printf("$ ");
        fflush(stdout);
        scanf("%s", cmd);
        if (strcmp(cmd, "login") == 0) {
            login();
        } else if (strcmp(cmd, "logout") == 0) {
            logout();
        } else if (strcmp(cmd, "format") == 0) {
            if (cur_uid == -1) {
                printf("Not logged in.\n");
            } else if (cur_uid != 0) {
                printf("Permission denied. Only root can format.\n");
            } else {
                format();
                cur_dir = 1;
            }
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
            else if (strcmp(arg2, "a") == 0) mode = O_APPEND;
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
                printf("Content:\n");
                for (int i = 0; i < n; i++) {
                    putchar(buf[i]);
                }
                printf("\n");
            }
            free(buf);
        } else if (strcmp(cmd, "write") == 0) {
            int fd;
            scanf("%d", &fd);
            fgets(arg1, 1024, stdin);
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
        } else if (strcmp(cmd, "grant") == 0) {
            int writable = 0;
            scanf("%s %s %d", arg1, arg2, &writable);
            grant(arg1, arg2, writable);
        } else if (strcmp(cmd, "chdir") == 0 || strcmp(cmd, "cd") == 0) {
            scanf("%s", arg1);
            chdir(arg1);
        } else if (strcmp(cmd, "dir") == 0 || strcmp(cmd, "ls") == 0) {
            dir();
        } else if (strcmp(cmd, "exit") == 0) {
            save_vdisk();
            break;
        } else if (strcmp(cmd, "init_kfs") == 0) {
            init_kfs();
        } else if (strcmp(cmd, "kfs_list") == 0) {
            scanf("%[^\n]", arg1);
            kfs_list_virtual_dir(arg1);
        } else if (strcmp(cmd, "kfs_tags") == 0) {
            int ino;
            scanf("%d", &ino);
            kfs_show_tags(ino);
        } else if (strcmp(cmd, "hot_cache") == 0) {
            kfs_hot_cache_show();
        } else if (strcmp(cmd, "kfs_save") == 0) {
            kfs_save_to_disk();
        } else if (strcmp(cmd, "kfs_load") == 0) {
            kfs_load_from_disk();
        } else if (strcmp(cmd, "kfs_ai_select") == 0) {
            kfs_ai_select_hot_files();
        } else if (strcmp(cmd, "kfs_memory_map") == 0) {
            kfs_show_memory_map();
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
        } else if (strcmp(cmd, "link") == 0) {
            scanf("%s %s", arg1, arg2);
            link(arg1, arg2);
        } else if (strcmp(cmd, "copy") == 0) {
            scanf("%s %s", arg1, arg2);
            copy_file_command(arg1, arg2);
        } else if (strcmp(cmd, "move") == 0) {
            scanf("%s %s", arg1, arg2);
            move_file_command(arg1, arg2);
        } else if (strcmp(cmd, "rename") == 0) {
            scanf("%s %s", arg1, arg2);
            rename_path_command(arg1, arg2);
        } else if (strcmp(cmd, "symlink") == 0) {
            scanf("%s %s", arg1, arg2);
            symlink(arg1, arg2);
        } else if (strcmp(cmd, "readlink") == 0) {
            scanf("%s", arg1);
            char buf[DIRSIZ + 1];
            readlink(arg1, buf, DIRSIZ);
            printf("readlink: %s\n", buf);
        } else if (strcmp(cmd, "unlink") == 0) {
            scanf("%s", arg1);
            fs_unlink(arg1);
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
            printf("  grant <path> <user> <writable> - Root grants file access to a user\n");
            printf("  chdir <name> - Change current directory\n");
            printf("  dir - List directory contents\n");
            printf("  blocks - Show block usage status\n");
            printf("\n=== 链接功能 ===\n");
            printf("  link <oldpath> <newpath> - Create hard link\n");
            printf("  copy <oldpath> <newpath> - Copy file content into a new file\n");
            printf("  move <oldpath> <newpath> - Move file into a new path\n");
            printf("  rename <oldpath> <newpath> - Rename a file or directory within the same parent\n");
            printf("  symlink <oldpath> <newpath> - Create symbolic link\n");
            printf("  readlink <path> - Read symbolic link\n");
            printf("  unlink <path> - Remove link or file\n");
            printf("\n=== 创新功能 ===\n");
            printf("  [KFS 智能文件系统]\n");
            printf("    init_kfs - Initialize KFS system\n");
            printf("    kfs_list <vdir> - List virtual directory\n");
            printf("    kfs_tags <ino> - Show file tags\n");
            printf("    kfs_save - Save KFS data to disk\n");
            printf("    kfs_load - Load KFS data from disk\n");
            printf("    kfs_ai_select - AI select hot files\n");
            printf("    kfs_memory_map - Show KFS memory content\n");
            printf("    hot_cache - Show hot file cache\n");
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
        } else {
            printf("Unknown command. Type 'help' for available commands.\n");
        }
        save_vdisk();
    }
    return 0;
}
