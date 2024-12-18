#include <os/string.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <os/mm.h>
#include <os/time.h>
#include <assert.h>

static fdesc_t fdesc_array[NUM_FDESCS];
static char superblock_buffer[SECTOR_SIZE];
static char inode_map[INODE_MAP_SIZE * SECTOR_SIZE];
static char block_map[BLOCK_MAP_SIZE * SECTOR_SIZE];    // size = 32 sectors, 4 blocks, 16KB
static inode_t inode_buffer[SECTOR_SIZE / sizeof(inode_t)];     // size = one sector, 512B
static dentry_t dentry_buffer[BLOCK_SIZE / sizeof(dentry_t)];  // size = one block, 4KB
static char rdata_buffer[BLOCK_SIZE];  // size = one block, 4KB
static char wdata_buffer[BLOCK_SIZE];  // size = one block, 4KB

static inode_t wd_inode;    // working directory

static inode_t *find_inode(char *path, inode_t *parent_inode);
static void parse_path(char *path, char *dir1, char *dir2, char *dir3);

/* ---------------------------------------------------------file cache------------------------------------------------------- */
bcache_t *bcache = (bcache_t *) FILE_CACHE_BASE;



void init_bcache() {
    // init bcache array
    bcache->head.next = bcache->head.prev = &bcache->head;
    for (int i = 0; i < NBUF; i++) {
        bcache->buf[i].valid = 0;
    }
    // create /proc/sys/vm, wd_inode is root when init
    do_mkdir("/proc");
    do_cd("proc");
    do_mkdir("sys");
    do_cd("sys");
    do_touch("vm");
    int fd = do_open("vm", O_RDWR);
    do_write(fd, "page_cache_policy = write back\n", 31);
    do_write(fd, "write_back_freq = 30\n", 22);
    do_close(fd);
    do_cd("../..");
    // cache config
    page_cache_policy = BWRITE_BACK;
    write_back_freq = 30;
}

buf_t *find_free_buf() {
    buf_t *buf = &bcache->buf[0];
    for (int i = 0; i < NBUF; i++) {
        if (buf->valid == 0) {
            return buf;
        }
        buf++;
    }
}

buf_t *search_buf(uint32_t sectorid) {
    buf_t *buf = bcache->head.next;
    while (buf != &bcache->head) {
        if ((buf->valid == 1) && (buf->sectorid == sectorid)) {
            return buf;
        }
        buf = buf->next;
    }
    return NULL;
}

buf_t *brefill(uint32_t sectorid) {
    buf_t *buf = find_free_buf();
    if (buf == NULL) {
        return NULL;
    }
    buf->valid = 1;
    buf->sectorid = sectorid;
    buf->dirty = 0;
    bios_sd_read((unsigned) buf->data, 1, sectorid);
    // insert to head
    buf->next = bcache->head.next;
    buf->prev = &bcache->head;
    bcache->head.next->prev = buf;
    bcache->head.next = buf;
    return buf;
}

void bwrite(unsigned mem_address, unsigned nsectors, unsigned sectorid, int mode) {
    for (int i = 0; i < nsectors; i++) {
        buf_t *buf = search_buf(sectorid + i);
        if (buf == NULL) {
            buf = brefill(sectorid + i);
        }
        buf->dirty = 1;
        memcpy((void *) buf->data, (void *) mem_address, SECTOR_SIZE);
        if (mode == BWRITE_THROUGH) { // write through
            bios_sd_write((unsigned) buf->data, 1, sectorid + i);
        }
    }
}

void bread(unsigned mem_address, unsigned nsectors, unsigned sectorid) {
    for (int i = 0; i < nsectors; i++) {
        buf_t *buf = search_buf(sectorid + i);
        if (buf == NULL) {
            buf = brefill(sectorid + i);
        }
        memcpy((void *) mem_address, (void *) buf->data, SECTOR_SIZE);
    }
}

void bflush() {
    buf_t *buf = bcache->head.next;
    while (buf != &bcache->head) {
        if (buf->dirty) {
            bios_sd_write((unsigned) buf->data, 1, buf->sectorid);
            buf->dirty = 0;
        }
        buf = buf->next;
    }
}









































/*---------------------------------------------------fs---------------------------------------------------------*/

