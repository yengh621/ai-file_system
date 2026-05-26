#include "filesystem.h"

static struct workload_analyzer workload;
static int workload_initialized = 0;

void set_prefetch_window(int window) {
    if (!workload_initialized) init_workload_analyzer();
    workload.prefetch_window = window;
    printf("I/O 预取窗口已设置为: %d\n", window);
}

void init_workload_analyzer() {
    if (workload_initialized) {
        printf("Workload analyzer already initialized.\n");
        return;
    }
    
    memset(&workload, 0, sizeof(workload));
    workload.prefetch_window = 3;
    workload.current_type = WORKLOAD_UNKNOWN;
    
    printf("=== AI 自适应 I/O 优化初始化完成 ===\n");
    workload_initialized = 1;
}

void record_io_request(unsigned short ino, int block_no, int is_read) {
    if (!workload_initialized) init_workload_analyzer();
    
    workload.history[workload.history_idx].ino = ino;
    workload.history[workload.history_idx].block_no = block_no;
    workload.history[workload.history_idx].is_read = is_read;
    workload.history[workload.history_idx].timestamp = (unsigned long)time(NULL);
    workload.history_idx = (workload.history_idx + 1) % WORKLOAD_HISTORY;
}

void analyze_workload() {
    if (!workload_initialized) return;
    
    int seq_count = 0;
    int rand_count = 0;
    int total = 0;
    
    for (int i = 1; i < WORKLOAD_HISTORY; i++) {
        int prev = (workload.history_idx - i - 1 + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        int curr = (workload.history_idx - i + WORKLOAD_HISTORY) % WORKLOAD_HISTORY;
        
        if (workload.history[prev].ino == 0 || workload.history[curr].ino == 0) continue;
        
        if (workload.history[prev].ino == workload.history[curr].ino) {
            int diff = workload.history[curr].block_no - workload.history[prev].block_no;
            if (diff == 1) {
                seq_count++;
            } else if (abs(diff) > 1) {
                rand_count++;
            }
        }
        total++;
    }
    
    if (total < 10) {
        workload.current_type = WORKLOAD_UNKNOWN;
        return;
    }
    
    if (seq_count > rand_count * 2) {
        workload.current_type = WORKLOAD_SEQUENTIAL;
        workload.prefetch_window = 10;
    } else if (rand_count > seq_count * 2) {
        workload.current_type = WORKLOAD_RANDOM;
        workload.prefetch_window = 0;
    } else {
        workload.current_type = WORKLOAD_UNKNOWN;
        workload.prefetch_window = 3;
    }
}

int get_prefetch_window() {
    analyze_workload();
    return workload.prefetch_window;
}

void show_io_stats() {
    if (!workload_initialized) {
        printf("Workload analyzer not initialized.\n");
        return;
    }
    
    const char* type_names[] = {"Sequential", "Random", "Stream", "Unknown"};
    
    printf("=== AI I/O 优化统计 ===\n");
    printf("Current Workload: %s\n", type_names[workload.current_type]);
    printf("Prefetch Window: %d blocks\n", workload.prefetch_window);
    printf("Cache Hits: %d\n", workload.cache_hits);
    printf("Cache Misses: %d\n", workload.cache_misses);
    
    if (workload.current_type == WORKLOAD_SEQUENTIAL) {
        printf("✓ 检测到大文件顺序读，已增大预取窗口以提升带宽利用率\n");
    } else if (workload.current_type == WORKLOAD_RANDOM) {
        printf("✓ 检测到小文件随机读，已关闭预取以节省内存\n");
    }
}
