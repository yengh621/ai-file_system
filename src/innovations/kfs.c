#include "filesystem.h"

static struct kfs_tag kfs_tags[FILEBLK];
static int kfs_initialized = 0;

static const char* get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    return dot ? dot + 1 : "";
}

static KFSType determine_file_type(const char* filename) {
    const char* ext = get_extension(filename);
    
    if (strcmp(ext, "c") == 0 || strcmp(ext, "h") == 0 ||
        strcmp(ext, "py") == 0 || strcmp(ext, "java") == 0 ||
        strcmp(ext, "cpp") == 0 || strcmp(ext, "js") == 0) {
        return KFS_TYPE_CODE;
    }
    
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "doc") == 0 ||
        strcmp(ext, "pdf") == 0 || strcmp(ext, "md") == 0) {
        return KFS_TYPE_DOC;
    }
    
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0 ||
        strcmp(ext, "gif") == 0 || strcmp(ext, "bmp") == 0) {
        return KFS_TYPE_IMAGE;
    }
    
    if (strcmp(ext, "dat") == 0 || strcmp(ext, "bin") == 0 ||
        strcmp(ext, "csv") == 0 || strcmp(ext, "json") == 0) {
        return KFS_TYPE_DATA;
    }
    
    return KFS_TYPE_OTHER;
}

static KFSTime determine_time_cat(unsigned long created) {
    time_t now = time(NULL);
    unsigned long diff = (unsigned long)now - created;
    
    if (diff < 86400) return KFS_TIME_TODAY;
    if (diff < 604800) return KFS_TIME_WEEK;
    if (diff < 2592000) return KFS_TIME_MONTH;
    return KFS_TIME_OLD;
}

void init_kfs() {
    if (kfs_initialized) {
        printf("KFS already initialized.\n");
        return;
    }
    
    memset(kfs_tags, 0, sizeof(kfs_tags));
    printf("=== KFS 智能文件系统初始化完成 ===\n");
    printf("虚拟目录结构:\n");
    printf("  /kfs/type/    - 按类型分类\n");
    printf("  /kfs/time/    - 按时间分类\n");
    printf("  /kfs/project/ - 按项目分类\n");
    kfs_initialized = 1;
}

void kfs_classify_file(char *filename, unsigned short ino) {
    if (!kfs_initialized) init_kfs();
    
    struct kfs_tag* tag = &kfs_tags[ino % FILEBLK];
    tag->ino = ino;
    tag->type = determine_file_type(filename);
    tag->created = (unsigned long)time(NULL);
    tag->last_access = tag->created;
    tag->time_cat = determine_time_cat(tag->created);
    tag->tag_count = 0;
    
    const char* type_names[] = {"code", "document", "image", "data", "other"};
    strncpy(tag->tags[tag->tag_count++], type_names[tag->type], 31);
    
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

void kfs_list_virtual_dir(char *vdir_path) {
    if (!kfs_initialized) {
        printf("KFS not initialized. Run 'init_kfs' first.\n");
        return;
    }
    
    printf("=== 虚拟目录: %s ===\n", vdir_path);
    
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
            for (int t = 0; t < kfs_tags[i].tag_count; t++) {
                if (strstr(vdir_path, kfs_tags[i].tags[t])) {
                    match = 1;
                    break;
                }
            }
        }
        
        if (match) {
            printf("  [ino:%d]\n", kfs_tags[i].ino);
            found++;
        }
    }
    
    if (found == 0) {
        printf("  (空目录)\n");
    }
}

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
