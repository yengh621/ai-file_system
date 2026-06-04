# 类UNIX V6虚拟文件系统（含AI增强）设计文档
这份代码实现了经典完整的类UNIX V6虚拟文件系统，并扩展AI智能增强能力。系统不直接操作真实硬盘，而是将宿主机文件`vdisk.bin`作为虚拟磁盘，完成格式化、读写与空间分配。本文档说明系统架构、头文件定义与所有函数功能。

---

## 一、系统整体架构分层
系统自下而上分为6层，在经典文件系统基础上新增智能增强层：
1. **硬件模拟层（disk.c）**：将本地`vdisk.bin`模拟为磁盘，提供512字节块级读写底层接口。
2. **物理存储层（block.c）**：管理磁盘空间分配与回收，维护空闲块与文件逻辑-物理块映射。
3. **Inode元数据层（inode.c）**：管理文件属性（权限、大小、存储位置），通过内存哈希表缓存加速访问。
4. **逻辑文件系统层**：directory.c负责目录树与路径解析；file.c提供文件读写、开关接口；permission.c实现权限校验与锁管理；process_lock.c支持跨进程锁。
5. **用户与接口层（user.c、main.c）**：处理系统初始化、用户登录、会话管理与终端命令交互。
6. **智能增强层（ai.c）**：集成多智能体，提供KFS热点缓存、AI自适应I/O、行为异常检测功能。

---

## 二、虚拟磁盘布局
执行`format`命令后，`vdisk.bin`分区规则（定义于`filesystem.h`）：
- Block 0：引导块，系统保留未使用
- Block 1：超级块，存储文件系统全局状态
- Block 2~33：Inode数组区，共32块，可存储512个文件/目录
- Block 34~65：KFS智能分区，32块，用于热点文件缓存与AI数据存储
- Block 66~末尾：数据块区，用于存储文件内容与目录表，共512块

---

## 三、函数功能详解
### 1. disk.c（底层驱动模拟）
`bread(int blkno, unsigned char *buf)`：块读函数，根据块号计算偏移量，从虚拟磁盘读取一个块数据到缓冲区。
`bwrite(int blkno, unsigned char *buf)`：块写函数，将缓冲区数据写入虚拟磁盘指定块位置。

### 2. block.c（空间分配与地址映射）
`ialloc(void)`：分配空闲Inode，优先使用超级块缓存，缓存耗尽后扫描磁盘获取。
`ifree(int ino)`：释放指定Inode，加入超级块空闲缓存，并标记磁盘Inode为空闲。
`balloc(void)`：分配空闲数据块，采用成组链接法，优先使用超级块缓存。
`bfree(int blkno)`：释放数据块，加入超级块缓存，缓存满时批量写入磁盘。
`bmap(struct inode *ip, int lbn)`：逻辑块转物理块，支持三级索引：前6块直接索引，第7块一级间接索引，第8块二级间接索引。

### 3. inode.c（内存Inode缓存池）
`iget_inode(int ino, struct dinode *di)`：从磁盘读取指定Inode数据到内存结构体。
`iput_inode(int ino, struct dinode *di)`：将内存Inode数据写回磁盘对应位置。
`iget(int ino)`：获取内存Inode，缓存命中则增加引用计数，未命中则从磁盘加载并加入缓存。
`iput(struct inode *p)`：释放Inode引用，引用计数归零时，将数据刷回磁盘并释放缓存。

### 4. directory.c（目录树解析）
`namei(char *path)`：路径解析核心函数，解析绝对/相对路径，支持符号链接递归解析，返回目标文件Inode。
`dir(void)`：实现`ls`命令，遍历目录项，打印文件名、类型、Inode号与链接信息。

### 5. file.c（文件操作接口层）
`create(char *name)`：创建新文件，分配Inode并在父目录添加目录项。
`delete(char *name)`：删除文件，校验权限后移除目录项，释放数据块与Inode。
`open(char *name, int mode)`：打开文件，校验权限、加锁，分配文件描述符。
`close(int fd)`：关闭文件，释放描述符、解锁并释放Inode引用。
`read(...)`：读取文件，优先命中KFS与预取缓存，未命中则读取磁盘并记录IO。
`write(...)`：写入文件，自动分配块并更新文件大小与IO记录。
`seek_file(...)`：修改读写偏移量，支持随机访问。
`copy_file_command`：实现文件复制；`move_file_command`实现文件移动；`rename_path_command`实现文件重命名。

