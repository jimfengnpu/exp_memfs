#include "ramfs.h"
#include "stdio.h"
#include "assert.h"
#include "protect.h"
#include "proc.h"
#include "const.h"
#include "global.h"
#include "proto.h"
#include "string.h"
#include "fs_const.h"
#include "fs_misc.h"
#include "fat32.h"
#include "fs.h"

static pRF_FAT RF_FAT_ROOT;
static pRF_CLU RF_DATA_ROOT;
extern struct file_desc f_desc_table[NR_FILE_DESC];

static int rf_find_first_free(){
	int i;
	for (i = 0; i < RF_NR_CLU;i++){
		if(*(RF_FAT_ROOT+i)==0)
			return i;
	}
	return -INSUFFICIENTSPACE;
}
static void rf_alloc_clu(int clu){
	*(RF_FAT_ROOT + clu) = MAX_UNSIGNED_INT;
	memset(RF_DATA_ROOT+clu,0,sizeof(RF_CLU));
}
static pRF_REC rf_write_record(u32 pClu, const char *name, u32 entClu, u32 type, u32 size)
{
	pRF_REC rec = (pRF_REC)(RF_DATA_ROOT+pClu);
	int i;
	for (i = 0; i < RF_NR_REC;i++){
		if(rec[i].record_type==RF_NONE){
			break;
		}
	}
	if(i==RF_NR_REC){
		i = rf_find_first_free();
		if(i<0)
			return NULL;
		rf_alloc_clu(i);
		RF_FAT_ROOT[pClu] = i;
		return rf_write_record(i, name, entClu, type, size);
	}
	strcpy(rec[i].name, name);
	rec[i].record_type = type;
	rec[i].size = size;
	rec[i].start_cluster = entClu;
	return rec+i;
}

//return fat record, NULL if not found
//"ram/dir1/dir2/file":
// ==>(vfs)get_index("ram/dir1/dir2/file"): get "ram" and pass "dir1/dir2/file" to funcs in ramfs
// ==>(ramfs) findpath("dir1/dir2/file") in root: entname: dir1 cont_path:"dir2/file":
// ==>... findpath("dir2/file") in dir1: entname:dir2 cont_path:"file":
// ==>... findpath("file") in dir2: entname:file spos = 0
static pRF_REC findpath(const char *path, u32 dir_clu, int flag, int find_type){
	char entname[RF_MX_ENT_NAME];
	char *pbuf;
	//in vfs the path like "ram/shell_0.bin" will be parsed and the first "/" removed
	//so in rf_* functions path is abspath without "/" at the beginning
	//in case the beginning occupy "/", the following added
	if(path[0]=='/')
		path++;
	int i, len = strlen(path), spos = 0; // parse path with "/"
	for (i = 0; i<len; i++)
	{
		if(path[i] == '/')
			break;
		entname[i] = path[i];
	}
	entname[i] = '\0';
	if (i < len)
		spos = i + 1;
	//spos 0 indicates no / in path meet the end part of path
	//note: for record cluster it may exist inner none type record
	pRF_REC rec = (pRF_REC)(RF_DATA_ROOT + dir_clu);
	for (i = 0; i < RF_NR_REC; i++){
		if(rec[i].record_type!= RF_NONE&&strcmp(rec[i].name,entname)==0){
			if(spos==0&&rec[i].record_type==find_type)
				return rec + i;
			if(spos && rec[i].record_type==RF_D){
				return findpath(path + spos, rec[i].start_cluster, flag, find_type);
			}
		}
	}
	if(i==RF_NR_REC&&RF_FAT_ROOT[dir_clu]!=MAX_UNSIGNED_INT){
		return findpath(path, RF_FAT_ROOT[dir_clu], flag, find_type);//find in the next record page
	}
	//all not found, if flag contains O_CREAT, build an entry for it
	if(flag&O_CREAT){
		int ni;
		ni = rf_find_first_free();
		if(ni<0)
			return NULL;
		rf_alloc_clu(ni);
		if (spos)//dir
		{
			rf_write_record(dir_clu, entname, ni, RF_D, 0);
			// dir content
			rf_write_record(ni, ".", ni, RF_D, 0);//dir to itself
			rf_write_record(ni, "..", dir_clu, RF_D, 0);//dir to parent
			return findpath(path + spos, ni, flag, find_type);
		}
		else//file
		{
			if(find_type==RF_D){
				rf_write_record(ni, ".", ni, RF_D, 0);//dir to itself
				rf_write_record(ni, "..", dir_clu, RF_D, 0);//dir to parent
			}
			return rf_write_record(dir_clu, entname, ni, find_type, 0);
		}
		kprintf("create in %d: %s(%d)\n", dir_clu, entname, ni);
	}
	return NULL;
}

