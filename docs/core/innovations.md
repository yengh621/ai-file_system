# 简易类 Unix 文件系统模块说明

## 一、AI 自适应 I/O 优化模块 (Workload Analyzer)

### 功能概述
该模块为每个文件维护独立的 I/O 访问历史，动态分析工作负载类型（顺序、随机），自适应调整预取窗口，并实现基于预测的预取缓存，以加速后续文件读写。同时支持从外部 AI 学习参数加载优化策略，并导出统计数据供 AI 进一步分析，形成“分析 → 优化 → 反馈”的闭环。

### 关键函数说明

| 函数 | 功能 |
|------|------|
| `init_workload_analyzer()` | 初始化分析器，清空历史，从磁盘 JSON 恢复上次统计数据 |
| `record_io_request(ino, block_no, is_read)` | 记录一次块访问，分析该文件访问模式，若为顺序则触发预取 |
| `get_prefetched_block(ino, block_no, buf)` | 查询预取缓存，命中则直接复制数据并返回 1，否则返回 0 |
| `export_io_stats_to_ai()` | 将当前各文件负载类型、预取窗口、缓存命中率等导出为 JSON，供外部 AI 使用 |
| `load_io_stats_from_disk()` | 从 JSON 文件恢复之前的统计状态，保持跨会话连续性 |
| `set_prefetch_window(int window)` | 手动统一设置所有文件的预取窗口大小 |
| `show_io_stats()` | 打印每个文件的 I/O 类型、预取窗口、访问次数等统计信息 |
| `load_file_prefetch_window_from_ai(ino)` | 从 AI 学习参数文件中读取某个文件的最佳预取窗口 |

### 工作流程
1. **初始化**：`init_workload_analyzer` 清空内存结构，调用 `load_io_stats_from_disk` 尝试从 `debug_memory/io_stats.json`（用户级）恢复历史文件列表及其负载类型、预取窗口。
2. **运行时记录**：每次文件 `read` 或 `write` 操作最终会调用 `record_io_request`，传入 inode 和本次访问的逻辑块号。
   - 将该块号写入该文件的历史环形缓冲区，更新最后访问块和总读取次数。
   - 调用 `analyze_file_history` 统计历史中连续块递增（顺序）和跳跃（随机）的次数。
   - 根据顺序/随机比例判定负载类型：若顺序跳跃数 ≥ 随机跳跃数的 2 倍且顺序次数 > 0，标记为 `WORKLOAD_SEQUENTIAL`；反之标记为 `WORKLOAD_RANDOM`；否则为 `UNKNOWN`。
   - 从 AI 参数中获得该文件的最佳预取窗口大小。
   - 若识别为顺序访问且预取窗口 > 0，则通过 `bmap` 将文件后续逻辑块转换为物理块号，提前读入内存中的预取缓存（环形数组 `cache`）。
3. **缓存查询**：上层 `read` 操作正式读盘前，先调用 `get_prefetched_block` 检查所需块是否已在预取缓存中。若命中，直接拷贝数据并返回，避免一次 `bread`；未命中才走正常读盘路径，并计入未命中数。
4. **持久化与AI交互**：`export_io_stats_to_ai` 可随时将当前状态写入 JSON 文件，供外部 AI 分析；AI 更新后的预取参数（`learned_params.json`）可通过 `load_file_prefetch_window_from_ai` 被实时读取，实现策略动态更新。

### 数据结构
- **`struct file_io_history`**：每个文件一份，包含访问过的块号历史（64项）、当前负载类型、预取窗口、最后访问块、总读取次数等。
- **`struct workload_analyzer`**：全局管理结构，包含所有文件的历史数组（最多512项，对应最大inode数）、预取缓存（32项）、命中/未命中统计。
- **预取缓存项**：记录 inode、块号、数据内容、时间戳，用于快速匹配。