uint32_t inodeidx2sector(uint32_t inode_idx) {
    return FS_START_SECTOR + INODE_OFFSET + ROUNDDOWN(inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
}
uint32_t inodeidx2offset(uint32_t inode_idx) {
    return inode_idx % (SECTOR_SIZE / sizeof(inode_t));
}
// inode_map
uint32_t find_free_inode() {
    bios_sd_read(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
    for (uint32_t byte_idx = 0; byte_idx < INODE_MAP_SIZE * SECTOR_SIZE; byte_idx++) {
        if (inode_map[byte_idx] != 0xFF) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                uint32_t inode_idx = byte_idx * 8 + bit;
                if (!(inode_map[byte_idx] & (1 << bit))) {
                    inode_map[byte_idx] |= (1 << bit);  // Set the corresponding bit to 1
                    bios_sd_write(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
                    return inode_idx;
                }
            }
        }
    }
    return -1;  // Not enough consecutive free pages found
}
// block_map, rdata_buffer
// return the first sector offset of the free block
uint32_t find_free_block() {
    bios_sd_read(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
    uint32_t data_block_num = SECTOR2BLOCK(DATA_SIZE);
    for (uint32_t byte_idx = 0; byte_idx < BLOCK_MAP_SIZE * SECTOR_SIZE; byte_idx++) {
        if (block_map[byte_idx] != 0xFF) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                uint32_t block_idx = byte_idx * 8 + bit;
                if (block_idx >= data_block_num) {
                    return -1;
                }
                if (!(block_map[byte_idx] & (1 << bit))) {
                    block_map[byte_idx] |= (1 << bit);  // Set the corresponding bit to 1
                    bios_sd_write(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
                    // clear block
                    uint32_t block_sector = BLOCK2SECTOR(block_idx) + DATA_OFFSET + FS_START_SECTOR;
                    bzero((void *) rdata_buffer, BLOCK_SIZE);
                    bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_sector);
                    return block_sector;
                }
            }
        }
    }
    return -1;  // Not enough consecutive free pages found
}
// rdata_buffer, block_map
void free_block(uint32_t block_sector) {
    // clear block
    // bzero((void *) rdata_buffer, BLOCK_SIZE);
    // bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_sector);
    assert(block_sector >= DATA_OFFSET + FS_START_SECTOR);
    // clear block map
    bios_sd_read(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
    uint32_t block_idx = SECTOR2BLOCK(block_sector - DATA_OFFSET - FS_START_SECTOR);
    uint32_t byte_idx = block_idx / 8;
    uint32_t bit = block_idx % 8;
    block_map[byte_idx] &= ~(1 << bit);  // Set the corresponding bit to 0
    bios_sd_write(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
}
// inode_buffer, inode_map
void free_inode(uint32_t inode_idx) {
    // get inode
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t *inode = &inode_buffer[inode_offset];
    // delete block
    uint32_t nblocks = ROUND(inode->size, BLOCK_SIZE) / BLOCK_SIZE;
    for (int64_t i = (int64_t) nblocks - 1; i >= 0; i--) {
        if (i < DIRECT_BLOCK_NUM) {  // direct block
            uint32_t data_block_sec = inode->blocks[i];
            if (data_block_sec != 0) {      // skip empty block
                free_block(data_block_sec);
                printl("<%d>: free_data_inode: (%d) %x\n", i, i, data_block_sec);
            }
        } else if (i < DIRECT_BLOCK_NUM + NBLOCK_LV1) {  // indirect block
            uint32_t index = i - DIRECT_BLOCK_NUM;
            uint32_t block_idx1 = index;
            uint32_t block_lv1_sec = inode->blocks[DIRECT_BLOCK_NUM];
            if (block_lv1_sec == 0) {   // skip whole block lv1
                i -= index;
                continue;
            }
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sec);
            uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
            uint32_t data_block_sec = block_lv1[block_idx1];
            if (data_block_sec != 0) {
                free_block(data_block_sec);
                printl("<%d>: free_data_inode: (%d) %x\n", i, block_idx1, data_block_sec);
            }
            // free indirect block
            if (index % NBLOCK_LV1 == 0) {
                free_block(block_lv1_sec);
                printl(" free_block_lv1： %x\n", block_lv1_sec);
            }
        } else if (i < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2) {  // double indirect block
            uint32_t index = i - DIRECT_BLOCK_NUM - NBLOCK_LV1;
            uint32_t block_idx2 = index / NBLOCK_LV1;
            uint32_t block_idx1 = index % NBLOCK_LV1;
            uint32_t block_lv2_sec = inode->blocks[DIRECT_BLOCK_NUM + 1];
            if (block_lv2_sec == 0) { // skip whole block lv2
                i -= index;
                continue;
            }
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sec);
            uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
            uint32_t block_lv1_sec = block_lv2[block_idx2];
            if (block_lv1_sec == 0) {    // skip whole block lv1
                i -= (index % NBLOCK_LV1);
                index = i - DIRECT_BLOCK_NUM - NBLOCK_LV1;
            } else {
                bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sec);
                uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
                uint32_t data_block_sec = block_lv1[block_idx1];
                if (data_block_sec != 0) {
                    free_block(data_block_sec);
                    printl("<%d>: free_data_inode: (%d, %d) %x\n", i, block_idx2, block_idx1, data_block_sec);
                }
                // free indirect block
                if (index % NBLOCK_LV1 == 0) {
                    free_block(block_lv1_sec);
                    printl("<%d>: free_block_lv1： %x\n", i, block_lv1_sec);
                }
            }
            if (index % NBLOCK_LV2 == 0) {
                free_block(block_lv2_sec);
                printl("<%d>: free_block_lv2： %x\n", i, block_lv2_sec);
            }

        } else if (i < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2 + NBLOCK_LV3) { // trip indirect block
            // get block lv1, lv2, lv3
            uint32_t index = i - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2;
            uint32_t block_idx3 = index / NBLOCK_LV2;
            uint32_t block_idx2 = (index - block_idx3 * NBLOCK_LV1 * NBLOCK_LV1) / NBLOCK_LV1;
            uint32_t block_idx1 = index % NBLOCK_LV1;
            uint32_t block_lv3_sec = inode->blocks[DIRECT_BLOCK_NUM + 2];
            if (block_lv3_sec == 0) {    // skip whole block lv3
                i -= index;
                continue;
            }
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv3_sec);
            uint32_t *block_lv3 = (uint32_t *) rdata_buffer;
            uint32_t block_lv2_sec = block_lv3[block_idx3];
            if (block_lv2_sec == 0) {   // skip whole block lv2
                i -= (index % NBLOCK_LV2);
                index = i - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2;
            } else {
                bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sec);
                uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
                uint32_t block_lv1_sec = block_lv2[block_idx2];
                if (block_lv1_sec == 0) {   // skip whole block lv1
                    i -= (index % NBLOCK_LV1);
                    index = i - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2;
                } else {
                    bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sec);
                    uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
                    uint32_t data_block_sec = block_lv1[block_idx1];
                    // free data block
                    if (data_block_sec != 0) {
                        free_block(data_block_sec);
                        printl("<%d>: free_data_inode: (%d, %d, %d)", i, block_idx1, block_idx2, block_idx3);
                    }
                    // free indirect block
                    if (index % NBLOCK_LV1 == 0) {  // free block lv1
                        free_block(block_lv1_sec);
                        printl("<%d>: free_block_lv1： %x\n", i, block_lv1_sec);
                    }
                }
                if (index % (NBLOCK_LV2) == 0) {  // free block lv2
                    free_block(block_lv2_sec);
                    printl("<%d>: free_block_lv2： %x\n", i, block_lv2_sec);
                }
            }
            if (index % (NBLOCK_LV3) == 0) {  // free block lv1
                free_block(block_lv3_sec);
                printl("<%d>: free_block_lv3： %x\n", i, block_lv3_sec);
            }
        }
    }
    // delete inode
    bzero((void *) &inode_buffer[inode_offset], sizeof(inode_t));
    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
    // clear inode map
    bios_sd_read(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
    uint32_t byte_idx = inode_idx / 8;
    uint32_t bit = inode_idx % 8;
    inode_map[byte_idx] &= ~(1 << bit);  // Set the corresponding bit to 0
    bios_sd_write(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
}
// rdata_buffer
uint32_t blockid2sector(uint32_t block_id, inode_t *inode) {
    if (block_id < DIRECT_BLOCK_NUM) {  // direct block
        return inode->blocks[block_id];
    }
    if (block_id < DIRECT_BLOCK_NUM + NBLOCK_LV1) {   // indirect block
        uint32_t block_lv1_sec = inode->blocks[DIRECT_BLOCK_NUM];
        if (block_lv1_sec == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM]);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        uint32_t block_lv1_idx = block_id - DIRECT_BLOCK_NUM;
        return block_lv1[block_lv1_idx];
    }
    // double indirect block
    if (block_id < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2) {
        uint32_t index = block_id - DIRECT_BLOCK_NUM - NBLOCK_LV1;
        uint32_t block_lv2_sec = inode->blocks[DIRECT_BLOCK_NUM + 1];
        if (block_lv2_sec == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 1]);
        uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
        uint32_t block_lv2_idx = index / NBLOCK_LV1;
        uint32_t block_lv1_sec = block_lv2[block_lv2_idx];
        if (block_lv1_sec == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sec);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        uint32_t block_lv1_idx = index % (NBLOCK_LV1);
        return block_lv1[block_lv1_idx];
    }
    // triple indirect block
    if (block_id < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2 + NBLOCK_LV3) {
        uint32_t index = block_id - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2;
        if (inode->blocks[DIRECT_BLOCK_NUM + 2] == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 2]);
        uint32_t *block_lv3 = (uint32_t *) rdata_buffer;
        uint32_t block_lv3_idx = index / NBLOCK_LV2;
        uint32_t block_lv2_sec = block_lv3[block_lv3_idx];
        if (block_lv2_sec == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sec);
        uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
        uint32_t block_lv2_idx = (index - block_lv3_idx * NBLOCK_LV2) / (NBLOCK_LV1);
        uint32_t block_lv1_sec = block_lv2[block_lv2_idx];
        if (block_lv1_sec == 0) {
            return 0;
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sec);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        uint32_t block_lv1_idx = index % (NBLOCK_LV1);
        return block_lv1[block_lv1_idx];
    }
    return -1;
}

