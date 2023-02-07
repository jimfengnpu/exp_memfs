#ifndef RAMFS_H
#define RAMFS_H
#include "type.h"

#define RAM_FS_CLUSTER_SIZE 512	//512 bytes per cluster
#define RAM_FS_BASE 0x02000000		//memman end 32M
#define RAM_FS_NR_CLU (128*1024)
#define RF_MX_ENT_NAME	20

// 32M512k--96M512k data(dir record&file data)
// 64M/0.5k*4 = 512k FAT_NO_HEAD(0(start) as root not 2)
// fake FAT no strict entry format, just borrow the name
// rf_inode record_type
#define RF_NONE 0	//free
#define RF_F	1	//file
#define RF_D	2	//dir

typedef struct{
	char name[RF_MX_ENT_NAME];
	u32 record_type;
	u32 size;
	u32 start_cluster;
} rf_inode;
#define RF_NR_REC (RAM_FS_CLUSTER_SIZE/sizeof(rf_inode))

typedef union{
	rf_inode entry[RF_NR_REC];
	char data[RAM_FS_CLUSTER_SIZE];
} rf_clu;

typedef rf_inode *p_rf_inode;
typedef rf_clu *p_rf_clu;
typedef u32 rf_fat, *p_rf_fat;

#define RAM_FS_DATA_BASE	(RAM_FS_BASE + sizeof(rf_fat) * RAM_FS_NR_CLU)


#include "fs.h"
#include "fs_misc.h"
void init_ram_fs();
p_rf_inode find_path(const char *path, p_rf_inode dir_rec, int flag, int find_type);
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
#endif