void init_ram_fs()
{
	RF_FAT_ROOT = (pRF_FAT)RAM_FS_BASE;
	RF_DATA_ROOT = (pRF_CLU)RAM_FS_DATA_BASE;
	rf_alloc_clu(0);
	rf_write_record(0, ".", 0, RF_D, 0);
	//in syscall we can only use 3G~3G+128M
	RF_FAT_ROOT = (pRF_FAT)K_PHY2LIN(RAM_FS_BASE);
	RF_DATA_ROOT = (pRF_CLU)K_PHY2LIN(RAM_FS_DATA_BASE);
}

//open and return fd
int rf_open(const char *path, int mode)
{
	/*caller_nr is the process number of the */
	int fd = -1;		/* return value */
	int name_len = strlen(path);
	char pathname[MAX_PATH];

	memcpy((void*)va2la(proc2pid(p_proc_current), pathname),(void*)va2la(proc2pid(p_proc_current), (void*)path), name_len);
	pathname[name_len] = 0;

	/* find a free slot in PROCESS::filp[] */
	int i;
	for (i = 0; i < NR_FILES; i++) { //modified by mingxuan 2019-5-20 cp from fs.c
		if (p_proc_current->task.filp[i] == 0) {
			fd = i;
			break;
		}
	}

	assert(0 <= fd && fd < NR_FILES);

	/* find a free slot in f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		//modified by mingxuan 2019-5-17
		if (f_desc_table[i].flag == 0)
			break;
	
	assert(i < NR_FILE_DESC);

	pRF_REC fd_ram = findpath(path, 0, mode, RF_F);//from root
	if(fd_ram){
		/* connects proc with file_descriptor */
		p_proc_current->task.filp[fd] = &f_desc_table[i];
		
		f_desc_table[i].flag = 1;	//added by mingxuan 2019-5-17

		/* connects file_descriptor with inode */
		f_desc_table[i].fd_node.fd_ram = fd_ram;	//modified by mingxuan 2019-5-17
		f_desc_table[i].dev_index = 5;
		f_desc_table[i].fd_mode = mode;
		f_desc_table[i].fd_pos = 0;
	}
	else
	{
		return -1;
	}
	return fd;
}

int rf_close(int fd)
{
	p_proc_current->task.filp[fd]->fd_node.fd_ram = 0;
	p_proc_current->task.filp[fd]->flag = 0;
	p_proc_current->task.filp[fd] = 0;
	return OK;
}


// //return bytes have been read
// int rf_read(int fd, void *buf, int length)
// {
// 	if (!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
// 		return -1;
// 	pRF_REC frec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
// 	int pos = p_proc_current->task.filp[fd]->fd_pos;
// 	int fat_offset = pos / RAM_FS_CLUSTER_SIZE;
// 	int cluster = frec->start_cluster;
// 	while(RF_FAT_ROOT[cluster]!=MAX_UNSIGNED_INT){
// 		if(fat_offset==0)
// 			break;
// 		fat_offset--;
// 		cluster = RF_FAT_ROOT[cluster];
// 	}
// 	pRF_CLU clu_data = RF_DATA_ROOT + cluster;
// 	char *rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
// 	int pref = 0, last_pref = 0;
// 	//end of expected buf later than current cluster
// 	while ((char *)(clu_data + 1) < rf_data + length-last_pref)
// 	{
// 		int pref = (char *)(clu_data + 1) - (rf_data + last_pref);//part in this cluster
// 		memcpy((void*)va2la(proc2pid(p_proc_current), buf + last_pref),rf_data, pref);
// 		if(RF_FAT_ROOT[cluster]==MAX_UNSIGNED_INT){
// 			rf_data = 0;
// 			break;
// 		}
// 		last_pref += pref;
// 		cluster = RF_FAT_ROOT[cluster];
// 		clu_data = RF_DATA_ROOT + cluster;
// 		rf_data = (char *)clu_data;
// 	}
// 	if(length>last_pref&& rf_data)
// 	memcpy((void*)va2la(proc2pid(p_proc_current), buf+last_pref), rf_data, length-last_pref);
// 	return last_pref;
// }