void init_fs(void) {
    // Initialize the filesystem
    // read superblock
    bios_sd_read(kva2pa(superblock_buffer), 1, FS_START_SECTOR);
    superblock_t *superblock = (superblock_t *) superblock_buffer;
    if (superblock->magic != SUPERBLOCK_MAGIC) {
        do_mkfs();
    }
    // read root inode;
    bios_sd_read(kva2pa(inode_buffer), 1, inodeidx2sector(0));
    wd_inode = inode_buffer[0];
}

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    printk("[FS] Start initialize filesystem!\n");
    /* -------------------------------------------------------superblock------------------------------------------------*/
    printk("[FS] Setting superblock...\n");
    superblock_t *superblock = (superblock_t *) superblock_buffer;
    superblock->magic = SUPERBLOCK_MAGIC;
    // offset
    superblock->fs_start_sec = FS_START_SECTOR;
    superblock->block_map_offset = BLOCK_MAP_OFFSET;
    superblock->inode_map_offset = INODE_MAP_OFFSET;
    superblock->inode_offset = INODE_OFFSET;
    superblock->data_offset = DATA_OFFSET;
    // size
    superblock->fs_size = FS_NUM_SECTORS;
    superblock->block_map_size = BLOCK_MAP_SIZE;
    superblock->inode_map_size = INODE_MAP_SIZE;
    superblock->inode_size = INODE_SIZE;
    superblock->data_size = DATA_SIZE;
    // entry size
    superblock->inode_entry_size = sizeof(inode_t);
    superblock->dir_entry_size = sizeof(dentry_t);

    printk("\t magic number: 0x%x\n", superblock->magic);
    printk("\t num sectors: %d, start sector: %d\n", superblock->fs_size, superblock->fs_start_sec);
    printk("\t block map offset : %d (%d)\n", superblock->block_map_offset, superblock->block_map_size);
    printk("\t inode map offset : %d (%d)\n", superblock->inode_map_offset, superblock->inode_map_size);
    printk("\t inode offset : %d (%d)\n", superblock->inode_offset, superblock->inode_size);
    printk("\t data offset : %d (%d)\n", superblock->data_offset, superblock->data_size);
    printk("\t inode entry size: %dB, dir entry size: %dB\n", superblock->inode_entry_size, superblock->dir_entry_size);

    bios_sd_write(kva2pa(superblock), 1, FS_START_SECTOR);
    /* --------------------------------------------------block map--------------------------------------------------------------- */
    printk("[FS] Setting block_map...\n");
    bzero((void *) block_map, BLOCK_MAP_SIZE * SECTOR_SIZE);
    bios_sd_write(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
    /* --------------------------------------------------inode map--------------------------------------------------------------- */
    printk("[FS] Setting inode_map...\n");
    bzero((void *) inode_map, INODE_MAP_SIZE * SECTOR_SIZE);
    bios_sd_write(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
    /* --------------------------------------------------inode--------------------------------------------------------------- */
    printk("[FS] Setting inode...\n");
    // root inode
    uint32_t root_inode_idx = find_free_inode();
    uint32_t root_inode_sector = inodeidx2sector(root_inode_idx);
    uint32_t root_inode_offset = inodeidx2offset(root_inode_idx);
    // bios_sd_read(kva2pa(inode_buffer), 1, root_inode_sector);
    bzero((void *) inode_buffer, SECTOR_SIZE);
    inode_t *root_inode = &inode_buffer[root_inode_offset];
    root_inode->mode = O_RDWR;
    root_inode->size = 2 * sizeof(dentry_t); // . and ..
    root_inode->atime = root_inode->mtime = root_inode->ctime = get_timer();
    root_inode->ino = root_inode_idx;
    root_inode->nlink = 1;
    root_inode->type = IT_DIR;
    root_inode->blocks[0] = find_free_block();
    printl("alloc_dir_block for root: %x\n", root_inode->blocks[0]);
    bios_sd_write(kva2pa(inode_buffer), 1, root_inode_sector);
    wd_inode = *root_inode;
    // root dentry
    uint32_t root_dentry_sector = root_inode->blocks[0];
    // bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, root_dentry_sector);   // one block
    bzero((void *) dentry_buffer, BLOCK_SIZE);
    strcpy(dentry_buffer[0].name, ".");
    dentry_buffer[0].ino = root_inode_idx;
    dentry_buffer[0].type = IT_DIR;
    strcpy(dentry_buffer[1].name, "..");
    dentry_buffer[1].ino = root_inode_idx;
    dentry_buffer[1].type = IT_DIR;
    bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, root_dentry_sector);
    printk("[FS] Filesystem initialized successfully!\n");
    init_bcache();
    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    printk("[FS] Filesystem information:\n");
    bios_sd_read(kva2pa(superblock_buffer), 1, FS_START_SECTOR);
    superblock_t *superblock = (superblock_t *) superblock_buffer;
    if (superblock->magic != SUPERBLOCK_MAGIC) {
        printk("\t magic number: 0x%x, not a valid filesystem!\n", superblock->magic);
        return -1;
    }
    printk("\t magic number: 0x%x\n", superblock->magic);
    uint32_t used_blocks = 0;
    bios_sd_read(kva2pa(block_map), BLOCK_MAP_SIZE, BLOCK_MAP_OFFSET + FS_START_SECTOR);
    for (uint32_t byte_idx = 0; byte_idx < BLOCK_MAP_SIZE * SECTOR_SIZE; byte_idx++) {
        if (block_map[byte_idx] != 0x00) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                uint32_t block_idx = byte_idx * 8 + bit;
                if (block_idx >= SECTOR2BLOCK(DATA_SIZE)) {
                    break;
                }
                if ((block_map[byte_idx] & (1 << bit))) {
                    used_blocks++;
                }
            }
        }
    }
    uint32_t used_inodes = 0;
    bios_sd_read(kva2pa(inode_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
    for (uint32_t byte_idx = 0; byte_idx < INODE_MAP_SIZE * SECTOR_SIZE; byte_idx++) {
        if (inode_map[byte_idx] != 0x00) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                if ((inode_map[byte_idx] & (1 << bit))) {
                    used_inodes++;
                }
            }
        }
    }
    printk("\t total sectors: %d, start sector: %d\n", superblock->fs_size, superblock->fs_start_sec);
    printk("\t block map offset : %d, occupied sector: %d\n", superblock->block_map_offset, superblock->block_map_size);
    printk("\t inode map offset : %d, occupied sector: %d\n", superblock->inode_map_offset, superblock->inode_map_size);
    printk("\t inode offset : %d, occupied sector: %d, used inode: %d/%d\n", superblock->inode_offset, superblock->inode_size
        , used_inodes, INODE_SIZE * SECTOR_SIZE / sizeof(inode_t));
    printk("\t data offset : %d, occupied sector: %d, used block: %d/%d\n", superblock->data_offset, DATA_SIZE
        , used_blocks, SECTOR2BLOCK(DATA_SIZE));
    printk("\t inode entry size: %dB, dir entry size: %dB\n", superblock->inode_entry_size, superblock->dir_entry_size);
    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    inode_t *inode = find_inode(path, &wd_inode);
    if (inode == NULL) {
        printk("[FS] cd: %s: No such file or directory\n", path);
        return -1;
    } else if (inode->type != IT_DIR) {
        printk("[FS] cd: %s: Not a directory\n", path);
        return -1;
    }
    wd_inode = *inode;
    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    if (strlen(path) >= 27) {
        printk("[FS] mkdir: cannot create directory '%s': File name too long\n", path);
        return -1;
    }
    // TODO [P6-task1]: Implement do_mkdir
    // search wd whether has the same name
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, path) == 0) {
            printk("[FS] mkdir: cannot create directory '%s': File exists\n", path);
            return -1;
        }
    }

    // find a free inode
    uint32_t inode_idx = find_free_inode();
    if (inode_idx == -1) {
        printk("[FS] mkdir: cannot create directory '%s': No space left on device\n", path);
        return -1;
    }
    // find a free block, assign
    uint32_t new_block = find_free_block();
    printl("alloc_dir_block for %s: %x\n", path, new_block);
    if (new_block == -1) {
        printk("[FS] mkdir: cannot create directory '%s': No space left on device\n", path);
        return -1;
    }
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);     // load inode from sd
    inode_t *inode = &inode_buffer[inode_offset];
    bzero((void *) inode, sizeof(inode_t));
    inode->mode = O_RDWR;
    inode->size = sizeof(dentry_t) * 2;   // .. and .
    inode->atime = inode->mtime = inode->ctime = get_timer();
    inode->ino = inode_idx;
    inode->nlink = 1;
    inode->type = IT_DIR;
    inode->blocks[0] = new_block;
    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
    // update wd dentry
    int found = 0;
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].name[0] == 0) {
            strcpy(dentry_buffer[j].name, path);
            dentry_buffer[j].ino = inode_idx;
            dentry_buffer[j].type = IT_DIR;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] mkdir: cannot create directory '%s': no dentry\n", path);
        return -1;
    }
    // update wd inode
    wd_inode.size += sizeof(dentry_t);
    wd_inode.mtime = get_timer();
    wd_inode.nlink++;
    uint32_t wd_inode_sector = inodeidx2sector(wd_inode.ino);
    uint32_t wd_inode_offset = inodeidx2offset(wd_inode.ino);
    bios_sd_read(kva2pa(inode_buffer), 1, wd_inode_sector);
    inode_buffer[wd_inode_offset] = wd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, wd_inode_sector);

    // add dentry
    bzero((void *) dentry_buffer, BLOCK_SIZE);
    strcpy(dentry_buffer[0].name, ".");
    dentry_buffer[0].ino = inode_idx;
    dentry_buffer[0].type = IT_DIR;
    strcpy(dentry_buffer[1].name, "..");
    dentry_buffer[1].ino = wd_inode.ino;
    dentry_buffer[1].type = IT_DIR;
    bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, new_block);

    return 0;  // do_mkdir succeeds
}

