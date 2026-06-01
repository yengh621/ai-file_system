这份代码实现了一个**非常经典且完整的类 UNIX V6 虚拟文件系统**。它并不直接操作真实的硬盘，而是将宿主机上的一个普通文件（`vdisk.bin`）当作一块硬盘来进行格式化、读写和分配。

为了让你在看代码前建立清晰的“全局观”，我为你整理了这份**架构设计与代码导读文档**。

---

# 类 UNIX 虚拟文件系统架构导读

## 1. 系统的整体架构分层
系统从下到上严格划分为 5 个层次：
1. **硬件模拟层 (`disk.c`)**：把本地的 `vdisk.bin` 视作磁盘，提供按 512 字节（Block）读写的接口。
2. **物理存储层 (`block.c`)**：管理磁盘空间的分配与回收，维护空闲块（超级块）和文件映射表。
3. **Inode 元数据层 (`inode.c`)**：管理文件的属性（大小、权限、创建时间、所在块位置），并在内存中维护缓存哈希表加速访问。
4. **逻辑文件系统层 (`directory.c`, `file.c`, `permission.c`)**：提供目录树结构、路径解析、权限校验以及通用的文件读写（Open, Read, Write）接口。
5. **用户与接口层 (`user.c`, `main.c`)**：处理系统初始化（格式化）、用户登录、多用户会话以及终端命令行交互。

---

## 2. 虚拟磁盘布局 (Disk Layout)
当你运行 `format` 命令时，系统会将 `vdisk.bin` 划分为以下几个区域（定义在 `filesystem.h`）：
* **Block 0**: 引导块（保留，本系统未使用）。
* **Block 1**: **Superblock (超级块)**，记录文件系统的全局状态（空闲块数、空闲 Inode 数等）。
* **Block 2 ~ 33**: **Inode 数组区**。共有 32 个块，每个块存 16 个 Inode（每个 32 字节），系统总共支持几百个文件/目录。
* **Block 34 ~ 末尾**: **Data Blocks (数据块区)**，真正用来存放文件内容和目录表的地方。

---

## 3. 按文件拆解：每个文件在干什么？

### 📌 1. `filesystem.h` (核心数据结构定义)
这是整个系统的**骨架**，定义了所有的常量和核心结构体：
* `struct filsys sb`: **超级块**，采用 UNIX 经典的**成组链接法**缓存空闲数据块。
* `struct dinode`: **磁盘级 Inode**，存在磁盘上，记录文件大小、UID/GID、权限以及 10 个数据块的物理地址（支持直接、一级间接、二级间接索引）。
* `struct inode`: **内存级 Inode**，除了包含 `dinode` 外，还加入了引用计数、锁、哈希链表指针等运行时状态。
* `struct direct`: **目录项**，只有 16 字节（14字节文件名 + 2字节 inode 号）。
* `struct file`: **系统打开文件表**，记录文件的读写偏移量（offset）和打开模式。

### 📌 2. `disk.c` (底层驱动模拟)
最底层的 I/O 模块，负责与宿主机文件系统交互。
* `bread(int blkno, buf)`: 算出偏移量 `blkno * 512`，从 `vdisk.bin` 读出 512 字节。
* `bwrite(int blkno, buf)`: 算出偏移量，向 `vdisk.bin` 写入 512 字节。

### 📌 3. `block.c` (空间分配与地址映射)
**系统的核心引擎之一**，负责管理“地盘”。
* `ialloc()` / `ifree()`: 分配和释放 Inode。当缓存用光时，会去磁盘扫描。
* `balloc()` / `bfree()`: 分配和释放物理数据块。使用了高效的**成组链接法**（超级块里存 50 个空闲块号，用完了一次性从磁盘读下一组）。
* `bmap(ip, lbn)`: **极其关键的函数**！它负责把文件的逻辑块号（LBN）翻译成磁盘的物理块号（PBN）。
  * 前 6 块是**直接索引**。
  * 第 7 块是**单级间接索引**（支持额外 256 块）。
  * 第 8 块是**双级间接索引**（支持 256 * 256 块，支持大文件）。

### 📌 4. `inode.c` (内存 Inode 缓存池)
为了避免每次读文件都去读磁盘，系统在内存中维护了一个 Inode 哈希链表数组 `inode[NHINO]`。
* `iget(ino)`: 获取一个 inode。先查内存缓存，如果没有，再调 `iget_inode` 从磁盘读。通过 `i_count` 增加引用计数。
* `iput(inode)`: 释放 inode。引用计数减 1，如果减到 0，则调 `iput_inode` 将修改过的数据刷回磁盘，并释放内存。