### 6. permission.c（权限与锁管理）
`init_file_locks(void)`：初始化文件锁表，重置所有锁状态。
`lock_file(...)`：为文件加读写锁，读共享、写排他，兼容跨进程同步。
`unlock_file(...)`：释放文件锁，恢复资源访问。
`chmod(...)`：修改文件权限，仅所有者与root可操作。
`check_permission(...)`：按属主、组、其他用户校验9位权限。
`grant(...)`：为指定用户授权文件访问权限。
`is_directory`与`is_empty_directory`：判断是否为目录、目录是否为空。

### 7. process_lock.c（跨进程锁管理）
`init_process_lock_manager`：初始化进程锁，创建`vdisk.lock`文件。
`close_process_lock_manager`：关闭进程锁管理器。
`process_lock_inode`：对Inode加跨进程锁，兼容Windows与Linux。
`process_unlock_inode`：释放Inode跨进程锁。

### 8. user.c（用户与系统管理）
`find_user_index_by_name`：按用户名查找用户索引；`get_current_user_gid`获取当前用户组ID。
`format(void)`：格式化虚拟磁盘，初始化超级块与系统目录。
`load_vdisk`：加载虚拟磁盘，不存在则自动格式化。
`save_vdisk`：将内存超级块数据写回磁盘。
`login`：用户登录验证，切换家目录并启动AI系统。
`logout`：用户登出，关闭文件与AI服务。
`mkdir`创建目录、`rmdir`删除空目录、`chdir`切换工作目录。

### 9. ai.c（AI多智能体集成）
`init_integration`：初始化AI多智能体系统。
`integration_set_user`：绑定用户，启动后台行为分析进程。
`integration_clear_user`：停止AI服务，清理用户数据。
`integration_record_operation`：记录用户操作用于AI分析。
`integration_apply_optimization`：应用AI生成的IO与安全优化配置。
`integration_show_suggestions`：展示AI优化建议。
`integration_start/end_session`：启动/结束AI优化会话，持久化KFS数据。

### 10. main.c（命令行入口）
`print_block_status`：打印磁盘块与Inode使用状态。
`main`：系统主函数，初始化环境、加载磁盘，循环解析执行终端命令。

---

## 四、智能增强功能函数
### KFS智能文件缓存
核心函数实现KFS分区管理：初始化、热点文件查找/更新/存储/删除、热度计算、缓存持久化、虚拟目录展示，将高频文件存入专属分区加速访问。

### AI自适应I/O优化
函数实现负载分析器初始化、IO请求记录、预取窗口动态配置、缓存命中查询，AI根据访问模式自动调整预取策略，提升读取效率。

### 安全行为异常检测
函数实现用户行为记录、异常操作检测拦截、安全阈值配置、行为画像与日志展示，防范批量删除、异常修改等风险操作。

### 链接系统
`link`创建硬链接、`symlink`创建软链接、`readlink`读取链接目标、`fs_unlink`删除链接、`resolve_link`递归解析链接，完整兼容UNIX链接机制。

---

## 五、核心工作流
### 经典文件读取流程
1. 终端命令解析后调用`read`函数
2. 通过文件描述符获取Inode与偏移量
3. 计算逻辑块号，优先查询KFS与预取缓存
4. 缓存未命中则通过`bmap`获取物理块，调用`bread`读磁盘
5. 拷贝数据至用户缓冲区，更新偏移量与IO记录

### 智能增强流程
系统自动记录IO与用户行为、更新文件热度评分、顺序读时预取后续块、构建行为画像用于异常检测，全程无感知优化性能与安全。

---

## 六、代码阅读建议
1. 先阅读`filesystem.h`，掌握宏定义与核心结构体
2. 学习`disk.c`理解磁盘模拟原理
3. 通过`user.c`的`format`理解磁盘初始化
4. 重点研究`directory.c`的`namei`路径解析
5. 攻克`block.c`的空间分配与索引机制
6. 通读`file.c`串联文件操作全流程
7. 最后学习`ai.c`理解AI增强功能