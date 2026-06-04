## 线程安全增量任务文档

### 1. 目标
为当前单线程模拟的文件系统增加多线程安全支持，保证在多用户并发执行命令时，全局共享数据结构（`block_buf`、inode 缓存、文件锁表、超级块等）不被竞争条件破坏。

### 2. 实现方案：粗粒度全局互斥锁
- 在**所有对外命令函数**的入口加锁，出口解锁。
- 每个命令执行期间持有锁，保证同一时刻只有一个线程在执行文件系统操作。
- 底层函数（`bread`、`bwrite`、`iget`、`iput` 等）**不再单独加锁**，它们被命令函数的大锁保护，无需考虑重入，使用普通 `pthread_mutex_t` 即可。
- 锁的初始化使用静态初始化器 `PTHREAD_MUTEX_INITIALIZER`，无需显式销毁。

### 3. 需要修改的函数清单
所有在 `filesystem.h` 中声明、且会修改或读取全局状态的命令函数都必须加锁，具体包括：

| 函数 | 说明 |
|------|------|
| `create` | 创建文件 |
| `delete` | 删除文件 |
| `open`   | 打开文件 |
| `close`  | 关闭文件 |
| `read`   | 读文件 |
| `write`  | 写文件 |
| `mkdir`  | 创建目录 |
| `rmdir`  | 删除目录 |
| `chdir`  | 切换目录 |
| `dir`    | 列出目录内容 |
| `chmod`  | 修改权限 |
| `login`  | 用户登录 |
| `logout` | 用户登出 |
| `format` | 格式化磁盘 |
| `load_vdisk` | 加载虚拟磁盘 |
| `save_vdisk` | 保存虚拟磁盘 |
| `kfs_classify_file` 等 KFS 函数 | 若被其他线程直接调用也需加锁，否则在 `create` 内部已被保护 |

此外，所有 KFS 或 AI 相关的暴露接口（如 `show_io_stats` 等）若访问全局数据，也应按同样方式加锁。

### 4. 具体代码修改示例
在 `filesystem.c` 文件顶部添加全局锁定义：
```c
#include <pthread.h>
static pthread_mutex_t fs_mutex = PTHREAD_MUTEX_INITIALIZER;
```

每个命令函数按以下模板修改：
```c
void create(char *name) {
    pthread_mutex_lock(&fs_mutex);          // 入口加锁

    if (cur_uid == -1) {
        printf("Not logged in.\n");
        pthread_mutex_unlock(&fs_mutex);    // 提前返回前解锁
        return;
    }

    // ... 原有全部业务逻辑 ...

    pthread_mutex_unlock(&fs_mutex);        // 正常出口解锁
}
```

**注意**：函数内所有的 `return` 语句前都必须调用 `pthread_mutex_unlock`。建议使用 `goto` 统一清理，或确保每个 `return` 前都有解锁（如早期错误检查）。

### 5. CMakeLists.txt 修改
如果项目使用 CMake 构建，需要链接 pthread 库。在 `CMakeLists.txt` 中添加：
```cmake
find_package(Threads REQUIRED)
target_link_libraries(your_target_name PRIVATE Threads::Threads)
```
其中 `your_target_name` 替换为你的可执行文件或库的目标名称。

若为简单 Makefile 编译，只需在链接选项中加入 `-lpthread`。

### 6. 注意事项
- 锁的粒度：当前为全局大锁，串行化所有文件系统操作，性能较低，但足以满足课设演示要求，且实现简单。
- 递归锁：由于不在底层函数加锁，不会出现同一线程重入加锁的情况，因此**不需要递归锁**，普通互斥锁即可。
- 初始化：`PTHREAD_MUTEX_INITIALIZER` 静态初始化无需销毁，程序退出时自动回收。若使用动态初始化，需在程序结束时调用 `pthread_mutex_destroy`。
- 死锁预防：命令函数内禁止再调用其他命令函数（正常逻辑不会发生），因此不会产生锁顺序问题。
- 编译测试：添加 `-lpthread` 后重新编译，使用多线程测试程序验证无数据竞争（可通过简单脚本并发执行创建、读写命令，检查文件系统一致性）。

### 7. 扩展（可选）
若未来需要提高并发性，可逐步将全局锁拆分为更细粒度的锁（如 inode 缓存锁、块缓冲锁），但需注意锁的获取顺序，防止死锁。当前版本优先保证正确性。
