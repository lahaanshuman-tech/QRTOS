#include "fas32q.h"

// ── Disk Layout ────────────────────────────
// Sector 0    → Bootloader
// Sector 1    → Superblock
// Sector 2-5  → FAT table
// Sector 6-10 → Directory table
// Sector 11+  → File data
// Last 10     → Recycle bin

// ── FAT Table in RAM cache ─────────────────
// (loaded from disk on boot)
unsigned int fat_table[512];  // 512 entries max
struct fas32q_entry dir_table[FAS32Q_MAX_FILES];
struct fas32q_superblock superblock;

// ── Read a sector from disk ─────────────────
void fas32q_read_sector(unsigned int sector, unsigned char* buf) {
    unsigned int i;
    unsigned char* src = (unsigned char*)(0x10000 + (sector * FAS32Q_SECTOR_SIZE));
    for (i = 0; i < FAS32Q_SECTOR_SIZE; i++)
        buf[i] = src[i];
}

// ── Write a sector to disk ──────────────────
void fas32q_write_sector(unsigned int sector, unsigned char* buf) {
    unsigned int i;
    unsigned char* dst = (unsigned char*)(0x10000 + (sector * FAS32Q_SECTOR_SIZE));
    for (i = 0; i < FAS32Q_SECTOR_SIZE; i++)
        dst[i] = buf[i];
}

// ── Load superblock ─────────────────────────
int fas32q_load_superblock() {
    unsigned char buf[FAS32Q_SECTOR_SIZE];
    fas32q_read_sector(1, buf);
    unsigned int* magic = (unsigned int*)buf;
    if (*magic != FAS32Q_MAGIC) return -1;  // not formatted!
    unsigned char* p = buf;
    int i;
    for (i = 0; i < sizeof(struct fas32q_superblock); i++)
        ((unsigned char*)&superblock)[i] = p[i];
    return 0;
}

// ── Format disk with FAS32Q ─────────────────
void fas32q_format() {
    unsigned int i;

    // write superblock
    superblock.magic        = FAS32Q_MAGIC;
    superblock.version      = FAS32Q_VERSION;
    superblock.total_sectors= 2880;  // 1.44MB floppy
    superblock.fat_start    = 2;
    superblock.dir_start    = 6;
    superblock.data_start   = 11;
    superblock.recycle_start= 2870;

    unsigned char buf[FAS32Q_SECTOR_SIZE];
    for (i = 0; i < FAS32Q_SECTOR_SIZE; i++) buf[i] = 0;
    unsigned char* p = (unsigned char*)&superblock;
    for (i = 0; i < sizeof(struct fas32q_superblock); i++)
        buf[i] = p[i];
    fas32q_write_sector(1, buf);

    // clear FAT table
    for (i = 0; i < 512; i++) fat_table[i] = 0;

    // mark system sectors as used
    fat_table[0] = 0xFFFFFFFF;  // bootloader
    fat_table[1] = 0xFFFFFFFF;  // superblock
    fat_table[2] = 0xFFFFFFFF;  // FAT
    fat_table[3] = 0xFFFFFFFF;  // FAT
    fat_table[4] = 0xFFFFFFFF;  // FAT
    fat_table[5] = 0xFFFFFFFF;  // FAT
    fat_table[6] = 0xFFFFFFFF;  // DIR
    fat_table[7] = 0xFFFFFFFF;  // DIR
    fat_table[8] = 0xFFFFFFFF;  // DIR
    fat_table[9] = 0xFFFFFFFF;  // DIR
    fat_table[10]= 0xFFFFFFFF;  // DIR

    // clear directory table
    for (i = 0; i < FAS32Q_MAX_FILES; i++)
        dir_table[i].flags = 0;
}

// ── Find free sector ────────────────────────
int fas32q_find_free_sector() {
    int i;
    for (i = 11; i < 512; i++)
        if (fat_table[i] == 0) return i;
    return -1;
}

// ── Find free dir entry ─────────────────────
int fas32q_find_free_entry() {
    int i;
    for (i = 0; i < FAS32Q_MAX_FILES; i++)
        if (dir_table[i].flags == 0) return i;
    return -1;
}

// ── Create file on disk ─────────────────────
int fas32q_disk_create(const char* name, unsigned char flags) {
    int entry = fas32q_find_free_entry();
    if (entry == -1) return -1;
    int sector = fas32q_find_free_sector();
    if (sector == -1) return -1;

    int i = 0;
    while (name[i] && i < FAS32Q_NAME_LEN - 1) {
        dir_table[entry].name[i] = name[i];
        i++;
    }
    dir_table[entry].name[i]    = '\0';
    dir_table[entry].flags      = flags;
    dir_table[entry].size       = 0;
    dir_table[entry].start_sector = sector;
    dir_table[entry].parent_id  = 0;

    fat_table[sector] = 0xFFFFFFFF;  // mark as used
    return entry;
}