void nest_rmdir(inode_t dir_inode) {
    // delete child
    for (int j = 2;j < BLOCK_SIZE / sizeof(dentry_t);j++) {
        bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir_inode.blocks[0]);
        if (dentry_buffer[j].name[0] != 0) {
            uint32_t inode_index = dentry_buffer[j].ino;
            uint32_t inode_sector = inodeidx2sector(inode_index);
            uint32_t inode_offset = inodeidx2offset(inode_index);
            bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
            inode_t child_inode = inode_buffer[inode_offset];
            if (child_inode.type == IT_DIR) { // child dir
                nest_rmdir(child_inode);
                // delete child dentry
                free_inode(child_inode.ino);
            } else {    // child file
                child_inode.nlink--;
                if (child_inode.nlink == 0) {
                    free_inode(child_inode.ino);
                } else {
                    uint32_t inode_sector = inodeidx2sector(child_inode.ino);
                    uint32_t inode_offset = inodeidx2offset(child_inode.ino);
                    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
                    inode_buffer[inode_offset] = child_inode;
                    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
                }
            }
        }
    }
}



int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    inode_t *d_inode_p = find_inode(path, &wd_inode);
    if (d_inode_p == NULL) {
        return -1;
    } else if (d_inode_p->type != IT_DIR) {
        printk("[FS] ls: cannot remove '%s': Not a directory\n", path);
        return -1;
    }
    nest_rmdir(*d_inode_p);
    // update wd inode
    wd_inode.mtime = get_timer();
    wd_inode.nlink--;
    uint32_t wd_inode_sector = inodeidx2sector(wd_inode.ino);
    uint32_t wd_inode_offset = inodeidx2offset(wd_inode.ino);
    bios_sd_read(kva2pa(inode_buffer), 1, wd_inode_sector);
    inode_buffer[wd_inode_offset] = wd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, wd_inode_sector);
    // update wd dentry
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == d_inode_p->ino) {
            dentry_buffer[j].name[0] = 0;
            dentry_buffer[j].ino = 0;
            dentry_buffer[j].type = 0;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
            break;
        }
    }
    // update wd inode
    wd_inode.size -= sizeof(dentry_t);
    wd_inode.mtime = get_timer();
    wd_inode.nlink--;
    wd_inode_sector = inodeidx2sector(wd_inode.ino);
    wd_inode_offset = inodeidx2offset(wd_inode.ino);
    bios_sd_read(kva2pa(inode_buffer), 1, wd_inode_sector);
    inode_buffer[wd_inode_offset] = wd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, wd_inode_sector);
    // delete path inode
    free_inode(d_inode_p->ino);
    return 0;  // do_rmdir succeeds
}