### 📌 5. `directory.c` (目录树解析)
* `namei(char *path)`: **路径解析引擎**。负责把 `/usr/root/test.txt` 这样的字符串，一层层剥开：找根目录 -> 读内容找 usr -> 找 root -> 找 test.txt，最终返回目标文件的 Inode 指针。
* `dir()`: 相当于 Linux 的 `ls` 命令，遍历当前目录的数据块并打印文件名。

### 📌 6. `file.c` (文件操作 VFS 层)
相当于操作系统的系统调用层。
* `create(name)`: 创建新文件，分配 Inode，在父目录写入 `struct direct` 目录项。
* `delete(name)`: 删除文件，释放 Inode 和数据块。
* `open(name, mode)`: 寻找文件，进行权限检查，加锁，分配系统文件表 `sysopenfile` 和用户句柄 `u_ofile`。
* `close(fd)`: 释放句柄，解锁。
* `read(fd, buf, count)` / `write(fd, buf, count)`: 根据当前文件指针偏移量（offset），利用 `bmap` 算出要读写哪个物理块，执行底层 I/O，并更新偏移量。

### 📌 7. `permission.c` (安全与并发)
* `lock_file()` / `unlock_file()`: 实现了一套**读写锁**。允许多个用户同时读（读锁），但写操作是排他的（写锁）。
* `chmod()`: 更改文件权限（读/写/执行位掩码）。
* `check_permission()`: UNIX 经典的 9 位权限校验（User/Group/Other 的 rwx 校验）。

### 📌 8. `user.c` (系统管理与用户态)
* `format()`: **系统格式化程序**。将整个 `vdisk.bin` 写入初始结构，生成超级块、空闲块链表，并创建 `/`、`/usr` 等系统核心目录，以及 AI、KFS 相关的扩展虚拟目录。
* `login()` / `logout()`: 用户会话管理，切换当前用户 UID 和当前所在目录（`cur_dir`）。
* `mkdir()` / `rmdir()` / `chdir()`: 创建/删除目录，相当于特殊的 `create`/`delete`。`chdir` 就是改变全局变量 `cur_dir`。

### 📌 9. `main.c` (Shell 命令行接口)
系统的入口，是一个 REPL（Read-Eval-Print Loop）死循环程序。
它接收用户输入的命令（如 `open`, `read`, `write`, `format`），解析字符串，然后分发给上述底层的各个 C 函数去执行。

---

## 4. 核心工作流举例：当你输入 `read 0 100` 时发生了什么？

1. `main.c` 解析出你的命令是要操作文件描述符 `fd = 0`，读取 `100` 字节。
2. 调用 `file.c` 中的 `read(0, buf, 100)`。
3. 系统通过 `u_ofile[0]` 查找到全局文件表 `sysopenfile` 中的结构体，获取该文件的 **Inode 指针** 和当前**读写偏移量(offset)**。
4. 计算出目前 offset 落在这个文件的逻辑块号 (`lbn = offset / 512`)。
5. 调用 `block.c` 中的 `bmap(inode, lbn)`，查询 Inode 里的数组，得到对应的**物理磁盘块号(PBN)**。
6. 调用 `disk.c` 中的 `bread(PBN, block_buf)`，把 512 字节读到内存缓冲区。
7. 从缓冲区中拷贝用户需要的 100 字节到目标 buf 中。
8. 增加文件表的 offset，返回读取成功的字节数。

---

## 5. 这个系统有何“特殊”之处？
你在 `filesystem.h` 和 `main.c` 的结尾会看到一些传统 UNIX 中没有的定义：
1. **KFS 智能标签分类** (如 `init_kfs`, `kfs_show_tags`)
2. **AI 自适应 I/O 优化** (如 `init_io_opt`, `record_io_request`)
3. **安全异常行为检测** (如 `init_security_system`, `user_profile`)
4. **NLP 自然语言交互** (`nlp_interact`)

这表明这个项目在经典的 UNIX 文件系统之上，**预留了用于 AI 和现代智能管理的 Hooks（钩子）**。通过在普通的文件读写（如 read、write、delete）过程中植入埋点统计，系统试图实现一个具备自适应缓存预测和异常拦截的“智能虚拟文件系统”。

---

**建议你的阅读顺序：**
1. 浏览 `filesystem.h`（看懂宏定义和核心结构体）。
2. 看 `disk.c` 和 `user.c` 的 `format()` 函数（理解磁盘是怎么被格式化出来的）。
3. 看 `directory.c` 的 `namei()`（理解 UNIX 路径是怎么变回 inode 的）。
4. 攻克最难的 `block.c`（搞懂直接索引和间接索引 `bmap`）。
5. 最后看 `file.c` 和 `main.c`（串联起所有逻辑）。