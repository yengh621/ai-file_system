#include "filesystem.h"  // 包含文件系统接口定义（inode、目录项、磁盘操作、权限检查等）

/*
 * 将路径拆分为父目录部分和文件名部分
 * path: 输入路径
 * parent: 输出父目录路径缓冲区
 * parent_size: 父目录缓冲区大小
 * name: 输出文件名缓冲区
 * name_size: 文件名缓冲区大小
 * 返回 1 表示成功，0 表示失败
 */
static int split_parent_and_name(const char *path, char *parent, size_t parent_size, char *name, size_t name_size) {
    char normalized[512];          // 用于存放去除末尾斜杠后的路径副本
    const char *last_slash;        // 指向最后一个 '/' 的指针
    size_t len;

    if (path == NULL || path[0] == '\0') {  // 无效路径或空字符串
        return 0;
    }

    strncpy(normalized, path, sizeof(normalized) - 1);  // 复制路径到局部数组，避免修改原始参数
    normalized[sizeof(normalized) - 1] = '\0';          // 确保字符串终止

    len = strlen(normalized);
    while (len > 1 && normalized[len - 1] == '/') {     // 去除末尾多余的 '/'
        normalized[len - 1] = '\0';
        len--;
    }

    last_slash = strrchr(normalized, '/');              // 查找最后一个斜杠
    if (last_slash == NULL) {                           // 没有斜杠，整个路径就是文件名
        strncpy(parent, ".", parent_size - 1);          // 父目录设为当前目录 "."
        parent[parent_size - 1] = '\0';
        strncpy(name, normalized, name_size - 1);       // 复制整个路径作为文件名
        name[name_size - 1] = '\0';
        return name[0] != '\0';                         // 文件名非空则成功
    }

    // 有斜杠的情况
    if (last_slash == normalized) {                     // 路径就是 "/" 或 "/xxx"
        strncpy(parent, "/", parent_size - 1);          // 父目录为根目录 "/"
        parent[parent_size - 1] = '\0';
    } else {
        size_t parent_len = (size_t)(last_slash - normalized);  // 计算父目录部分的长度
        if (parent_len >= parent_size) {
            parent_len = parent_size - 1;               // 防止溢出
        }
        memcpy(parent, normalized, parent_len);         // 复制父目录部分
        parent[parent_len] = '\0';
    }

    strncpy(name, last_slash + 1, name_size - 1);       // 斜杠后面的部分为文件名
    name[name_size - 1] = '\0';
    return name[0] != '\0';                             // 文件名非空则成功
}

/*
 * 切换到指定目录（修改全局 cur_dir）
 * path: 目录路径
 * op_name: 操作名称（用于错误提示）
 * 返回 1 表示成功，0 表示失败
 */
static int switch_to_directory(const char *path, const char *op_name) {
    struct inode *ip;

    if (strcmp(path, ".") == 0) {   // 当前目录无需切换
        return 1;
    }

    ip = namei((char *)path);       // 通过路径名查找 inode
    if (ip == NULL) {
        printf("%s: directory %s not found\n", op_name, path);
        return 0;
    }
    if (!is_directory(ip)) {        // 不是目录则报错
        printf("%s: %s is not a directory\n", op_name, path);
        iput(ip);                   // 释放 inode
        return 0;
    }

    cur_dir = ip->i_ino;            // 将当前目录设为该 inode 号
    iput(ip);                       // 释放 inode（iget 增加引用，这里不需要再持有）
    return 1;
}

/*
 * 若目标文件存在则尝试删除（用于失败回滚）
 * target_name: 目标文件名（相对当前目录）
 * 返回 0（辅助函数，返回值未使用）
 */
static int remove_partial_target(const char *target_name) {
    struct inode *target_inode;

    target_inode = namei((char *)target_name);  // 查找目标文件
    if (target_inode == NULL) {
        return 0;                               // 不存在则无事可做
    }
    iput(target_inode);                         // 释放查找时获得的引用
    delete((char *)target_name);                // 删除文件（使用已有的 delete 函数）
    return 0;
}

/*
 * 重命名文件命令（仅支持同一目录内重命名）
 * source_path: 源路径
 * target_path: 目标路径
 * 返回 0 成功，-1 失败
 */