inode_t *find_inode(char *path, inode_t *parent_inode)
{
    if (path[0] == 0) {
        return parent_inode;
    }
    // parse path, dir1/dir2/dir3
    char dir1[MAX_PATHNAME_LEN] = { 0 };
    char dir2[MAX_PATHNAME_LEN] = { 0 };
    char dir3[MAX_PATHNAME_LEN] = { 0 };
    parse_path(path, dir1, dir2, dir3);
    // find one level node
    uint32_t inode_idx = 0;
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, parent_inode->blocks[0]);
    int found = 0;
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, dir1) == 0) {
            inode_idx = dentry_buffer[j].ino;
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] find_inode: cannot access '%s': No such file or directory\n", path);
        return NULL;
    }
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t dir1_inode = inode_buffer[inode_offset];
    if (dir2[0] == 0) {
        return &inode_buffer[inode_offset];
    }
    // find two level node
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir1_inode.blocks[0]);
    found = 0;
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, dir2) == 0) {
            inode_idx = dentry_buffer[j].ino;
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] find_inode: cannot access '%s': No such file or directory\n", path);
        return NULL;
    }
    inode_sector = inodeidx2sector(inode_idx);
    inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t dir2_inode = inode_buffer[inode_offset];
    if (dir3[0] == 0) {
        return &inode_buffer[inode_offset];
    }
    // find three level node
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir2_inode.blocks[0]);
    found = 0;
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, dir3) == 0) {
            inode_idx = dentry_buffer[j].ino;
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] find_inode: cannot access '%s': No such file or directory\n", path);
        return NULL;
    }
    inode_sector = inodeidx2sector(inode_idx);
    inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    return &inode_buffer[inode_offset];
}






int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    // find directory inode
    inode_t *d_inode_p = find_inode(path, &wd_inode);
    if (d_inode_p == NULL) {
        printk("[FS] ls: cannot access '%s': No such file or directory\n", path);
        return -1;
    } else if (d_inode_p->type != IT_DIR) {
        printk("[FS] ls: cannot access '%s': Not a directory\n", path);
        return -1;
    }
    inode_t d_inode = *d_inode_p;
    if (option == 0) {
        // list directory
        bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, d_inode.blocks[0]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
            if (dentry_buffer[j].name[0] != 0) {
                printk("%s\n", dentry_buffer[j].name);
            }
        }
    } else {
        // list directory with details
        bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, d_inode.blocks[0]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
            if (dentry_buffer[j].name[0] != 0) {
                inode_t *inode = find_inode(dentry_buffer[j].name, &d_inode);
                assert(inode != NULL);
                printk("%c%c%c nlink: %d ino: %d size: %d atime: %d mtime: %d ctime: %d %s\n",
                    inode->type == IT_DIR ? 'd' : '-',
                    inode->mode & O_RDWR ? 'r' : '-',
                    inode->mode & O_RDWR ? 'w' : '-',
                    inode->nlink,
                    inode->ino,
                    inode->size,
                    inode->atime,
                    inode->mtime,
                    inode->ctime,
                    dentry_buffer[j].name);
            }
        }
    }
    return 0;  // do_ls succeeds
}

void nest_pwd(inode_t dir_inode) {
    if (dir_inode.ino == 0) {
        printk("/");
        return;
    }
    // find parent inode
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir_inode.blocks[0]);
    uint32_t parent_inode_idx = dentry_buffer[1].ino;
    uint32_t parent_inode_sector = inodeidx2sector(parent_inode_idx);
    uint32_t parent_inode_offset = inodeidx2offset(parent_inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, parent_inode_sector);
    inode_t parent_inode = inode_buffer[parent_inode_offset];
    nest_pwd(parent_inode);
    // find dir name
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, parent_inode.blocks[0]);  // not root
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == dir_inode.ino) {
            printk("%s/", dentry_buffer[j].name);
            return;
        }
    }
}



int do_pwd(void) {
    nest_pwd(wd_inode);
    printk("\n");
    return 0;  // do_pwd succeeds
}