### 设计要点
- **按文件独立分析**：不同于传统全局预取策略，该模块为每个文件独立维护历史和窗口，适合多文件混合访问场景。
- **块级预取**：预取时直接用 `bmap` 获得真实物理块号，模拟操作系统预读机制，避免后续 I/O 等待。
- **跨会话记忆**：通过导出/导入 JSON 保留历史负载类型和预取窗口，程序重启后不必重新学习。
- **安全约束**：预取窗口限制在 1～10 块，防止激进预取浪费内存和I/O资源。
- **兼容性**：所有辅助函数（路径构造、JSON解析）均考虑了用户登录状态和跨平台目录创建。

### 与系统其他模块的关系
- 被 `read`/`write` 系统调用调用，记录每次 I/O。
- 预取时依赖 `bmap` 和 `bread` 底层块操作。
- 导出的 JSON 数据可与外部 AI 代理联动（KFS、安全模块等），形成智能文件系统生态。

---

## 二、链接子系统 (Link)

### 2.1 功能概述
实现 Unix 风格的硬链接和符号链接（软链接），以及对应的删除操作。

### 2.2 关键函数
| 函数 | 作用 |
|------|------|
| `link(char *oldpath, char *newpath)` | 创建硬链接 |
| `symlink(char *oldpath, char *newpath)` | 创建符号链接（文件内容为目标路径） |
| `readlink(char *path, char *buf, int bufsize)` | 读取符号链接的目标路径 |
| `fs_unlink(char *path)` | 删除一个目录项，正确处理链接计数 |
| `is_link(struct inode *ip)` | 判断 inode 是否为符号链接 (`S_IFLNK`) |

### 2.3 实现细节
- **硬链接**：在目标目录添加目录项，指向与源文件相同的 inode，`di_nlink` 加 1。删除时仅减少计数，计数归零才真正释放数据和 inode。
- **符号链接**：创建一个普通文件，其内容存储目标路径字符串，然后将 inode 模式设为 `S_IFLNK`。`readlink` 通过 `readlink_inode` 读取文件内容获得路径。
- **删除**：`fs_unlink` 会区分硬链接和符号链接，正确回收间接块（当前版本仅释放直接块，待完善）。
- **路径解析**：`lookup_path_nofollow` 用于 `readlink` 和 `unlink` 时不对符号链接进行跟随，直接操作链接本身。

---

## 三、安全异常检测 (Security)

### 3.1 功能概述
模拟基于用户行为画像的实时入侵检测，监控高频删除/修改操作，可由 AI 动态调整阈值。

### 3.2 关键函数
| 函数 | 作用 |
|------|------|
| `init_security_system()` | 初始化各用户行为画像，从 JSON 加载阈值 |
| `record_user_action(char *action, char *target)` | 将用户的一次操作记入环形历史缓冲区 |
| `detect_anomaly(char *action, char *target)` | 检测 60 秒内操作频率是否超阈值，异常时询问用户 |
| `show_user_profile()` | 显示当前用户的阈值和近期活动统计 |
| `show_security_events()` | 展示安全事件日志 |

### 3.3 工作流程
1. 在 `delete`、`create`、`write` 等操作前调用 `detect_anomaly`。
2. 扫描当前用户最近 60 秒内的行为记录，计算删除和修改次数。
3. 若超过对应阈值，打印警告并等待用户确认，允许继续或取消。
4. 所有操作最终通过 `record_user_action` 写入环形历史（内存，不持久化）。
5. 阈值默认 5（删除）/10（修改）次/分钟，可从 `learned_params.json` 动态读取，也可用命令手动调整。

---

## 四、KFS 智能文件系统 (KFS)

### 4.1 功能概述
在标准磁盘布局上预留一块区域，用于存放热点文件的完整副本，并提供基于访问频率的自动缓存和加速读取。

