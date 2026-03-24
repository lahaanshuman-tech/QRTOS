#ifndef FAS32Q_H
#define FAS32Q_H

// FAS32Q - File Accuracy System 32 Qourtra
// by Qourtra Software Foundation 2026

// ── Constants ──────────────────────────────
#define FAS32Q_MAGIC      0xFA532051  // "FAS32Q" magic number
#define FAS32Q_VERSION    1
#define FAS32Q_SECTOR_SIZE 512
#define FAS32Q_MAX_FILES  128
#define FAS32Q_NAME_LEN   32
#define FAS32Q_EXT_LEN    8

// ── File Flags ─────────────────────────────
#define FAS32Q_FLAG_FILE     0x01   // its a file
#define FAS32Q_FLAG_FOLDER   0x02   // its a folder
#define FAS32Q_FLAG_DELETED  0x04   // deleted (in recycle bin)
#define FAS32Q_FLAG_SYSTEM   0x08   // OS system file

// ── Timestamp ──────────────────────────────
struct fas32q_time {
    unsigned char  min;
    unsigned char  hour;
    unsigned char  day;
    unsigned char  month;
    unsigned short year;
} __attribute__((packed));

// ── Directory Entry ────────────────────────
struct fas32q_entry {
    char           name[FAS32Q_NAME_LEN];
    char           ext[FAS32Q_EXT_LEN];
    unsigned int   size;
    unsigned int   start_sector;
    unsigned int   parent_id;
    unsigned char  flags;
    struct fas32q_time timestamp;
} __attribute__((packed));

// ── Superblock ─────────────────────────────
struct fas32q_superblock {
    unsigned int  magic;
    unsigned char version;
    unsigned int  total_sectors;
    unsigned int  fat_start;
    unsigned int  dir_start;
    unsigned int  data_start;
    unsigned int  recycle_start;
} __attribute__((packed));

// RAM storage - extern declarations
extern struct fas32q_entry ram_files[FAS32Q_MAX_FILES];
extern int ram_file_count;

// RAM functions
int fas32q_ram_create(const char* name, unsigned char flags);
int fas32q_ram_find(const char* name);
int fas32q_ram_delete(const char* name);
int fas32q_ram_restore(const char* name);

#endif