int do_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_open
    inode_t *inode_p = find_inode(path, &wd_inode);
    if (inode_p == NULL) {
        printk("[FS] open: cannot access '%s': No such file or directory\n", path);
        return -1;
    } else if (inode_p->type == IT_DIR) {
        printk("[FS] open: cannot open '%s': Is a directory\n", path);
        return -1;
    }
    // find a free file descriptor
    int fd = -1;
    for (int i = 0; i < NUM_FDESCS; i++) {
        if (fdesc_array[i].valid == 0) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        printk("[FS] open: cannot open '%s': No space left on device\n", path);
        return -1;
    }
    // create file descriptor
    fdesc_array[fd].valid = 1;
    fdesc_array[fd].mode = mode;
    fdesc_array[fd].write_pos = 0;
    fdesc_array[fd].read_pos = 0;
    fdesc_array[fd].ino = inode_p->ino;

    return fd;  // return the id of file descriptor
}

int do_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_read
    // check fd
    if (fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].valid == 0) {
        printk("[FS] read: invalid file descriptor\n");
        return -1;
    } else if (fdesc_array[fd].mode == O_WRONLY) {
        printk("[FS] read: write only open\n");
        return -1;
    }
    //get inode
    uint32_t inode_idx = fdesc_array[fd].ino;
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t inode = inode_buffer[inode_offset];
    // read data
    uint32_t start_block = fdesc_array[fd].read_pos / BLOCK_SIZE;
    bios_sd_read(kva2pa(rdata_buffer), 1, blockid2sector(start_block, &inode));
    for (int i = 0;i < length;i++) {
        if (fdesc_array[fd].read_pos % BLOCK_SIZE == 0) {
            uint32_t block_id = fdesc_array[fd].read_pos / BLOCK_SIZE;
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, blockid2sector(block_id, &inode));
        }
        buff[i] = rdata_buffer[fdesc_array[fd].read_pos % BLOCK_SIZE];
        fdesc_array[fd].read_pos++;
    }
    // update inode
    inode.atime = get_timer();
    inode_buffer[inode_offset] = inode;
    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
    return length;  // return the length of trully read data
}
// inode do not update to disk in this function, it should be updated in do_write
// indirect block should update in this function
int alloc_inode_block(uint32_t block_idx, inode_t *inode) {
    if (block_idx < DIRECT_BLOCK_NUM) {
        inode->blocks[block_idx] = find_free_block();
        printl("<%d> alloc_data_block: %x\n", block_idx, inode->blocks[block_idx]);
        return 0;
    }
    if (block_idx < DIRECT_BLOCK_NUM + NBLOCK_LV1) {  // indirect block
        uint32_t block_idx1 = block_idx - DIRECT_BLOCK_NUM;
        if (inode->blocks[DIRECT_BLOCK_NUM] == 0) {
            inode->blocks[DIRECT_BLOCK_NUM] = find_free_block();
            printl("<%d> alloc_block_lv1: %x\n", block_idx, inode->blocks[DIRECT_BLOCK_NUM]);
        }
        uint32_t new_block = find_free_block();
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM]);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        block_lv1[block_idx1] = new_block;
        bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM]);
        printl("<%d> alloc_data_block: (%d) %x\n", block_idx, block_idx1, new_block);
        return 0;
    }
    if (block_idx < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2) {  // double indirect block
        uint32_t block_idx2 = (block_idx - DIRECT_BLOCK_NUM - NBLOCK_LV1) / (NBLOCK_LV1);
        uint32_t block_idx1 = (block_idx - DIRECT_BLOCK_NUM - NBLOCK_LV1) % (NBLOCK_LV1);
        if (inode->blocks[DIRECT_BLOCK_NUM + 1] == 0) {
            inode->blocks[DIRECT_BLOCK_NUM + 1] = find_free_block();
            printl("<%d> alloc_block_lv2: %x\n", block_idx, inode->blocks[DIRECT_BLOCK_NUM + 1]);
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 1]);
        uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
        if (block_lv2[block_idx2] == 0) {
            uint32_t new_block = find_free_block();
            printl("<%d> alloc_block_lv1: %x\n", block_idx, new_block);
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 1]);
            block_lv2[block_idx2] = new_block;
            bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 1]);
        }
        uint32_t block_lv1_sector = block_lv2[block_idx2];
        uint32_t new_block = find_free_block();
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sector);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        block_lv1[block_idx1] = new_block;
        bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sector);
        printl("<%d> alloc_data_block: (%d, %d)%x\n", block_idx, block_idx1, block_idx2, new_block);
        return 0;
    }
    if (block_idx < DIRECT_BLOCK_NUM + NBLOCK_LV1 + NBLOCK_LV2 + NBLOCK_LV3) {  // triple indirect block
        uint32_t block_idx3 = (block_idx - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2) / NBLOCK_LV2;
        uint32_t block_idx2 = (block_idx - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2 - block_idx3 * NBLOCK_LV2) / NBLOCK_LV1;
        uint32_t block_idx1 = (block_idx - DIRECT_BLOCK_NUM - NBLOCK_LV1 - NBLOCK_LV2) % NBLOCK_LV1;
        if (inode->blocks[DIRECT_BLOCK_NUM + 2] == 0) {
            inode->blocks[DIRECT_BLOCK_NUM + 2] = find_free_block();
            printl("<%d> alloc_block_lv3: %x\n", block_idx, inode->blocks[DIRECT_BLOCK_NUM + 2]);
        }
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 2]);
        uint32_t *block_lv3 = (uint32_t *) rdata_buffer;
        if (block_lv3[block_idx3] == 0) {
            uint32_t new_block = find_free_block();
            printl("<%d> alloc_block_lv2: %x\n", block_idx, new_block);
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 2]);
            block_lv3[block_idx3] = new_block;
            bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[DIRECT_BLOCK_NUM + 2]);
        }
        uint32_t block_lv2_sector = block_lv3[block_idx3];
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sector);
        uint32_t *block_lv2 = (uint32_t *) rdata_buffer;
        if (block_lv2[block_idx2] == 0) {
            uint32_t new_block = find_free_block();
            printl("<%d> alloc_block_lv1: %x\n", block_idx, new_block);
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sector);
            block_lv2[block_idx2] = new_block;
            bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv2_sector);
        }
        uint32_t block_lv1_sector = block_lv2[block_idx2];
        uint32_t new_block = find_free_block();
        printl("<%d> alloc_data_block: (%d, %d, %d) , %x\n", block_idx, block_idx1, block_idx2, block_idx3, new_block);
        bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sector);
        uint32_t *block_lv1 = (uint32_t *) rdata_buffer;
        block_lv1[block_idx1] = new_block;
        bios_sd_write(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_lv1_sector);
        return 0;
    }
    printk("[FS] alloc_inode_block: too many blocks\n");
    return -1;

}
int do_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_write
    uint32_t inode_idx = fdesc_array[fd].ino;
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t *inode = &inode_buffer[inode_offset];

    // write data
    uint32_t start_block = fdesc_array[fd].write_pos / BLOCK_SIZE;
    uint32_t start_block_sec = blockid2sector(start_block, inode);
    if (start_block_sec == 0) {
        alloc_inode_block(start_block, inode);
        start_block_sec = blockid2sector(start_block, inode);
    }
    bios_sd_read(kva2pa(wdata_buffer), BLOCK_SIZE / SECTOR_SIZE, start_block_sec);
    for (int i = 0; i < length; i++) {
        if (fdesc_array[fd].write_pos % BLOCK_SIZE == 0) {
            uint32_t block_id = fdesc_array[fd].write_pos / BLOCK_SIZE;
            uint32_t block_sector = blockid2sector(block_id, inode);
            if (block_sector == 0) {
                alloc_inode_block(block_id, inode);
                block_sector = blockid2sector(block_id, inode);
            }
            bios_sd_read(kva2pa(wdata_buffer), BLOCK_SIZE / SECTOR_SIZE, block_sector);
        }
        uint32_t offset = fdesc_array[fd].write_pos % BLOCK_SIZE;
        wdata_buffer[offset] = buff[i];
        if (offset == BLOCK_SIZE - 1) {
            bios_sd_write(kva2pa(wdata_buffer), BLOCK_SIZE / SECTOR_SIZE, blockid2sector(fdesc_array[fd].write_pos / BLOCK_SIZE, inode));
        }
        fdesc_array[fd].write_pos++;
    }
    if (fdesc_array[fd].write_pos % BLOCK_SIZE != 0) {

        bios_sd_write(kva2pa(wdata_buffer), BLOCK_SIZE / SECTOR_SIZE, blockid2sector(fdesc_array[fd].write_pos / BLOCK_SIZE, inode));
    }
    // update inode
    if (fdesc_array[fd].write_pos > inode->size) {
        inode->size = fdesc_array[fd].write_pos;
    }
    inode->mtime = inode->atime = get_timer();
    bios_sd_write(kva2pa(inode_buffer), BLOCK_SIZE / SECTOR_SIZE, inode_sector);
    return length;  // return the length of trully written data
}

