#ifndef RAMFS_H
#define RAMFS_H
#include "type.h"
#include "memman.h"
#define RAM_FS_CLUSTER_SIZE 0x1000	//4k bytes per cluster
//memman end 32M set enough space for fat(index)
#define RAM_FS_NR_CLU (MEMEND / RAM_FS_CLUSTER_SIZE)
#define RF_MX_ENT_NAME	20

// FAT_NO_HEAD(0(start) as root not 2)
// fake FAT no strict entry format, just borrow the name
// rf_inode record_type
#define RF_NONE 0	//free
#define RF_F	1	//file
#define RF_D	2	//dir

typedef struct{
	char name[RF_MX_ENT_NAME];
	u32 record_type;
	u32 *size;
	u32 *link_cnt;
	u32 start_cluster;
} rf_inode, *p_rf_inode;
#define RF_NR_REC (RAM_FS_CLUSTER_SIZE/sizeof(rf_inode))

typedef union{
	rf_inode entry[RF_NR_REC];
	char data[RAM_FS_CLUSTER_SIZE];
} rf_clu, *p_rf_clu;

typedef struct{
	u32 next_cluster;
	u32 addr;
} rf_fat, *p_rf_fat;



#include "fs.h"
#include "fs_misc.h"
void init_ram_fs();
p_rf_inode find_path(const char *path, p_rf_inode dir_rec, int flag, int find_type, p_rf_inode p_fa);
int rf_open(const char *path, int mode);
int rf_close(int fd);
int rf_read(int fd, void *buf, int length);
int rf_write(int fd, const void *buf, int length);
int rf_lseek(int fd, int offset, int whence);
int rf_create(const char *filename);
int rf_create_dir(const char *dirname);
// int rf_open_dir(const char *dirname, struct dir_ent *dirent, int mx_ent);
int rf_open_dir(const char *dirname);
int rf_delete(const char *filename);
int rf_delete_dir(const char *dirname);
int rf_unlink(const char *path);
int rf_link(const char *oldpath, const char *newpath);

#endif