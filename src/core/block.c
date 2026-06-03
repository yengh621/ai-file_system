#include "filesystem.h"

int ialloc(void) {
    int ino;
    while (sb.s_ninode > 0) {
        struct dinode di;

        ino = sb.s_inode[--sb.s_ninode];
        if (ino < 1 || ino > DINODEBLK * (BLOCKSIZ / DINODESIZ)) {
            printf("Error: invalid inode %d from stack!\n", ino);
            continue;
        }

        iget_inode(ino, &di);
        if (di.di_mode != 0) {
            continue;
        }

        sb.s_fmod = 1;
        return ino;
    }
    for (ino = 1; ino <= DINODEBLK * (BLOCKSIZ / DINODESIZ); ino++) {
        struct dinode di;
        iget_inode(ino, &di);
        if (di.di_mode == 0) {
            sb.s_fmod = 1;
            return ino;
        }
    }
    return 0;
}

void ifree(int ino) {
    if (sb.s_ninode < NICINOD) {
        sb.s_inode[sb.s_ninode++] = ino;
    }
    // 清零磁盘 inode 的 di_mode，标记为空闲
    struct dinode di;
    memset(&di, 0, sizeof(di));
    iput_inode(ino, &di);   // 直接写回清零的 dinode
    sb.s_fmod = 1;
}

int balloc(void) {
    int blkno;
    if (sb.s_nfree > 1) {
        blkno = sb.s_free[--sb.s_nfree];
        sb.s_fmod = 1;
        return blkno;
    } else if (sb.s_nfree == 1) {
        blkno = sb.s_free[0];
        if (blkno == 0) {
            printf("Error: Out of disk space!\n");
            return 0;
        }
        unsigned char local_buf[BLOCKSIZ];
        bread(blkno, local_buf);
        memcpy(&sb.s_nfree, local_buf, sizeof(unsigned short));
        memcpy(sb.s_free, local_buf + sizeof(unsigned short), NICFREE * sizeof(unsigned short));
        sb.s_fmod = 1;
        return blkno;
    }
    return 0;
}

void bfree(int blkno) {
    if (sb.s_nfree < NICFREE) {
        sb.s_free[sb.s_nfree++] = blkno;
    } else {
        unsigned char local_buf[BLOCKSIZ];
        memset(local_buf, 0, BLOCKSIZ);
        memcpy(local_buf, &sb.s_nfree, sizeof(unsigned short));
        memcpy(local_buf + sizeof(unsigned short), sb.s_free, NICFREE * sizeof(unsigned short));
        bwrite(blkno, local_buf);
        sb.s_nfree = 1;
        sb.s_free[0] = blkno;
    }
    sb.s_fmod = 1;
}

int bmap(struct inode *ip, int lbn) {
    int i, bn;
    unsigned short *addr = ip->i_din.di_addr;
    if (lbn < 6) {
        bn = addr[lbn];
        if (bn == 0) {
            bn = balloc();
            if (bn != 0) {
                addr[lbn] = bn;
                ip->i_flag |= 1;
            }
        }
        return bn;
    } else if (lbn < 6 + 256) {
        unsigned char indirect_buf[BLOCKSIZ];
        if (addr[6] == 0) {
            addr[6] = balloc();
            if (addr[6] == 0) return 0;
            memset(indirect_buf, 0, BLOCKSIZ);
            bwrite(addr[6], indirect_buf);
            ip->i_flag |= 1;
        }
        bread(addr[6], indirect_buf);
        bn = ((unsigned short*)indirect_buf)[lbn - 6];
        if (bn == 0) {
            bn = balloc();
            if (bn != 0) {
                ((unsigned short*)indirect_buf)[lbn - 6] = bn;
                bwrite(addr[6], indirect_buf);
                ip->i_flag |= 1;
            }
        }
        return bn;
    } else if (lbn < 6 + 256 + 256*256) {
        unsigned char buf1[BLOCKSIZ];
        unsigned char buf2[BLOCKSIZ];
        int idx1 = (lbn - 6 - 256) / 256;
        int idx2 = (lbn - 6 - 256) % 256;
        if (addr[7] == 0) {
            addr[7] = balloc();
            if (addr[7] == 0) return 0;
            memset(buf1, 0, BLOCKSIZ);
            bwrite(addr[7], buf1);
            ip->i_flag |= 1;
        }
        bread(addr[7], buf1);
        bn = ((unsigned short*)buf1)[idx1];
        if (bn == 0) {
            bn = balloc();
            if (bn == 0) return 0;
            ((unsigned short*)buf1)[idx1] = bn;
            memset(buf2, 0, BLOCKSIZ);
            bwrite(bn, buf2);
            bwrite(addr[7], buf1);
            ip->i_flag |= 1;
        }
        int blk2 = bn;
        bread(blk2, buf2);
        bn = ((unsigned short*)buf2)[idx2];
        if (bn == 0) {
            bn = balloc();
            if (bn != 0) {
                ((unsigned short*)buf2)[idx2] = bn;
                bwrite(blk2, buf2);
                ip->i_flag |= 1;
            }
        }
        return bn;
    }
    return 0;
}
