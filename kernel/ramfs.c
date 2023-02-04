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
#include "vfs.h"
#include "fat32.h"
#include "fs.h"

static pRF_FAT RF_FAT_ROOT;
static pRF_CLU RF_DATA_ROOT;
extern struct file_desc f_desc_table[NR_FILE_DESC];

static int rf_find_first_free(){
	int i;
	for (i = 0; i < RAM_FS_NR_CLU;i++){
		if(*(RF_FAT_ROOT+i)==0)
			return i;
	}
	return -INSUFFICIENTSPACE;
}
static void rf_alloc_clu(int clu){
	*(RF_FAT_ROOT + clu) = MAX_UNSIGNED_INT;
	memset(RF_DATA_ROOT+clu, 0, sizeof(RF_CLU));
}
static pRF_REC rf_write_record(u32 pClu, const char *name, u32 entClu, u32 type, u32 size)
{
	pRF_REC rec = (pRF_REC)(RF_DATA_ROOT+pClu);
	int i;
	for (i = 0; i < RF_NR_REC;i++){
		if(rec[i].record_type == RF_NONE){
			break;
		}
	}
	if(i == RF_NR_REC){
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

static void init_dir_record(int dir_clu, int parent_dir)
{
	// dir content
	rf_write_record(dir_clu, ".", dir_clu, RF_D, 0);//dir to itself
	rf_write_record(dir_clu, "..", parent_dir, RF_D, 0);//dir to parent
}

// //return fat record, NULL if not found
// //"ram/dir1/dir2/file":
// // ==>(vfs)get_index("ram/dir1/dir2/file"): get "ram" and pass "dir1/dir2/file" to funcs in ramfs
// // ==>(ramfs) find_path("dir1/dir2/file") in root: entname: dir1 cont_path:"dir2/file":
// // ==>... find_path("dir2/file") in dir1: entname:dir2 cont_path:"file":
// // ==>... find_path("file") in dir2: entname:file spos = 0
// // just for understanding, have changed to iteration instead of recursion
// path: 文件路径; dir_clu: 路径起始文件夹索引; flag: 标志位; find_type: 查找类型(文件/文件夹)
// static pRF_REC find_path(const char *path, u32 dir_clu, int flag, int find_type){
// 	char ent_name[RF_MX_ENT_NAME];
// 	//in vfs the path like "ram/shell_0.bin" will be parsed and the first "/" removed
// 	//so in rf_* functions path is abspath without "/" at the beginning
// 	//in case the beginning occupy "/", the following added
// 	// if(path[0]=='/')
// 		// path++;
// 	//modify:23.1.15: change recursion to iteration
// 	int ipos=0, j, len = strlen(path), sep; // parse path with "/"
// 	while(ipos < len){
// 		sep = 0;
// 		for (j = 0; ipos < len; j++, ipos++)
// 		{
// 			// assert(len == strlen(path));
// 			assert(j < len);
// 			if(path[ipos] == '/')
// 				break;
// 			ent_name[j] = path[ipos];
// 		}
// 		ent_name[j] = '\0';
// 		if (ipos < len)
// 			sep = ++ipos;
// 		//spos 0 indicates no / in path meet the end part of path
// 		//note: for record cluster it may exist inner none type record
// 		pRF_REC rec = (pRF_REC)(RF_DATA_ROOT + dir_clu);
// 		int i;
// 		for (i = 0; i < RF_NR_REC; i++)
// 		{
// 			if(rec[i].record_type != RF_NONE && strcmp(rec[i].name, ent_name) == 0)
// 			{
// 				if(sep == 0 && rec[i].record_type == find_type)
// 					return rec + i;
// 				if(sep && rec[i].record_type == RF_D){
// 					break;
// 					// return findpath(path + spos, rec[i].start_cluster, flag, find_type);
// 				}
// 			}
// 		}
// 		if(i < RF_NR_REC) {
// 			// path += sep;
// 			dir_clu = rec[i].start_cluster;
// 			continue;
// 		}
// 		if(i == RF_NR_REC && RF_FAT_ROOT[dir_clu] != MAX_UNSIGNED_INT){
// 			dir_clu = RF_FAT_ROOT[dir_clu];
// 			continue;
// 			// return findpath(path, RF_FAT_ROOT[dir_clu], flag, find_type); // find in the next record page
// 		}
// 		//all not found, if flag contains O_CREAT, build an entry for it
// 		if(flag & O_CREAT){
// 			int inew;
// 			inew = rf_find_first_free();
// 			if(inew<0)
// 				return NULL;
// 			rf_alloc_clu(inew);
// 			if (sep)//dir
// 			{
// 				rf_write_record(dir_clu, ent_name, inew, RF_D, 0);
// 				path += sep;
// 				dir_clu = inew;
// 				continue;
// 				// return findpath(path + spos, ni, flag, find_type);
// 			}
// 			else//file
// 			{
// 				if(find_type == RF_D){
// 					rf_write_record(inew, ".", inew, RF_D, 0);//dir to itself
// 					rf_write_record(inew, "..", dir_clu, RF_D, 0);//dir to parent
// 				}
// 				return rf_write_record(dir_clu, ent_name, inew, find_type, 0);
// 			}
// 			kprintf("create in %d: %s(%d)\n", dir_clu, ent_name, inew);
// 		} else {
// 			return NULL;
// 		}
// 	}
// 	return NULL;
// }

/*
先前的find_path有几个寻址有点问题
1.找不到时，没返回NULL
2.若是创建文件时，未到文件末尾时，还会创建。若上层文件夹不存在时，应返回NULL
*/
// 先前没有ram文件夹时，可以直接用ram，现在得先创建ram文件夹再用了，mkdir默认ramfs。
static pRF_REC find_path(const char *path, u32 dir_clu, int flag, int find_type) {
	char ent_name[RF_MX_ENT_NAME];
	int pathpos = 0, j, len = strlen(path);
	while(pathpos < len) {
		for(j = 0; pathpos < len;j++, pathpos++) {
			if(path[pathpos] == '/') break;
			ent_name[j] = path[pathpos];
		}
		ent_name[j] = '\0';
		pRF_REC rec = (pRF_REC)(RF_DATA_ROOT + dir_clu);
		int i;
		for(i = 0; i < RF_NR_REC; i++) {
			if(rec[i].record_type == RF_NONE) continue;
			if(pathpos < len && rec[i].record_type == RF_D && strcmp(rec[i].name, ent_name) == 0) {
				dir_clu = rec[i].start_cluster;
				break;
			}
			if(pathpos == len && rec[i].record_type != RF_NONE && strcmp(rec[i].name, ent_name) == 0) {
				if(rec[i].record_type == find_type) return rec + i;
				else return NULL;
			}
		}
		if(pathpos < len && i == RF_NR_REC) return NULL;
		else if(pathpos < len) { // 表示找到了目录，但是还没到文件末尾
			pathpos++;
			continue;
		}
		if(pathpos == len && i == RF_NR_REC) {
			if(flag & O_CREAT) {
				int inew = rf_find_first_free();
				if(inew < 0) return NULL;
				rf_alloc_clu(inew);
				if(find_type == RF_D) {
					rf_write_record(inew, ".", inew, RF_D, 0);
					rf_write_record(inew, "..", dir_clu, RF_D, 0);
				}
				return rf_write_record(dir_clu, ent_name, inew, find_type, 0);
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}


void init_ram_fs()
{
	//in syscall we can only use 3G~3G+128M so we init the two at the beginning
	RF_FAT_ROOT = (pRF_FAT)K_PHY2LIN(RAM_FS_BASE);
	RF_DATA_ROOT = (pRF_CLU)K_PHY2LIN(RAM_FS_DATA_BASE);
	rf_alloc_clu(0);
	rf_write_record(0, ".", 0, RF_D, 0);
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

	pRF_REC fd_ram = find_path(path, 0, mode, RF_F);//from root
	if(fd_ram) {
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
// ramfs read
int rf_read(int fd, void *buf, int length)
{
	if(!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
		return -1; // 权限检查
	pRF_REC pf_rec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
	int cur_pos = p_proc_current->task.filp[fd]->fd_pos;
	int cur_nr_clu = cur_pos / RAM_FS_CLUSTER_SIZE; // 得到当前是第几个簇
	assert(cur_pos < pf_rec->size); // 偏移指针肯定小于文件大小
	// 得到当前的起始簇
	int cnt_clu = 0;
	int st_clu = pf_rec->start_cluster;
	while(cnt_clu != cur_nr_clu) {
		cnt_clu++;
		st_clu = RF_FAT_ROOT[st_clu];
	}
	assert(st_clu != MAX_UNSIGNED_INT); // 不可能是-1
	pRF_CLU clu_data = RF_DATA_ROOT + st_clu;
	char *rf_data = (char *)clu_data + cur_pos % RAM_FS_CLUSTER_SIZE;
	int bytes_read = 0;
	while(bytes_read < length && st_clu != MAX_UNSIGNED_INT) {
		int bytes_left = (char *)(clu_data + 1) - rf_data;
		int bytes_to_read = min(bytes_left, length - bytes_read);
		bytes_to_read = min(bytes_to_read, pf_rec->size - cur_pos);
		memcpy((void *)va2la(proc2pid(p_proc_current), buf + bytes_read), rf_data, bytes_to_read);
		bytes_read += bytes_to_read;
		st_clu = RF_FAT_ROOT[st_clu];
		clu_data = RF_DATA_ROOT + st_clu;
		rf_data = (char *)clu_data;
	}
	// 返回的字节数要么是length，要么是文件剩余的字节数
	assert(bytes_read == length || bytes_read == pf_rec->size - p_proc_current->task.filp[fd]->fd_pos);
	p_proc_current->task.filp[fd]->fd_pos += bytes_read; // 更新偏移指针
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
	while(cluster != MAX_UNSIGNED_INT){
		if(fat_offset==0)
			break;
		fat_offset--;
		if(RF_FAT_ROOT[cluster] == MAX_UNSIGNED_INT){
			int inew = rf_find_first_free();
			assert(inew >= 0);
			rf_alloc_clu(inew);
			RF_FAT_ROOT[cluster] = inew;
		}
		cluster = RF_FAT_ROOT[cluster];
	}
	pRF_CLU clu_data = RF_DATA_ROOT + cluster;
	char *rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
	int bytes_write = 0;
	//end of expected buf later than current cluster
	while ((char *)(clu_data + 1) < rf_data + length-bytes_write)
	{
		int pref = (char *)(clu_data + 1) - rf_data;//part in this cluster, fixed 23.1.10 jf
		memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf + bytes_write), pref);
		bytes_write += pref;
		if(RF_FAT_ROOT[cluster] == MAX_UNSIGNED_INT){
			int inew = rf_find_first_free();
			assert(inew >= 0);
			rf_alloc_clu(inew);
			RF_FAT_ROOT[cluster] = inew;
		}
		cluster = RF_FAT_ROOT[cluster];
		clu_data = RF_DATA_ROOT + cluster;
		rf_data = (char *)clu_data;
	}
	memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf+bytes_write), length - bytes_write);
	pos += length;
	p_proc_current->task.filp[fd]->fd_pos = pos;
	if(pos > frec->size) frec->size = pos;
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
	return find_path(pathname, 0, O_CREAT, RF_F) != NULL ? OK : -1;
}

int rf_create_dir(const char *dirname)
{
	return find_path(dirname, 0, O_CREAT, RF_D) != NULL ? OK : -1;
}

int rf_open_dir(const char *dirname)
{
	// 打开文件夹相当于打开目录文件，整体逻辑和rf_open类似
	int fd = -1;
	int name_len = strlen(dirname);
	if(name_len > MAX_PATH)
		return -1;
	// find a free slot in PROC::filp[]
	int i;
	for(i = 0;i < NR_FILES;i++) {
		if(p_proc_current->task.filp[i] == 0) {
			fd = i;
			break;
		}
	}
	if(i == NR_FILES)
		return -1;
	for(i = 0;i < NR_FILE_DESC;i++)
		if(f_desc_table[i].flag == 0)
			break;
	if(i == NR_FILE_DESC)
		return -1;
	pRF_REC fd_ram = find_path(dirname, 0, O_RDWR, RF_D);
	if(fd_ram == NULL)
		return -1;
	// update f_desc_table
	p_proc_current->task.filp[fd] = &f_desc_table[i];
	f_desc_table[i].flag = 1;
	f_desc_table[i].fd_node.fd_ram = fd_ram;
	f_desc_table[i].dev_index = VFS_INDEX_RAMFS;
	f_desc_table[i].fd_mode = O_RDWR; // 应该是只读，但目前文件flag还不完善
	f_desc_table[i].fd_pos = 0;
	return fd;
}

int rf_delete(const char *filename)
{
	pRF_REC pREC = find_path(filename, 0, 0, RF_F);
	if(pREC){
		pREC->record_type = RF_NONE;
		u32 clu = pREC->start_cluster, nxt_clu=0;
		while(nxt_clu != MAX_UNSIGNED_INT){
			nxt_clu = RF_FAT_ROOT[clu];
			RF_FAT_ROOT[clu] = 0;
			clu = nxt_clu;
		}
		return OK;
	}
	return -1;
}

int rf_delete_dir(const char *dirname)
{
	panic("todo: check empty in rf_delete_dir");
	pRF_REC pREC = find_path(dirname, 0, 0, RF_D);
	if(pREC){
		pREC->record_type = RF_NONE;
		u32 clu = pREC->start_cluster, nxt_clu=0;
		while(nxt_clu != MAX_UNSIGNED_INT){
			nxt_clu = RF_FAT_ROOT[clu];
			RF_FAT_ROOT[clu] = 0;
			clu = nxt_clu;
		}
		return OK;
	}
	return -1;
}
