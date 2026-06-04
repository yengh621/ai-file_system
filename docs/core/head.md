### 基础尺寸
- BLOCKSIZ=512：磁盘单块字节；DINODESIZ=32：磁盘inode大小
- DINODEBLK=32：inode占用磁盘块；DATASTART标记数据区起始
- NICFREE/NICINOD=50：超级块缓存空闲块、空闲inode上限
- SYSOPENFILE/NOFILE：系统/单用户最大打开文件数；NHINO=128：inode哈希桶数量
### 磁盘分区
SUPERBLOCK(1)超级块块号，DINODESTART(2)inode起始，KFS_START(34)AI缓存分区起始，DATASTART(66)普通数据区起始。
### 权限与打开标识
兼容UNIX权限位：S_IFDIR/REG/LNK区分目录、普通文件、软链接；UR/GRP/OTH三段rwx权限。
O_RDONLY/WRONLY/RDWR/APPEND为四种打开模式；R/W/X_OK用于权限校验。
### 锁与缓存
LOCK_NONE/READ/WRITE实现读写锁；PREFETCH_CACHE_SIZE、HOT_FILE_CACHE_SIZE限定预取、热点缓存容量。

## 2.核心结构体
1. **struct filsys**：超级块，保存全局空闲块、空闲inode、文件系统修改标记、AI扩展字段。
2. **struct dinode**：磁盘inode，存权限、uid/gid、链接数、文件大小、三级索引地址、时间戳。
3. **struct inode**：内存inode，在dinode基础上加引用计数、哈希链表指针、读写偏移。
4. **struct direct**：目录项（14字节文件名+2字节inode号），构成目录内容。
5. **struct file**：系统打开文件表，记录打开模式、inode指针、当前读写偏移。
6. **文件锁结构体**：实现读共享、写独占并发控制。
7. AI扩展结构体：负载记录、预取缓存、热点文件、用户行为、安全日志相关结构，支撑智能优化。

## 3.全局变量
sb：全局超级块；inode[]：内存inode哈希缓存；user[]：用户列表；cur_uid/cur_dir：当前用户、当前目录；block_buf：全局磁盘读写缓冲区。

## 4.函数声明
分底层磁盘、inode管理、文件目录、权限锁、KFS热点缓存、IO智能预取、安全检测、AI集成八大类接口。