### 4.2 关键函数
| 函数 | 作用 |
|------|------|
| `init_kfs()` | 从磁盘或 JSON 恢复热点缓存，并从 AI 加载推荐热点 |
| `kfs_hot_cache_lookup(char *filename)` | 查找文件名是否在热点缓存中，更新访问统计 |
| `kfs_hot_cache_update(char *filename, unsigned short ino)` | 将文件加入缓存或更新访问记录 |
| `kfs_store_hot_file(char *filename, unsigned short ino)` | 将文件内容拷贝到 KFS 预留数据区，形成连续副本 |
| `kfs_read_hot_file(char *filename, uchar *buf)` | 从 KFS 连续数据区直接读取文件内容 |
| `export_kfs_stats_to_ai()` | 导出热点文件统计到 JSON 供外部 AI 分析 |
| `kfs_load_hot_files_from_ai()` | 读取 AI 推荐的热点文件并自动存入 KFS |

### 4.3 存储结构
- **磁盘预留区**：`KFS_START` 开始的 32 个块，包含头部、热点文件索引、内存文件映射、热点数据区。
- **内存缓存**：`hot_file_cache[HOT_FILE_CACHE_SIZE]`，每个条目记录文件名、inode、访问次数、长短期评分，以及 KFS 数据区的起始块和块数。
- **快速路径**：热点文件在 KFS 数据区内**物理连续**存放，读取时只需 `bread(start_blk + i, buf)`，跳过 inode → bmap 的映射过程。

### 4.4 智能特性
- **长短期评分**：根据首次访问时间和最近访问时间计算短期热度与长期重要性，为淘汰提供依据。
- **与 AI 联动**：可定期从 AI 的 `learned_params.json` 获取推荐热点文件列表，自动完成存储。
- **持久化**：支持将缓存状态保存到磁盘和 JSON，程序重启后可恢复。

---

## 五、文件操作层 (FileOp)

### 5.1 功能概述
基于 inode 缓存、块管理、权限和锁等底层支持，实现完整的文件和目录操作命令。

### 5.2 核心函数
| 函数 | 功能 |
|------|------|
| `namei(char *path)` | 路径解析，从根/当前目录逐级查找，返回目标 inode |
| `create(char *name)` | 创建普通文件，在父目录添加目录项，分配 inode |
| `delete(char *name)` | 删除普通文件，清除目录项，回收资源 |
| `open(char *name, int mode)` | 打开文件，返回 fd，检查权限并加锁 |
| `close(int fd)` | 关闭文件描述符，解锁并减少引用计数 |
| `read(int fd, uchar *buf, int count)` | 从文件偏移处读取数据，支持跨块 |
| `write(int fd, uchar *buf, int count)` | 向文件写入数据，自动扩展文件大小 |
| `mkdir(char *name)` | 创建目录，写入 `.` 和 `..` 项 |
| `rmdir(char *name)` | 删除空目录 |
| `chdir(char *name)` | 改变当前工作目录 |
| `chmod(char *name, ushort mode)` | 修改文件权限 |
| `dir()` | 列出当前目录内容 |
| `rename_path_command(...)` | 文件重命名（同目录） |
| `copy_file_command(...)` | 复制文件内容 |
| `move_file_command(...)` | 移动文件（复制后删除源文件） |

### 5.3 依赖机制
- **Inode 缓存**：`iget`/`iput` 通过哈希桶管理内存 inode，引用计数控制写回。
- **权限检查**：`check_permission` 根据 `cur_uid` 与 inode 的 uid/gid 及模式位验证访问。
- **文件锁**：`lock_file`/`unlock_file` 实现简单的读写锁，防止多用户冲突。
- **全局缓冲区**：所有目录和文件数据均通过 `block_buf` 中转，各操作原子执行（计划加锁）。

### 5.4 数据流示例
```
用户命令 → create/open/read…
    ↓
namei 解析路径 → iget/iput (inode 缓存)
    ↓
bmap (逻辑块→物理块) → bread/bwrite (磁盘 I/O)
    ↓
vdisk.bin
```
操作过程中，权限、锁、安全检测和 KFS 热点加速在对应环节并行介入。

---

以上五个模块共同构建了一个用户态、多用户、具有智能特性的文件系统，涵盖了存储管理、命名空间、并发控制、行为监控和性能优化等操作系统核心概念。