int do_close(int fd)
{
    // TODO [P6-task2]: Implement do_close
    fdesc_array[fd].valid = 0;
    return 0;  // do_close succeeds
}
void parse_path(char *path, char *dir1, char *dir2, char *dir3) {
    int i = 0;
    while (path[i] != '/' && path[i] != 0) {
        dir1[i] = path[i];
        i++;
    }
    dir1[i] = 0;
    if (path[i] != 0) {
        i++;
        int j = 0;
        while (path[i] != '/' && path[i] != 0) {
            dir2[j] = path[i];
            i++;
            j++;
        }
        dir2[j] = 0;
        if (path[i] != 0) {
            i++;
            int k = 0;
            while (path[i] != '/' && path[i] != 0) {
                dir3[k] = path[i];
                i++;
                k++;
            }
            dir3[k] = 0;
        } else {
            dir3[0] = 0;
        }
    } else {
        dir2[0] = 0;
        dir3[0] = 0;
    }
}
void add_file_dentry(inode_t *parent_inode, char *path, uint32_t ino) {
    // parse path, dir1/dir2/dir3
    char dir1[MAX_PATHNAME_LEN];
    char dir2[MAX_PATHNAME_LEN];
    char dir3[MAX_PATHNAME_LEN];
    parse_path(path, dir1, dir2, dir3);
    // level 1
    if (dir2[0] == 0) { // filename.txt
        bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, parent_inode->blocks[0]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
            if (dentry_buffer[j].ino == 0 && dentry_buffer[j].name[0] == 0) {
                strcpy(dentry_buffer[j].name, dir1);
                dentry_buffer[j].ino = ino;
                dentry_buffer[j].type = IT_FILE;
                bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, parent_inode->blocks[0]);
                return;
            }
        }
        printk("[FS] add_dentry: cannot create file '%s': no dentry\n", path);
        return;
    }
    // level 2
    uint32_t dir1_inode_idx;
    int found = 0;
    uint8_t type;
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, parent_inode->blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, dir1) == 0) {
            dir1_inode_idx = dentry_buffer[j].ino;
            found = 1;
            type = dentry_buffer[j].type;
            break;
        }
    }
    if (!found || type != IT_DIR) {
        printk("[FS] add_dentry: cannot create file '%s': No such directory\n", path);
        return;
    }
    uint32_t dir1_inode_sector = inodeidx2sector(dir1_inode_idx);
    uint32_t dir1_inode_offset = inodeidx2offset(dir1_inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, dir1_inode_sector);
    inode_t dir1_inode = inode_buffer[dir1_inode_offset];
    if (dir3[0] == 0) {     // dir1/filename
        bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir1_inode.blocks[0]);
        for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
            if (dentry_buffer[j].ino == 0 && dentry_buffer[j].name[0] == 0) {
                strcpy(dentry_buffer[j].name, dir2);
                dentry_buffer[j].ino = ino;
                dentry_buffer[j].type = IT_FILE;
                bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir1_inode.blocks[0]);
                return;
            }
        }
        printk("[FS] add_dentry: cannot create file '%s': no dentry\n", path);
        return;
    }
    // level 3
    uint32_t dir2_inode_idx;
    found = 0;
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir1_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, dir2) == 0) {
            dir2_inode_idx = dentry_buffer[j].ino;
            found = 1;
            type = dentry_buffer[j].type;
            break;
        }
    }
    if (!found || type != IT_DIR) {
        printk("[FS] add_dentry: cannot create file '%s': No such directory\n", path);
        return;
    }
    uint32_t dir2_inode_sector = inodeidx2sector(dir2_inode_idx);
    uint32_t dir2_inode_offset = inodeidx2offset(dir2_inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, dir2_inode_sector);
    inode_t dir2_inode = inode_buffer[dir2_inode_offset];
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir2_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == 0 && dentry_buffer[j].name[0] == 0) {
            strcpy(dentry_buffer[j].name, dir3);
            dentry_buffer[j].ino = ino;
            dentry_buffer[j].type = IT_FILE;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, dir2_inode.blocks[0]);
            return;
        }
    }
}
int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    inode_t *src_inode_p = find_inode(src_path, &wd_inode);
    if (src_inode_p == NULL) {
        printk("[FS] ln: cannot access '%s': No such file or directory\n", src_path);
        return -1;
    } else if (src_inode_p->type == IT_DIR) {
        printk("[FS] ln: cannot link '%s': Is a directory\n", src_path);
        return -1;
    }
    inode_t src_inode = *src_inode_p;
    add_file_dentry(&wd_inode, dst_path, src_inode.ino);
    // update src inode
    bios_sd_read(kva2pa(inode_buffer), 1, inodeidx2sector(src_inode.ino));
    inode_buffer[inodeidx2offset(src_inode.ino)].nlink++;
    bios_sd_write(kva2pa(inode_buffer), 1, inodeidx2sector(src_inode.ino));
    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    inode_t *inode_p = find_inode(path, &wd_inode);
    if (inode_p == NULL) {
        printk("[FS] rm: cannot access '%s': No such file or directory\n", path);
        return -1;
    } else if (inode_p->type == IT_DIR) {
        printk("[FS] rm: cannot remove '%s': Is a directory\n", path);
        return -1;
    }
    // delete parent dentry, assert wd is the parent inode
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == inode_p->ino) {
            dentry_buffer[j].name[0] = 0;
            dentry_buffer[j].ino = 0;
            dentry_buffer[j].type = 0;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
            break;
        }
    }
    // update wd inode
    wd_inode.size -= sizeof(dentry_t);
    wd_inode.mtime = get_timer();
    wd_inode.nlink--;
    uint32_t wd_inode_sector = inodeidx2sector(wd_inode.ino);
    uint32_t wd_inode_offset = inodeidx2offset(wd_inode.ino);
    bios_sd_read(kva2pa(inode_buffer), 1, wd_inode_sector);
    inode_buffer[wd_inode_offset] = wd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, wd_inode_sector);
    // update inode
    inode_p->nlink--;
    if (inode_p->nlink == 0) {
        free_inode(inode_p->ino);
    } else {
        bios_sd_write(kva2pa(inode_buffer), 1, inodeidx2sector(inode_p->ino));
    }

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
    if (fd < 0 || fd >= NUM_FDESCS || fdesc_array[fd].valid == 0) {
        printk("[FS] lseek: invalid file descriptor\n");
        return -1;
    }
    uint32_t inode_idx = fdesc_array[fd].ino;
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t *inode = &inode_buffer[inode_offset];
    if (whence == SEEK_SET) {
        fdesc_array[fd].read_pos = fdesc_array[fd].write_pos = offset;
    } else if (whence == SEEK_CUR) {
        fdesc_array[fd].read_pos += offset;
        fdesc_array[fd].write_pos += offset;
    } else if (whence == SEEK_END) {
        fdesc_array[fd].read_pos = fdesc_array[fd].write_pos = inode->size + offset;
    } else {
        printk("[FS] lseek: invalid whence\n");
        return -1;
    }


    return 0;  // the resulting offset location from the beginning of the file
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    if (strlen(path) >= 27) {
        printk("[FS] touch: cannot create file '%s': File name too long\n", path);
        return -1;
    }
    // search wd whether has the same name
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (strcmp(dentry_buffer[j].name, path) == 0) {
            printk("[FS] touch: cannot create file '%s': File exists\n", path);
            return -1;
        }
    }
    // find a free inode
    uint32_t inode_idx = find_free_inode();
    if (inode_idx == -1) {
        printk("[FS] touch: cannot create file '%s': No space left on device\n", path);
        return -1;
    }
    // create inode
    uint32_t inode_sector = inodeidx2sector(inode_idx);
    uint32_t inode_offset = inodeidx2offset(inode_idx);
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t *inode = &inode_buffer[inode_offset];
    bzero(inode, sizeof(inode_t));
    inode->mode = O_RDWR;
    inode->size = 0;
    inode->atime = inode->mtime = inode->ctime = get_timer();
    inode->ino = inode_idx;
    inode->nlink = 1;
    inode->type = IT_FILE;
    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
    // update wd dentry
    int found = 0;
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == 0 && dentry_buffer[j].name[0] == 0) {
            strcpy(dentry_buffer[j].name, path);
            dentry_buffer[j].ino = inode_idx;
            dentry_buffer[j].type = IT_FILE;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, wd_inode.blocks[0]);
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] touch: cannot create file '%s': no dentry\n", path);
        return -1;
    }
    // update wd inode
    wd_inode.size += sizeof(dentry_t);
    wd_inode.mtime = get_timer();
    wd_inode.nlink++;
    uint32_t wd_inode_sector = inodeidx2sector(wd_inode.ino);
    uint32_t wd_inode_offset = inodeidx2offset(wd_inode.ino);
    bios_sd_read(kva2pa(inode_buffer), 1, wd_inode_sector);
    inode_buffer[wd_inode_offset] = wd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, wd_inode_sector);

    return 0;  // do_touch succeeds
}
int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    inode_t *inode_p = find_inode(path, &wd_inode);
    if (inode_p == NULL) {
        printk("[FS] cat: cannot access '%s': No such file or directory\n", path);
        return -1;
    } else if (inode_p->type == IT_DIR) {
        printk("[FS] cat: cannot open '%s': Is a directory\n", path);
        return -1;
    }
    // read data
    uint32_t max_len = inode_p->size > 200 ? 200 : inode_p->size;
    for (int i = 0; i < max_len; i++) {
        if (i % BLOCK_SIZE == 0) {
            uint32_t block_id = i / BLOCK_SIZE;
            bios_sd_read(kva2pa(rdata_buffer), BLOCK_SIZE / SECTOR_SIZE, blockid2sector(block_id, inode_p));
        }
        printk("%c", rdata_buffer[i % BLOCK_SIZE]);
    }

    return 0;  // do_cat succeeds
}