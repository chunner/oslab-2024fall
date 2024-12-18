#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */


#define SUPERBLOCK_MAGIC 0xDF4C4459
#define NUM_FDESCS 16

#define SECTOR_SIZE 512
#define BLOCK_SIZE 4096
#define SECTOR2BLOCK(s) ((s) / (BLOCK_SIZE / SECTOR_SIZE))
#define BLOCK2SECTOR(b) ((b) * (BLOCK_SIZE / SECTOR_SIZE))

#define FS_NUM_SECTORS (1 << 20)                    // 512 MB = SECTOR_SIZE * NUM_SECTORS = 512B * 2^20
#define FS_START_SECTOR (1 << 20)                // the start sector of file system

// SIZE
#define BLOCK_MAP_SIZE 32                            // 32 SECTOR
#define INODE_MAP_SIZE 1                            // 1 SECTOR
#define INODE_SIZE 512                              // 512 SECTOR
#define DATA_SIZE (FS_NUM_SECTORS - DATA_OFFSET)

// OFFSET
#define BLOCK_MAP_OFFSET 1                          // 4 SECTOR
#define INODE_MAP_OFFSET (BLOCK_MAP_OFFSET + BLOCK_MAP_SIZE)     // 1 SECTOR
#define INODE_OFFSET (INODE_MAP_OFFSET + INODE_MAP_SIZE)         // 512 SECTOR
#define DATA_OFFSET (INODE_OFFSET + INODE_SIZE) 

// inode type
#define IT_DIR 1
#define IT_FILE 2


#define MAX_PATHNAME_LEN 27

// indirect block
#define DIRECT_BLOCK_NUM 7
#define NBLOCK_LV1 (BLOCK_SIZE / sizeof(uint32_t))
#define NBLOCK_LV2 (NBLOCK_LV1 * NBLOCK_LV1)
#define NBLOCK_LV3 (NBLOCK_LV2 * NBLOCK_LV1)




/* data structures of file system */
typedef struct superblock {
    uint32_t magic;
    // offset (sector)
    uint32_t fs_start_sec;
    uint32_t block_map_offset;
    uint32_t inode_map_offset;
    uint32_t inode_offset;
    uint32_t data_offset;
    // size (sector)
    uint32_t fs_size;
    uint32_t block_map_size;
    uint32_t inode_map_size;
    uint32_t inode_size;
    uint32_t data_size;
    // entry size (Byte)
    uint32_t inode_entry_size;
    uint32_t dir_entry_size;

    // TODO [P6-task1]: Implement the data structure of superblock
} superblock_t;     // 52 bytes

typedef struct dentry {
    // TODO [P6-task1]: Implement the data structure of directory entry
    char name[MAX_PATHNAME_LEN];
    uint8_t type;         // 0 is data, 1 is directory
    uint32_t ino;
} dentry_t;               // Total: 32 bytes

typedef struct inode {
    uint8_t mode;        // File type and permissions (1 bytes)
    uint8_t type;        // 0 is data, 1 is directory (1 bytes)
    uint16_t nlink;       // Link count (2 bytes)
    uint32_t ino;         // Inode number (4 bytes)
    uint32_t size;        // File size (Bytes) (4 bytes)
    uint32_t atime;       // Last access time (4 bytes)
    uint32_t mtime;       // Last modification time (4 bytes)
    uint32_t ctime;       // Creation time (4 bytes)
    uint32_t blocks[DIRECT_BLOCK_NUM];  // Data block pointers (32 bytes)
    uint32_t indirect;    // Single indirect pointer (4 bytes)
    uint32_t double_indirect; // Double indirect pointer (4 bytes)
    uint32_t triple_indirect; // Triple indirect pointer (4 bytes)
} inode_t;                // Total: 64 bytes, one sector has 512/64 = 8 inodes 
typedef struct fdesc {
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint8_t mode;
    uint8_t valid;
    uint32_t ino;
    uint32_t read_pos;
    uint32_t write_pos;
} fdesc_t;

/* modes of do_open */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_open(char *path, int mode);
extern int do_read(int fd, char *buff, int length);
extern int do_write(int fd, char *buff, int length);
extern int do_close(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_pwd(void);
extern void init_fs(void);

/* -------------------------------------------------buffer------------------------------------------------------------------------*/
// struct buf {
//   int flags;             // 标志位，b_valid和b_dirty.
//   uint dev;              // 缓存块对应的磁盘设备号
//   uint blockno;          // 缓存块对应的block块号
//   struct sleeplock lock; // 睡眠锁， 保证一个buffer同一时间只可能被一个进程拥有
//   uint refcnt;           // 引用次数
//   struct buf *prev;      // LRU双向链表
//   struct buf *next;      // LRU双向链表
//   struct buf *qnext;     // 当Buffer块需要与磁盘间进行同步时，Buffer块之间组成的单向同步队列
//   uchar data[BSIZE];     // 512字节的数据缓存区
// };
#define NBUF 0x5000
typedef struct buf {
    uint16_t valid;
    uint16_t dirty;
    uint32_t sectorid;
    struct buf *prev;
    struct buf *next;
    char data[SECTOR_SIZE];
}buf_t;         // less then 1024 bytes = 0x400
// struct {
//   struct spinlock lock;   // 锁
//   struct buf buf[NBUF];   // buffer块数组,一大块连续的buffer
//   struct buf head;        // 它可以看出一个哨兵，目的方便双向链表的操作
//                           // head.next 是第一个buffer块， 它是最近使用过的!
// } bcache;
typedef struct {
    buf_t buf[NUM_FDESCS];
    buf_t head;
} bcache_t;         // less than 0x5000 * 0x400 =  0x2000000 = 32MB

int page_cache_policy;
int write_back_freq;

#define BWRITE_BACK 0
#define BWRITE_THROUGH 1


#endif