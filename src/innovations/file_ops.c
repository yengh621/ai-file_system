#include "filesystem.h"

static int split_parent_and_name(const char *path, char *parent, size_t parent_size, char *name, size_t name_size) {
    char normalized[512];
    const char *last_slash;
    size_t len;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    strncpy(normalized, path, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';

    len = strlen(normalized);
    while (len > 1 && normalized[len - 1] == '/') {
        normalized[len - 1] = '\0';
        len--;
    }

    last_slash = strrchr(normalized, '/');
    if (last_slash == NULL) {
        strncpy(parent, ".", parent_size - 1);
        parent[parent_size - 1] = '\0';
        strncpy(name, normalized, name_size - 1);
        name[name_size - 1] = '\0';
        return name[0] != '\0';
    }

    if (last_slash == normalized) {
        strncpy(parent, "/", parent_size - 1);
        parent[parent_size - 1] = '\0';
    } else {
        size_t parent_len = (size_t)(last_slash - normalized);
        if (parent_len >= parent_size) {
            parent_len = parent_size - 1;
        }
        memcpy(parent, normalized, parent_len);
        parent[parent_len] = '\0';
    }

    strncpy(name, last_slash + 1, name_size - 1);
    name[name_size - 1] = '\0';
    return name[0] != '\0';
}

static int switch_to_directory(const char *path, const char *op_name) {
    struct inode *ip;

    if (strcmp(path, ".") == 0) {
        return 1;
    }

    ip = namei((char *)path);
    if (ip == NULL) {
        printf("%s: directory %s not found\n", op_name, path);
        return 0;
    }
    if (!is_directory(ip)) {
        printf("%s: %s is not a directory\n", op_name, path);
        iput(ip);
        return 0;
    }

    cur_dir = ip->i_ino;
    iput(ip);
    return 1;
}

static int remove_partial_target(const char *target_name) {
    struct inode *target_inode;

    target_inode = namei((char *)target_name);
    if (target_inode == NULL) {
        return 0;
    }
    iput(target_inode);
    delete((char *)target_name);
    return 0;
}

int rename_path_command(char *source_path, char *target_path) {
    unsigned short saved_dir = cur_dir;
    char source_parent[512];
    char source_name[DIRSIZ];
    char target_parent[512];
    char target_name[DIRSIZ];
    struct inode *parent_inode = NULL;
    struct inode *target_inode = NULL;
    struct direct dir;
    int result = -1;

    if (!split_parent_and_name(source_path, source_parent, sizeof(source_parent), source_name, sizeof(source_name)) ||
        !split_parent_and_name(target_path, target_parent, sizeof(target_parent), target_name, sizeof(target_name))) {
        printf("rename: invalid path\n");
        return -1;
    }

    if (strcmp(source_parent, target_parent) != 0) {
        printf("rename: moving between directories is not supported\n");
        return -1;
    }

    if (strcmp(source_name, ".") == 0 || strcmp(source_name, "..") == 0 ||
        strcmp(target_name, ".") == 0 || strcmp(target_name, "..") == 0) {
        printf("rename: invalid name\n");
        return -1;
    }

    if (strlen(target_name) >= DIRSIZ) {
        printf("rename: name too long\n");
        return -1;
    }

    if (strcmp(source_path, target_path) == 0 || strcmp(source_name, target_name) == 0) {
        printf("rename: source and target are the same\n");
        return -1;
    }

    target_inode = namei(target_path);
    if (target_inode != NULL) {
        iput(target_inode);
        printf("rename: %s already exists\n", target_path);
        return -1;
    }

    parent_inode = namei(source_parent);
    if (parent_inode == NULL) {
        printf("rename: directory %s not found\n", source_parent);
        return -1;
    }
    if (!is_directory(parent_inode)) {
        printf("rename: %s is not a directory\n", source_parent);
        goto cleanup;
    }
    if (check_permission(parent_inode, W_OK | X_OK) != 0) {
        printf("Permission denied.\n");
        goto cleanup;
    }

    for (int i = 0; i < (parent_inode->i_din.di_size + 15) / 16; i++) {
        int bn = bmap(parent_inode, i / 32);
        if (bn == 0) {
            break;
        }

        bread(bn, block_buf);
        memcpy(&dir, block_buf + (i % 32) * 16, 16);
        if (dir.d_ino != 0 && strcmp(dir.d_name, source_name) == 0) {
            struct direct *dirp = (struct direct *)(block_buf + (i % 32) * 16);
            memset(dirp->d_name, 0, DIRSIZ);
            strncpy(dirp->d_name, target_name, DIRSIZ - 1);
            bwrite(bn, block_buf);
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
        iput(parent_inode);
    }
    cur_dir = saved_dir;
    return result;
}

static int transfer_file(const char *source_path, const char *target_path, int remove_source) {
    unsigned short saved_dir = cur_dir;
    char source_parent[512];
    char source_name[DIRSIZ];
    char target_parent[512];
    char target_name[DIRSIZ];
    int source_fd = -1;
    int target_fd = -1;
    int result = -1;
    const char *op_name = remove_source ? "move" : "copy";
    struct inode *source_inode;
    struct inode *target_inode;

    if (strcmp(source_path, target_path) == 0) {
        printf("%s: source and target are the same\n", op_name);
        return -1;
    }

    if (!split_parent_and_name(source_path, source_parent, sizeof(source_parent), source_name, sizeof(source_name)) ||
        !split_parent_and_name(target_path, target_parent, sizeof(target_parent), target_name, sizeof(target_name))) {
        printf("%s: invalid path\n", op_name);
        return -1;
    }

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
    iput(source_inode);

    target_inode = namei((char *)target_path);
    if (target_inode != NULL) {
        printf("%s: %s already exists\n", op_name, target_path);
        iput(target_inode);
        return -1;
    }

    if (!switch_to_directory(source_parent, op_name)) {
        goto cleanup;
    }
    source_fd = open(source_name, O_RDONLY);
    if (source_fd < 0) {
        goto cleanup;
    }

    if (!switch_to_directory(target_parent, op_name)) {
        goto cleanup;
    }
    create(target_name);
    target_fd = open(target_name, O_WRONLY);
    if (target_fd < 0) {
        goto cleanup;
    }

    while (1) {
        unsigned char buf[BLOCKSIZ];
        int read_count = read(source_fd, buf, BLOCKSIZ);

        if (read_count < 0) {
            goto cleanup;
        }
        if (read_count == 0) {
            break;
        }
        if (write(target_fd, buf, read_count) != read_count) {
            printf("%s: failed while writing %s\n", op_name, target_path);
            goto cleanup;
        }
    }

    close(source_fd);
    source_fd = -1;
    close(target_fd);
    target_fd = -1;

    if (remove_source) {
        struct inode *remaining_source;

        if (!switch_to_directory(source_parent, op_name)) {
            goto cleanup;
        }
        delete(source_name);
        remaining_source = namei(source_name);
        if (remaining_source != NULL) {
            iput(remaining_source);
            printf("move: failed to remove %s\n", source_path);
            goto cleanup;
        }
        printf("Move successful: %s -> %s\n", source_path, target_path);
    } else {
        printf("Copy successful: %s -> %s\n", source_path, target_path);
    }

    result = 0;

cleanup:
    if (source_fd >= 0) {
        close(source_fd);
    }
    if (target_fd >= 0) {
        close(target_fd);
    }
    if (result != 0 && switch_to_directory(target_parent, op_name)) {
        remove_partial_target(target_name);
    }
    cur_dir = saved_dir;
    return result;
}

int copy_file_command(char *source_path, char *target_path) {
    return transfer_file(source_path, target_path, 0);
}

int move_file_command(char *source_path, char *target_path) {
    return transfer_file(source_path, target_path, 1);
}
