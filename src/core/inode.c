#include "filesystem.h"

struct inode *inode[NHINO];

void iget_inode(int ino, struct dinode *di) {
    int blkno = DINODESTART + (ino - 1) / (BLOCKSIZ / DINODESIZ);
    int offset = ((ino - 1) % (BLOCKSIZ / DINODESIZ)) * DINODESIZ;
    bread(blkno, block_buf);
    memcpy(di, block_buf + offset, DINODESIZ);
}

void iput_inode(int ino, struct dinode *di) {
    int blkno = DINODESTART + (ino - 1) / (BLOCKSIZ / DINODESIZ);
    int offset = ((ino - 1) % (BLOCKSIZ / DINODESIZ)) * DINODESIZ;
    bread(blkno, block_buf);
    memcpy(block_buf + offset, di, DINODESIZ);
    bwrite(blkno, block_buf);
}

struct inode* iget(int ino) {
    int hash = ino % NHINO;
    struct inode *p = inode[hash];
    while (p != NULL) {
        if (p->i_ino == ino) {
            p->i_count++;
            return p;
        }
        p = p->i_forw;
    }
    p = (struct inode*)malloc(sizeof(struct inode));
    iget_inode(ino, &p->i_din);
    p->i_ino = ino;
    p->i_count = 1;
    p->i_flag = 0;
    p->i_offset = 0;
    p->i_forw = inode[hash];
    p->i_back = NULL;
    if (inode[hash] != NULL) inode[hash]->i_back = p;
    inode[hash] = p;
    return p;
}

void iput(struct inode *p) {
    if (p == NULL) return;
    p->i_count--;
    if (p->i_count > 0) return;
    iput_inode(p->i_ino, &p->i_din);
    int hash = p->i_ino % NHINO;
    if (p->i_back != NULL) p->i_back->i_forw = p->i_forw;
    else inode[hash] = p->i_forw;
    if (p->i_forw != NULL) p->i_forw->i_back = p->i_back;
    free(p);
}
