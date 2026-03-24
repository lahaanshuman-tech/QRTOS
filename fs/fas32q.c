#include "fas32q.h"

// ── RAM Storage ────────────────────────────
struct fas32q_entry ram_files[FAS32Q_MAX_FILES];
int ram_file_count = 0;

// ── Find free RAM slot ──────────────────────
int fas32q_ram_find_free() {
    int i;
    for (i = 0; i < FAS32Q_MAX_FILES; i++)
        if (ram_files[i].flags == 0) return i;
    return -1;
}

// ── Create file in RAM ──────────────────────
int fas32q_ram_create(const char* name, unsigned char flags) {
    int slot = fas32q_ram_find_free();
    if (slot == -1) return -1;

    int i = 0;
    while (name[i] && i < FAS32Q_NAME_LEN - 1) {
        ram_files[slot].name[i] = name[i];
        i++;
    }
    ram_files[slot].name[i] = '\0';
    ram_files[slot].flags = flags;
    ram_files[slot].size  = 0;
    ram_files[slot].parent_id = 0;
    ram_file_count++;
    return slot;
}

// ── Find file in RAM ────────────────────────
int fas32q_ram_find(const char* name) {
    int i, j;
    for (i = 0; i < FAS32Q_MAX_FILES; i++) {
        if (ram_files[i].flags == 0) continue;
        if (ram_files[i].flags & FAS32Q_FLAG_DELETED) continue;
        int match = 1;
        for (j = 0; j < FAS32Q_NAME_LEN; j++) {
            if (ram_files[i].name[j] != name[j]) { match = 0; break; }
            if (ram_files[i].name[j] == '\0') break;
        }
        if (match) return i;
    }
    return -1;
}

// ── Delete file in RAM (recycle) ────────────
int fas32q_ram_delete(const char* name) {
    int slot = fas32q_ram_find(name);
    if (slot == -1) return -1;
    ram_files[slot].flags |= FAS32Q_FLAG_DELETED;
    return 0;
}

// ── Restore from recycle bin ────────────────
int fas32q_ram_restore(const char* name) {
    int i, j;
    for (i = 0; i < FAS32Q_MAX_FILES; i++) {
        if (!(ram_files[i].flags & FAS32Q_FLAG_DELETED)) continue;
        int match = 1;
        for (j = 0; j < FAS32Q_NAME_LEN; j++) {
            if (ram_files[i].name[j] != name[j]) { match = 0; break; }
            if (ram_files[i].name[j] == '\0') break;
        }
        if (match) {
            ram_files[i].flags &= ~FAS32Q_FLAG_DELETED;
            return 0;
        }
    }
    return -1;
}
