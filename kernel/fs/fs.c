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
static char block_map[BLOCK_MAP_SIZE * SECTOR_SIZE];
static inode_t inode_buffer[SECTOR_SIZE / sizeof(inode_t)];     // size = one sector, 512B
static dentry_t dentry_buffer[BLOCK_SIZE / sizeof(dentry_t)];  // size = one block, 4KB

static inode_t pwd_inode;


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
                    bios_sd_write(kva2pa(block_map), INODE_MAP_SIZE, INODE_MAP_OFFSET + FS_START_SECTOR);
                    return BLOCK2SECTOR(block_idx) + DATA_OFFSET;
                }
            }
        }
    }
    return -1;  // Not enough consecutive free pages found
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
    // bios_sd_read(kva2pa(inode_buffer), 1, root_inode_sector);
    bzero((void *) inode_buffer, SECTOR_SIZE);
    inode_t *root_inode = &inode_buffer[root_inode_offset];
    root_inode->mode = O_RDWR;
    root_inode->size = 1;   // one block
    root_inode->atime = root_inode->mtime = root_inode->ctime = get_timer();
    root_inode->ino = root_inode_idx;
    root_inode->nlink = 1;
    root_inode->type = IT_DIR;
    root_inode->blocks[0] = find_free_block();
    bios_sd_write(kva2pa(inode_buffer), 1, root_inode_sector);
    pwd_inode = *root_inode;
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
    assert(strlen(path) <= 27);
    // TODO [P6-task1]: Implement do_mkdir
    // search pwd whether has the same name
    assert(pwd_inode.size == 1);
    bios_sd_read(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, pwd_inode.blocks[0]);
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
    uint32_t inode_sector = INODE_OFFSET + ROUNDDOWN(inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    uint32_t inode_offset = inode_idx % (SECTOR_SIZE / sizeof(inode_t));
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    inode_t *inode = &inode_buffer[inode_offset];
    inode->mode = O_RDWR;
    inode->size = 1;   // one block
    inode->atime = inode->mtime = inode->ctime = get_timer();
    inode->ino = inode_idx;
    inode->nlink = 1;
    inode->type = IT_DIR;
    inode->blocks[0] = find_free_block();
    bios_sd_write(kva2pa(inode_buffer), 1, inode_sector);
    // update pwd dentry
    int found = 0;
    for (int j = 0; j < BLOCK_SIZE / sizeof(dentry_t); j++) {
        if (dentry_buffer[j].ino == 0 && dentry_buffer[j].name[0] == 0) {
            strcpy(dentry_buffer[j].name, path);
            dentry_buffer[j].ino = inode_idx;
            dentry_buffer[j].type = IT_DIR;
            bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, pwd_inode.blocks[0]);
            found = 1;
            break;
        }
    }
    if (!found) {
        printk("[FS] mkdir: cannot create directory '%s': no dentry\n", path);
        return -1;
    }
    // update pwd inode
    pwd_inode.mtime = get_timer();
    pwd_inode.nlink++;
    uint32_t pwd_inode_sector = INODE_OFFSET + ROUNDDOWN(pwd_inode.ino * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    uint32_t pwd_inode_offset = pwd_inode.ino % (SECTOR_SIZE / sizeof(inode_t));
    bios_sd_read(kva2pa(inode_buffer), 1, pwd_inode_sector);
    inode_buffer[pwd_inode_offset] = pwd_inode;
    bios_sd_write(kva2pa(inode_buffer), 1, pwd_inode_sector);

    // add dentry
    bzero((void *) dentry_buffer, BLOCK_SIZE);
    strcpy(dentry_buffer[0].name, ".");
    dentry_buffer[0].ino = inode_idx;
    dentry_buffer[0].type = IT_DIR;
    strcpy(dentry_buffer[1].name, "..");
    dentry_buffer[1].ino = pwd_inode.ino;
    dentry_buffer[1].type = IT_DIR;
    bios_sd_write(kva2pa(dentry_buffer), BLOCK_SIZE / SECTOR_SIZE, inode->blocks[0]);

    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

inode_t *find_inode(char *path, inode_t *parent_inode)
{
    if (path[0] == 0) {
        return parent_inode;
    }
    // parse path, dir1/dir2/dir3
    uint32_t inode_idx = 0;
    char dir1[MAX_PATHNAME_LEN] = { 0 };
    char dir2[MAX_PATHNAME_LEN] = { 0 };
    char dir3[MAX_PATHNAME_LEN] = { 0 };
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
        }
    }
    // find one level node
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
    uint32_t inode_sector = INODE_OFFSET + ROUNDDOWN(inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    uint32_t inode_offset = inode_idx % (SECTOR_SIZE / sizeof(inode_t));
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
    inode_sector = INODE_OFFSET + ROUNDDOWN(inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    inode_offset = inode_idx % (SECTOR_SIZE / sizeof(inode_t));
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
    inode_sector = INODE_OFFSET + ROUNDDOWN(inode_idx * sizeof(inode_t), SECTOR_SIZE) / SECTOR_SIZE;
    inode_offset = inode_idx % (SECTOR_SIZE / sizeof(inode_t));
    bios_sd_read(kva2pa(inode_buffer), 1, inode_sector);
    return &inode_buffer[inode_offset];
}







int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    // find directory inode
    inode_t *d_inode_p = find_inode(path, &pwd_inode);
    if (d_inode_p == NULL) {
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