#ifndef RAMFS_H
#define RAMFS_H
#include "type.h"
#define RAM_FS_CLUSTER_SIZE 512	//512 bytes per cluster
#define RAM_FS_BASE 0x02000000		//memman end 32M
#define RAM_FS_DATA_BASE	0x02080000
#define RF_NR_CLU ((RAM_FS_DATA_BASE-RAM_FS_BASE)/4)
#define RF_MX_ENT_NAME	20
//32M512k--96M512k data(dir record&file data)
//64M/0.5k*4 = 512k FAT_NO_HEAD(0(start) as root not 2)
//fake FAT no strict entry format, just borrow the name
#define RF_NONE 0
#define RF_F	1	//file
#define RF_D	2	//dir
typedef struct{
	char name[RF_MX_ENT_NAME];
	u32 record_type;
	u32 size;
	u32 start_cluster;
} RAM_FS_RECORD;
#define RF_NR_REC (RAM_FS_CLUSTER_SIZE/sizeof(RAM_FS_RECORD))
typedef union{
	RAM_FS_RECORD entry[RF_NR_REC];
	char data[RAM_FS_CLUSTER_SIZE];
} RF_CLU;
typedef RAM_FS_RECORD *pRF_REC;
typedef RF_CLU *pRF_CLU;
typedef u32 RF_FAT, *pRF_FAT;
void init_ram_fs();
int rf_open(const char *path, int mode);
int rf_close(int fd);
int rf_read(int fd, void *buf, int length);
int rf_write(int fd, const void *buf, int length);
int rf_lseef(int fd, int offset, int whence);
int rf_create(const char *filename);
int rf_createDir(const char *dirname);
int rf_openDir(const char *dirname);
int rf_delete(const char *filename);
int rf_deleteDir(const char *dirname);
#endif