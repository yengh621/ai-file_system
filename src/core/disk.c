#include "filesystem.h"

struct filsys sb;
unsigned char block_buf[BLOCKSIZ];

void bread(int blkno, unsigned char *buf) {
    FILE *fp = fopen("vdisk.bin", "rb");
    if (fp == NULL) return;
    fseek(fp, blkno * BLOCKSIZ, SEEK_SET);
    fread(buf, BLOCKSIZ, 1, fp);
    fclose(fp);
}

void bwrite(int blkno, unsigned char *buf) {
    FILE *fp = fopen("vdisk.bin", "rb+");
    if (fp == NULL) fp = fopen("vdisk.bin", "wb");
    fseek(fp, blkno * BLOCKSIZ, SEEK_SET);
    fwrite(buf, BLOCKSIZ, 1, fp);
    fclose(fp);
}