// 先前写得rf_read没有更新pos，并且在跨扇区读入有问题
// 许安杰重写了这一部分
int rf_read(int fd, void *buf, int length)
{
	if (!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
		return -1;
	pRF_REC frec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	int fat_offset = pos / RAM_FS_CLUSTER_SIZE;
	int cluster = frec->start_cluster;
	while(RF_FAT_ROOT[cluster] != MAX_UNSIGNED_INT){
		if(fat_offset==0)
			break;
		fat_offset--;
		cluster = RF_FAT_ROOT[cluster];
	}
	pRF_CLU clu_data = RF_DATA_ROOT + cluster;
	char *rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
	int bytes_read = 0;
	while(bytes_read < length && cluster != MAX_UNSIGNED_INT) {
		int cluster_can_read = min(length - bytes_read, 512 - (pos % RAM_FS_CLUSTER_SIZE));
		assert(cluster_can_read >= 0 && cluster_can_read <= 512);
		memcpy((void*)va2la(proc2pid(p_proc_current), buf + bytes_read), rf_data, cluster_can_read);
		bytes_read += cluster_can_read;
		pos += cluster_can_read;
		cluster = RF_FAT_ROOT[cluster];
		clu_data = RF_DATA_ROOT + cluster;
		rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
	}
	// *((char*) buf + bytes_read) = '\0';
	p_proc_current->task.filp[fd]->fd_pos = pos;
	return bytes_read;
}


//return bytes have been write
int rf_write(int fd, const void *buf, int length)
{
	if (!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
		return -1;
	pRF_REC frec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	int fat_offset = pos / RAM_FS_CLUSTER_SIZE;
	int cluster = frec->start_cluster;
	while(cluster!=MAX_UNSIGNED_INT){
		if(fat_offset==0)
			break;
		fat_offset--;
		if(RF_FAT_ROOT[cluster]==MAX_UNSIGNED_INT){
			int ni = rf_find_first_free();
			assert(ni >= 0);
			rf_alloc_clu(ni);
			RF_FAT_ROOT[cluster] = ni;
		}
		cluster = RF_FAT_ROOT[cluster];
	}
	pRF_CLU clu_data = RF_DATA_ROOT + cluster;
	char *rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
	int pref = 0, last_pref = 0;
	//end of expected buf later than current cluster
	while ((char *)(clu_data + 1) < rf_data + length-last_pref)
	{
		int pref = (char *)(clu_data + 1) - (rf_data + last_pref);//part in this cluster
		memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf + last_pref), pref);
		if(RF_FAT_ROOT[cluster]==MAX_UNSIGNED_INT){
			int ni = rf_find_first_free();
			assert(ni >= 0);
			rf_alloc_clu(ni);
			RF_FAT_ROOT[cluster] = ni;
		}
		last_pref += pref;
		cluster = RF_FAT_ROOT[cluster];
		clu_data = RF_DATA_ROOT + cluster;
		rf_data = (char *)clu_data;
	}
	memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf+last_pref), length-pref);
	frec->size = pos + length;
	p_proc_current->task.filp[fd]->fd_pos += length;
	return length;
}

int rf_lseek(int fd, int offset, int whence)
{
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	//int f_size = p_proc_current->task.filp[fd]->fd_inode->i_size; //deleted by mingxuan 2019-5-17
	int f_size = p_proc_current->task.filp[fd]->fd_node.fd_ram->size; //modified by mingxuan 2019-5-17

	switch (whence) {
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_CUR:
		pos += offset;
		break;
	case SEEK_END:
		pos = f_size + offset;
		break;
	default:
		return -1;
		break;
	}
	if ((pos > f_size) || (pos < 0)) {
		return -1;
	}
	p_proc_current->task.filp[fd]->fd_pos = pos;
	return pos;
}

int rf_create(const char *pathname)
{
	return findpath(pathname, 0, O_CREAT, RF_F)!=NULL?OK:-1;
}

int rf_createDir(const char *dirname)
{
	return findpath(dirname, 0, O_CREAT, RF_D)!=NULL?OK:-1;
}

int rf_openDir(const char *dirname)
{
	return OK;
}

int rf_delete(const char *filename)
{
	pRF_REC pREC = findpath(filename, 0, 0, RF_F);
	if(pREC){
		pREC->record_type = RF_NONE;
		u32 clu = pREC->start_cluster,nxt_clu=0;
		while(nxt_clu !=MAX_UNSIGNED_INT){
			nxt_clu = RF_FAT_ROOT[clu];
			RF_FAT_ROOT[clu] = 0;
			clu = nxt_clu;
		}
		return OK;
	}
	return -1;
}

int rf_deleteDir(const char *dirname)
{
	pRF_REC pREC = findpath(dirname, 0, 0, RF_D);
	if(pREC){
		pREC->record_type = RF_NONE;
		u32 clu = pREC->start_cluster,nxt_clu=0;
		while(nxt_clu !=MAX_UNSIGNED_INT){
			nxt_clu = RF_FAT_ROOT[clu];
			RF_FAT_ROOT[clu] = 0;
			clu = nxt_clu;
		}
		return OK;
	}
	return -1;
}
