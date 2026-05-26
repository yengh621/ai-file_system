#include "filesystem.h"

int ialloc(void) {
    int ino;
    if (sb.s_ninode > 0) {
        ino = sb.s_inode[--sb.s_ninode];
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
    } else {
        struct dinode di;
        memset(&di, 0, sizeof(di));
        iput_inode(ino, &di);
    }
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
        bread(blkno, block_buf);
        memcpy(&sb.s_nfree, block_buf, sizeof(unsigned short));
        memcpy(sb.s_free, block_buf + sizeof(unsigned short), NICFREE * sizeof(unsigned short));
        sb.s_fmod = 1;
        return blkno;
    }
    return 0;
}

void bfree(int blkno) {
    if (sb.s_nfree < NICFREE) {
        sb.s_free[sb.s_nfree++] = blkno;
    } else {
        memcpy(block_buf, &sb.s_nfree, sizeof(unsigned short));
        memcpy(block_buf + sizeof(unsigned short), sb.s_free, NICFREE * sizeof(unsigned short));
        bwrite(blkno, block_buf);
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
    } else if (lbn < 6 + 128) {
        if (addr[6] == 0) {
            addr[6] = balloc();
            if (addr[6] == 0) return 0;
            memset(block_buf, 0, BLOCKSIZ);
            bwrite(addr[6], block_buf);
            ip->i_flag |= 1;
        }
        bread(addr[6], block_buf);
        bn = ((unsigned short*)block_buf)[lbn - 6];
        if (bn == 0) {
            bn = balloc();
            if (bn != 0) {
                ((unsigned short*)block_buf)[lbn - 6] = bn;
                bwrite(addr[6], block_buf);
                ip->i_flag |= 1;
            }
        }
        return bn;
    } else if (lbn < 6 + 128 + 128*128) {
        int idx1 = (lbn - 6 - 128) / 128;
        int idx2 = (lbn - 6 - 128) % 128;
        if (addr[7] == 0) {
            addr[7] = balloc();
            if (addr[7] == 0) return 0;
            memset(block_buf, 0, BLOCKSIZ);
            bwrite(addr[7], block_buf);
            ip->i_flag |= 1;
        }
        bread(addr[7], block_buf);
        bn = ((unsigned short*)block_buf)[idx1];
        if (bn == 0) {
            bn = balloc();
            if (bn == 0) return 0;
            ((unsigned short*)block_buf)[idx1] = bn;
            memset(block_buf, 0, BLOCKSIZ);
            bwrite(bn, block_buf);
            ip->i_flag |= 1;
        }
        bread(bn, block_buf);
        bn = ((unsigned short*)block_buf)[idx2];
        if (bn == 0) {
            bn = balloc();
            if (bn != 0) {
                ((unsigned short*)block_buf)[idx2] = bn;
                bwrite(addr[7], block_buf);
                ip->i_flag |= 1;
            }
        }
        return bn;
    }
    return 0;
}
