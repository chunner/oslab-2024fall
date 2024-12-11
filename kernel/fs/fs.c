#include <os/string.h>
#include <os/fs.h>
#include <os/kernel.h>
#include <pgtable.h>
#include <os/mm.h>
#include <os/time.h>

static fdesc_t fdesc_array[NUM_FDESCS];
static char superblock_buffer[SECTOR_SIZE];
static char inode_map[INODE_MAP_SIZE * SECTOR_SIZE];
static char block_map[BLOCK_MAP_SIZE * SECTOR_SIZE];
static inode_t inode_buffer[SECTOR_SIZE / sizeof(inode_t)];     // size = one sector, 512B
static dentry_t dentry_buffer[BLOCK_SIZE / sizeof(dentry_t)];  // size = one block, 4KB




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
                    bios_sd_write(kva2pa(block_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
                    return BLOCK2SECTOR(block_idx) + DATA_OFFSET;
                }
            }
        }
    }
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
    uint32_t root_inode_sector = INODE_OFFSET + ROUNDDOWN(root_inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    uint32_t root_inode_offset = root_inode_idx % (SECTOR_SIZE / sizeof(inode_t));
    bios_sd_read(kva2pa(inode_buffer), 1, root_inode_sector);
    inode_t *root_inode = &inode_buffer[root_inode_offset];
    root_inode->mode = O_RDWR;
    root_inode->size = 1;   // one block
    root_inode->atime = root_inode->mtime = root_inode->ctime = get_timer();
    root_inode->ino = root_inode_idx;
    root_inode->nlink = 1;
    root_inode->type = IT_DIR;
    root_inode->blocks[0] = find_free_block();
    bios_sd_write(kva2pa(inode_buffer), 1, root_inode_sector);
    // root dentry
    uint32_t root_dentry_sector = root_inode->blocks[0];
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, root_dentry_sector);   // one block
    strcpy(dentry_buffer[0].name, ".");
    dentry_buffer[0].ino = root_inode_idx;
    strcpy(dentry_buffer[1].name, "..");
    dentry_buffer[1].ino = root_inode_idx;
    bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, root_dentry_sector);
    printk("[FS] Filesystem initialized successfully!\n");
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
    uint32_t used_sectors = 1 + superblock->block_map_size + superblock->inode_map_size + superblock->inode_size;
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
                    used_sectors += BLOCK_SIZE / SECTOR_SIZE;
                }
            }
        }
    }
    printk("\t used sectors: %d/%d, start sector: %d\n", used_sectors, superblock->fs_size, superblock->fs_start_sec);
    printk("\t block map offset : %d, occupied sector: %d\n", superblock->block_map_offset, superblock->block_map_size);
    uint32_t used_inodes = 0;
    for (uint32_t byte_idx = 0; byte_idx < INODE_MAP_SIZE * SECTOR_SIZE; byte_idx++) {
        if (inode_map[byte_idx] != 0x00) {  // Check if there are any free bits in this byte
            // Traverse each bit in the current byte
            for (int bit = 0; bit < 8; bit++) {
                uint32_t inode_idx = byte_idx * 8 + bit;
                if ((inode_map[byte_idx] & (1 << bit))) {
                    used_inodes++;
                }
            }
        }
    }
    printk("\t inode map offset : %d, occupied sector: %d, used: %d/%d\n", superblock->inode_map_offset, superblock->inode_map_size, used_inodes, superblock->inode_size * (SECTOR_SIZE / sizeof(inode_t)));
    printk("\t inode offset : %d, occupied sector: %d\n", superblock->inode_offset, superblock->inode_size);
    printk("\t data offset : %d, occupied sector: %d\n", superblock->data_offset, DATA_SIZE);
    printk("\t inode entry size: %dB, dir entry size: %dB\n", superblock->inode_entry_size, superblock->dir_entry_size);
    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd

    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir


    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core

    return 0;  // do_ls succeeds
}

int do_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_open

    return 0;  // return the id of file descriptor
}

int do_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_read

    return 0;  // return the length of trully read data
}

int do_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_write

    return 0;  // return the length of trully written data
}

int do_close(int fd)
{
    // TODO [P6-task2]: Implement do_close

    return 0;  // do_close succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch

    return 0;  // do_touch succeeds
}
int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat

    return 0;  // do_cat succeeds
}