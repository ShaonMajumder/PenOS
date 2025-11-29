#ifndef FS_9P_H
#define FS_9P_H

#include <stdint.h>

// 9P Protocol Constants
#define P9_TVERSION  100
#define P9_RVERSION 101
#define P9_TATTACH   104
#define P9_RATTACH   105
#define P9_TWALK     110
#define P9_RWALK     111
#define P9_TOPEN     112
#define P9_ROPEN     113
#define P9_TREAD     116
#define P9_RREAD     117
#define P9_TCLUNK    120
#define P9_RCLUNK    121

#define P9_NOTAG  ((uint16_t)(~0))
#define P9_NOFID  ((uint32_t)(~0))
#define P9_QIDLEN 13

// 9P QID structure
typedef struct {
    uint8_t type;
    uint32_t version;
    uint64_t path;
} __attribute__((packed)) p9_qid_t;

// 9P Message Header
typedef struct {
    uint32_t size;
    uint8_t type;
    uint16_t tag;
} __attribute__((packed)) p9_hdr_t;

// Functions
int p9_init(void);
int p9_version(void);
int p9_attach(const char *uname, const char *aname);
int p9_walk(uint32_t fid, uint32_t newfid, const char *path);
int p9_open(uint32_t fid, uint8_t mode);
int p9_read(uint32_t fid, uint64_t offset, uint32_t count, void *data);
void p9_clunk(uint32_t fid);

#endif