int rename_path_command(char *source_path, char *target_path) {
    unsigned short saved_dir = cur_dir;      // 保存当前目录以便恢复
    char source_parent[512];
    char source_name[DIRSIZ];
    char target_parent[512];
    char target_name[DIRSIZ];
    struct inode *parent_inode = NULL;
    struct inode *target_inode = NULL;
    struct direct dir;
    int result = -1;                         // 默认失败

    // 拆分路径为父目录和文件名，任一失败则报错
    if (!split_parent_and_name(source_path, source_parent, sizeof(source_parent), source_name, sizeof(source_name)) ||
        !split_parent_and_name(target_path, target_parent, sizeof(target_parent), target_name, sizeof(target_name))) {
        printf("rename: invalid path\n");
        return -1;
    }

    // 当前实现仅支持同一目录内重命名
    if (strcmp(source_parent, target_parent) != 0) {
        printf("rename: moving between directories is not supported\n");
        return -1;
    }

    // 禁止重命名特殊目录项 "." 和 ".."
    if (strcmp(source_name, ".") == 0 || strcmp(source_name, "..") == 0 ||
        strcmp(target_name, ".") == 0 || strcmp(target_name, "..") == 0) {
        printf("rename: invalid name\n");
        return -1;
    }

    // 目标文件名不能超过目录项的最大长度（DIRSIZ）
    if (strlen(target_name) >= DIRSIZ) {
        printf("rename: name too long\n");
        return -1;
    }

    // 源和目标名称相同则无需操作（但按设计返回失败）
    if (strcmp(source_path, target_path) == 0 || strcmp(source_name, target_name) == 0) {
        printf("rename: source and target are the same\n");
        return -1;
    }

    // 检查目标文件是否已存在
    target_inode = namei(target_path);
    if (target_inode != NULL) {
        iput(target_inode);
        printf("rename: %s already exists\n", target_path);
        return -1;
    }

    // 获取源文件所在父目录的 inode
    parent_inode = namei(source_parent);
    if (parent_inode == NULL) {
        printf("rename: directory %s not found\n", source_parent);
        return -1;
    }
    if (!is_directory(parent_inode)) {
        printf("rename: %s is not a directory\n", source_parent);
        goto cleanup;
    }
    // 检查对父目录的写和执行权限
    if (check_permission(parent_inode, W_OK | X_OK) != 0) {
        printf("Permission denied.\n");
        goto cleanup;
    }

    // 遍历父目录的所有目录项，寻找源文件名
    for (int i = 0; i < (parent_inode->i_din.di_size + 15) / 16; i++) {
        int bn = bmap(parent_inode, i / 32);       // 获取目录项所在块号
        if (bn == 0) {
            break;                                 // 超出文件范围
        }

        bread(bn, block_buf);                      // 读入该块
        memcpy(&dir, block_buf + (i % 32) * 16, 16); // 拷贝出第 i 个目录项
        if (dir.d_ino != 0 && strcmp(dir.d_name, source_name) == 0) {  // 找到匹配项
            struct direct *dirp = (struct direct *)(block_buf + (i % 32) * 16);
            memset(dirp->d_name, 0, DIRSIZ);       // 清空原文件名
            strncpy(dirp->d_name, target_name, DIRSIZ - 1); // 写入新文件名
            bwrite(bn, block_buf);                 // 写回磁盘块
            kfs_rename_file(dir.d_ino, target_name);
            printf("Rename successful: %s -> %s\n", source_path, target_path);
            result = 0;
            break;
        }
    }

    if (result != 0) {
        printf("rename: %s not found\n", source_path);
    }

cleanup:
    if (parent_inode != NULL) {
        iput(parent_inode);    // 释放父目录 inode
    }
    cur_dir = saved_dir;       // 恢复当前目录
    return result;
}

/*
 * 传输文件（复制或移动）
 * source_path: 源路径
 * target_path: 目标路径
 * remove_source: 1 表示移动（删除源文件），0 表示复制
 * 返回 0 成功，-1 失败
 */
static int transfer_file(const char *source_path, const char *target_path, int remove_source) {
    unsigned short saved_dir = cur_dir;          // 保存当前目录
    char source_parent[512];
    char source_name[DIRSIZ];
    char target_parent[512];
    char target_name[DIRSIZ];
    int source_fd = -1;
    int target_fd = -1;
    int result = -1;
    const char *op_name = remove_source ? "move" : "copy";  // 操作名称用于错误提示
    struct inode *source_inode;
    struct inode *target_inode;

    // 路径相同则报错
    if (strcmp(source_path, target_path) == 0) {
        printf("%s: source and target are the same\n", op_name);
        return -1;
    }

    // 拆分路径
    if (!split_parent_and_name(source_path, source_parent, sizeof(source_parent), source_name, sizeof(source_name)) ||
        !split_parent_and_name(target_path, target_parent, sizeof(target_parent), target_name, sizeof(target_name))) {
        printf("%s: invalid path\n", op_name);
        return -1;
    }

    // 检查源文件存在且不是目录
    source_inode = namei((char *)source_path);
    if (source_inode == NULL) {
        printf("%s: %s not found\n", op_name, source_path);
        return -1;
    }
    if (is_directory(source_inode)) {
        printf("%s: directories are not supported\n", op_name);
        iput(source_inode);
        return -1;
    }
    iput(source_inode);  // 仅用于检查，释放引用

    // 检查目标文件不存在
    target_inode = namei((char *)target_path);
    if (target_inode != NULL) {
        printf("%s: %s already exists\n", op_name, target_path);
        iput(target_inode);
        return -1;
    }

    // 切换到源父目录，打开源文件
    if (!switch_to_directory(source_parent, op_name)) {
        goto cleanup;
    }
    source_fd = open(source_name, O_RDONLY);  // 只读打开
    if (source_fd < 0) {
        goto cleanup;
    }

    // 切换到目标父目录，创建并打开目标文件
    if (!switch_to_directory(target_parent, op_name)) {
        goto cleanup;
    }
    create(target_name);                     // 创建目标文件
    target_fd = open(target_name, O_WRONLY); // 只写打开
    if (target_fd < 0) {
        goto cleanup;
    }

    // 循环读取源文件并写入目标文件
    while (1) {
        unsigned char buf[BLOCKSIZ];         // 临时缓冲区（块大小）
        int read_count = read(source_fd, buf, BLOCKSIZ);  // 读一块数据

        if (read_count < 0) {                // 读出错
            goto cleanup;
        }
        if (read_count == 0) {               // 读到文件尾，结束循环
            break;
        }
        if (write(target_fd, buf, read_count) != read_count) {  // 写入，必须完整
            printf("%s: failed while writing %s\n", op_name, target_path);
            goto cleanup;
        }
    }

    // 关闭文件描述符
    close(source_fd);
    source_fd = -1;
    close(target_fd);
    target_fd = -1;

    // 如果是移动操作，删除源文件
    if (remove_source) {
        struct inode *remaining_source;

        if (!switch_to_directory(source_parent, op_name)) {
            goto cleanup;
        }
        delete(source_name);                 // 删除源文件
        remaining_source = namei(source_name); // 验证是否删除成功
        if (remaining_source != NULL) {
            iput(remaining_source);
            printf("move: failed to remove %s\n", source_path);
            goto cleanup;
        }
        printf("Move successful: %s -> %s\n", source_path, target_path);
    } else {
        printf("Copy successful: %s -> %s\n", source_path, target_path);
    }

    result = 0;  // 成功

cleanup:
    // 关闭可能仍打开的文件描述符
    if (source_fd >= 0) {
        close(source_fd);
    }
    if (target_fd >= 0) {
        close(target_fd);
    }
    // 如果操作失败，尝试删除已部分创建的目标文件
    if (result != 0 && switch_to_directory(target_parent, op_name)) {
        remove_partial_target(target_name);
    }
    cur_dir = saved_dir;  // 恢复当前目录
    return result;
}

/*
 * 复制文件命令（包装 transfer_file，remove_source = 0）
 */
int copy_file_command(char *source_path, char *target_path) {
    return transfer_file(source_path, target_path, 0);
}

/*
 * 移动文件命令（包装 transfer_file，remove_source = 1）
 */
int move_file_command(char *source_path, char *target_path) {
    return transfer_file(source_path, target_path, 1);
}
