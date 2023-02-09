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
#include "errno.h"

static p_rf_fat RF_FAT_ROOT;
 
extern struct file_desc f_desc_table[NR_FILE_DESC];

static int rf_alloc_clu(int clu){
	RF_FAT_ROOT[clu].next_cluster = MAX_UNSIGNED_INT;
	u32 phy_addr = do_malloc_4k();
	if(phy_addr == -1){
		return -1;
	}
	RF_FAT_ROOT[clu].addr = K_PHY2LIN(phy_addr);
	memset((void*)RF_FAT_ROOT[clu].addr, 0, sizeof(rf_clu));
	return 0;
}

static int rf_find_first_free_alloc(){
	int i;
	for (i = 0; i < RAM_FS_NR_CLU;i++){
		if(RF_FAT_ROOT[i].next_cluster == 0) {
			if(rf_alloc_clu(i) == -1)
				break;
			return i;
		}
	}
	return -ENOMEM;
}

static void rf_free_clu(int clu) {
	int nxt_clu = 0;
	while (nxt_clu != MAX_UNSIGNED_INT)
	{
		nxt_clu = RF_FAT_ROOT[clu].next_cluster;
		RF_FAT_ROOT[clu].next_cluster = 0;
		if(do_free_4k(K_LIN2PHY(RF_FAT_ROOT[clu].addr)) == -1) {
			panic("free error");
		}
		clu = nxt_clu;
	}
}
static u32 check_dir_size(u32 dir_clu)
{
	u32 dir_size = 0;
	while(RF_FAT_ROOT[dir_clu].next_cluster != MAX_UNSIGNED_INT) {
		dir_size += RAM_FS_CLUSTER_SIZE;
		dir_clu = RF_FAT_ROOT[dir_clu].next_cluster;
	}
	p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
	int i;
	for (i = RF_NR_REC - 1; i >= 0; i--) {
		if(rec[i].record_type != RF_NONE){
			break;
		}
	}
	dir_size += (i + 1)*sizeof(rf_inode);
	return dir_size;
}

//parent_rec: 待初始化的文件夹记录项(在上级目录文件数据中)
static void init_dir_record(int dir_clu, p_rf_inode parent_rec)
{
	// dir content
	// rf_write_record(dir_rec, ".", dir_rec->start_cluster, RF_D, 0);//dir to itself
	// rf_write_record(dir_rec, "..", parent_dir, RF_D, 0);//dir to parent
	p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
		
	strcpy(rec->name, ".");
	rec->record_type = RF_D;
	rec->size = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
	rec->link_cnt = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
	*rec->size = sizeof(rf_inode);
	*rec->link_cnt = 0;
	rec->start_cluster = dir_clu;
	if(parent_rec != NULL) {
		*rec->size += sizeof(rf_inode);
		rec++;
		strcpy(rec->name, "..");
		rec->record_type = RF_D;
		rec->size = parent_rec->size;
		rec->start_cluster = parent_rec->start_cluster;
		rec->link_cnt = parent_rec->link_cnt;
	}
}

//对于文件夹,传入的size无用
//dir_rec: 待写入的文件所在文件夹记录项(在上级目录文件数据中)
// p_fa表示是否有父亲inode，若为NULL则创建，否则指向父亲
static p_rf_inode rf_write_record(p_rf_inode dir_rec, const char *name, u32 entClu, u32 type, p_rf_inode p_fa)
{
	u32 dir_clu = 0;
	if(dir_rec != NULL) {
		dir_clu = dir_rec->start_cluster;
	}
	p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr); // 文件夹记录项
	int i, inew;
	for (i = 0; i < RF_NR_REC;){
		if(rec[i].record_type == RF_NONE){
			break;
		}
		i++;
		if(i == RF_NR_REC) {
			inew = rf_find_first_free_alloc();
			if(inew<0)
				return NULL;
			RF_FAT_ROOT[dir_clu].next_cluster = inew;
			// return rf_write_record(i, name, entClu, type, size);
			dir_clu = inew;
			i = 0;
		}
	}
	
	strcpy(rec[i].name, name);
	rec[i].record_type = type;
	u32 new_size = check_dir_size(dir_clu);
	*rec->size = new_size;
	if(p_fa != NULL) {
		rec[i].size = p_fa->size;
		rec[i].link_cnt = p_fa->link_cnt;
	}
	else if(type == RF_F) {
		rec[i].size = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32))); // 初次分配
		// *rec[i].size = size;
		*rec[i].size = 0;
		rec[i].link_cnt = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
		*rec[i].link_cnt = 0;
	}
	else if(type == RF_D) {
		init_dir_record(entClu, rec); // link_cnt已经维护
		rec[i].size = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32))); // 初次分配
		*rec[i].size = check_dir_size(entClu);
		rec[i].link_cnt = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
		*rec[i].link_cnt = 0;
	}
	rec[i].start_cluster = entClu;
	// pRF_REC parent = find_path("..", dir_clu, 0, RF_D);
	if(dir_rec != NULL) {
		*dir_rec->size = new_size;
	}
	return rec+i;
}


void init_ram_fs()
{
	//in syscall we can only use 3G~3G+128M so we init the two at the beginning
	RF_FAT_ROOT = (p_rf_fat)K_PHY2LIN(do_kmalloc(sizeof(rf_fat) * RAM_FS_NR_CLU));
	// RF_DATA_ROOT = (p_rf_clu)K_PHY2LIN(RAM_FS_DATA_BASE);
	rf_alloc_clu(0);
	init_dir_record(0, NULL);
}

/*
先前的find_path有几个寻址有点问题
1.找不到时，没返回NULL
2.若是创建文件时，未到文件末尾时，还会创建。若上层文件夹不存在时，应返回NULL
*/
//"ram/dir1/dir2/file":
// ==>(vfs)get_index("ram/dir1/dir2/file"): get "ram" and pass "dir1/dir2/file" to funcs in ramfs
// ==>(ramfs) find_path("dir1/dir2/file") in root: entname: dir1 cont_path:"dir2/file":
// ==>... find_path("dir2/file") in dir1: entname:dir2 cont_path:"file":
// ==>... find_path("file") in dir2: entname:file spos = 0
// just for understanding, have changed to iteration instead of recursion
// vfs调整后,ram默认存在(将fs_name作为一层文件夹)
// 参数变更: 从dir_rec记录表示的文件夹开始搜索,NULL则从RAMFS的根开始
// path: 文件路径; dir_rec: 路径起始文件夹记录; flag: 标志位; find_type: 查找类型(文件/文件夹); 
// p_fa是为了链接时使用，其余情况则为NULL
//return fat record, NULL if not found
p_rf_inode find_path(const char *path, p_rf_inode dir_rec, int flag, int find_type, p_rf_inode p_fa) 
{
	u32 dir_clu = 0;
	if(dir_rec != NULL) {
		dir_clu = dir_rec->start_cluster;
	}
	char ent_name[RF_MX_ENT_NAME];
	int pathpos = 0, j, len = strlen(path);
	while(pathpos < len) {
		for(j = 0; pathpos < len;j++, pathpos++) {
			if(path[pathpos] == '/') break;
			ent_name[j] = path[pathpos];
		}
		ent_name[j] = '\0';
		p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
		int i;
		for(i = 0; i < RF_NR_REC; i++) {
			if(rec[i].record_type == RF_NONE) continue;
			if(pathpos < len && rec[i].record_type == RF_D && strcmp(rec[i].name, ent_name) == 0) {
				*rec[i].size = check_dir_size(rec[i].start_cluster);
				dir_rec = rec + i;
				dir_clu = rec[i].start_cluster;
				break;
			}
			if(pathpos == len && rec[i].record_type != RF_NONE && strcmp(rec[i].name, ent_name) == 0) {
				if(rec[i].record_type == find_type) {
					if(rec[i].record_type == RF_D)
						*rec[i].size = check_dir_size(rec[i].start_cluster);
					return rec + i;
				}
				else return NULL;
			}
		}
		if(pathpos < len && i == RF_NR_REC) {
			if(RF_FAT_ROOT[dir_clu].next_cluster == MAX_UNSIGNED_INT)
				return NULL;
			dir_clu = RF_FAT_ROOT[dir_clu].next_cluster;
			continue;
		}
		else if(pathpos < len) { // 表示找到了目录，但是还没到文件末尾
			pathpos++;
			continue;
		}
		if(pathpos == len && i == RF_NR_REC) {
			if(flag & O_CREAT) {
				int inew = rf_find_first_free_alloc();
				if(inew < 0) return NULL;
				// if(find_type == RF_D) {
				// 	rf_write_record(inew, ".", inew, RF_D, 0);
				// 	rf_write_record(inew, "..", dir_clu, RF_D, 0);
				// } 
				//dir init move to write_record
				return rf_write_record(dir_rec, ent_name, inew, find_type, p_fa);
			} else {
				return NULL;
			}
		}
	}
	return NULL;
}


//open and return fd
int rf_open(const char *pathname, int flags)
{
	/*caller_nr is the process number of the */
	int fd = -1;		/* return value */
	int name_len = strlen(pathname);
	char path[MAX_PATH];

	memcpy((void*)va2la(proc2pid(p_proc_current), path),
	(void*)va2la(proc2pid(p_proc_current), (void*)pathname), name_len);
	path[name_len] = 0;

	/* find a free slot in PROCESS::filp[] */
	int i;
	for (i = 0; i < NR_FILES; i++) { //modified by mingxuan 2019-5-20 cp from fs.c
		if (p_proc_current->task.filp[i] == 0) {
			fd = i;
			break;
		}
	}
	if(i == NR_FILES) {
		return -EMFILE;
	}
	assert(0 <= fd && fd < NR_FILES);

	/* find a free slot in f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		//modified by mingxuan 2019-5-17
		if (f_desc_table[i].flag == 0)
			break;
	if(i == NR_FILE_DESC) {
		return -ENFILE;
	}
	assert(i < NR_FILE_DESC);

	p_rf_inode fd_ram = find_path(pathname, NULL, flags, RF_F, NULL);//from root
	if(fd_ram) {
		/* connects proc with file_descriptor */
		p_proc_current->task.filp[fd] = &f_desc_table[i];
		
		f_desc_table[i].flag = 1;	//added by mingxuan 2019-5-17

		/* connects file_descriptor with inode */
		f_desc_table[i].fd_node.fd_ram = fd_ram;	//modified by mingxuan 2019-5-17
		f_desc_table[i].dev_index = VFS_INDEX_RAMFS;
		f_desc_table[i].fd_mode = flags;
		f_desc_table[i].fd_pos = 0;
	}
	else
	{
		return -ENOENT;
	}
	return fd;
}

int rf_close(int fd)
{
	if(p_proc_current->task.filp[fd] == 0) {
		return -1;
	}
	p_proc_current->task.filp[fd]->fd_node.fd_ram = 0;
	p_proc_current->task.filp[fd]->flag = 0;
	p_proc_current->task.filp[fd] = 0;
	return 0;
}


// 先前写得rf_read没有更新pos，并且在跨扇区读入有问题

// ramfs read
// return bytes have been read
int rf_read(int fd, void *buf, int length)
{
	if(!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
		return -EACCES; // 权限检查
	p_rf_inode pf_rec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
	int cur_pos = p_proc_current->task.filp[fd]->fd_pos;
	int cur_nr_clu = cur_pos / RAM_FS_CLUSTER_SIZE; // 得到当前是第几个簇
	// assert(cur_pos <= pf_rec->size); // 偏移指针肯定小于文件大小 恰好到文件尾的时候，不该报assert吧
	// 得到当前的起始簇
	int cnt_clu = 0;
	int st_clu = pf_rec->start_cluster;
	while(cnt_clu != cur_nr_clu) {
		cnt_clu++;
		st_clu = RF_FAT_ROOT[st_clu].next_cluster;
	}
	// assert(st_clu != MAX_UNSIGNED_INT); // 不可能是-1
	p_rf_clu clu_data = (p_rf_clu) RF_FAT_ROOT[st_clu].addr;
	char *rf_data = (char *)clu_data + cur_pos % RAM_FS_CLUSTER_SIZE;
	int bytes_read = 0;
	while(bytes_read < length && st_clu != MAX_UNSIGNED_INT) {
		// read数据
		int bytes_left = (char *)(clu_data + 1) - rf_data;
		int bytes_to_read = min(bytes_left, length - bytes_read);
		bytes_to_read = min(bytes_to_read, *pf_rec->size - cur_pos);
		memcpy((void *)va2la(proc2pid(p_proc_current), buf + bytes_read), rf_data, bytes_to_read);
		bytes_read += bytes_to_read;
		st_clu = RF_FAT_ROOT[st_clu].next_cluster;
		clu_data = (p_rf_clu) RF_FAT_ROOT[st_clu].addr;
		rf_data = (char *)clu_data;
	}
	// 返回的字节数要么是length，要么是文件剩余的字节数
	assert(bytes_read == length || bytes_read == *pf_rec->size - p_proc_current->task.filp[fd]->fd_pos);
	p_proc_current->task.filp[fd]->fd_pos += bytes_read; // 更新偏移指针
	return bytes_read;
}

//return bytes have been write
int rf_write(int fd, const void *buf, int length)
{
	if (!(p_proc_current->task.filp[fd]->fd_mode & O_RDWR))
		return -EACCES;
	p_rf_inode frec = p_proc_current->task.filp[fd]->fd_node.fd_ram;
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	int fat_offset = pos / RAM_FS_CLUSTER_SIZE;
	int cluster = frec->start_cluster;
	while(cluster != MAX_UNSIGNED_INT){
		if(fat_offset==0)
			break;
		fat_offset--;
		if(RF_FAT_ROOT[cluster].next_cluster == MAX_UNSIGNED_INT){
			int inew = rf_find_first_free_alloc();
			assert(inew >= 0);
			RF_FAT_ROOT[cluster].next_cluster = inew;
		}
		cluster = RF_FAT_ROOT[cluster].next_cluster;
	}
	p_rf_clu clu_data = (p_rf_clu) RF_FAT_ROOT[cluster].addr;
	char *rf_data = (char *)clu_data + pos % RAM_FS_CLUSTER_SIZE;
	int bytes_write = 0;
	//end of expected buf later than current cluster
	while ((char *)(clu_data + 1) < rf_data + length-bytes_write)
	{
		int pref = (char *)(clu_data + 1) - rf_data;//part in this cluster, fixed 23.1.10 jf
		memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf + bytes_write), pref);
		bytes_write += pref;
		if(RF_FAT_ROOT[cluster].next_cluster == MAX_UNSIGNED_INT){
			int inew = rf_find_first_free_alloc();
			assert(inew >= 0);
			RF_FAT_ROOT[cluster].next_cluster = inew;
		}
		cluster = RF_FAT_ROOT[cluster].next_cluster;
		clu_data = (p_rf_clu) RF_FAT_ROOT[cluster].addr;
		rf_data = (char *)clu_data;
	}
	memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf+bytes_write), length - bytes_write);
	pos += length;
	p_proc_current->task.filp[fd]->fd_pos = pos;
	if(pos > *frec->size) *frec->size = pos;
	return length;
}

int rf_lseek(int fd, int offset, int whence)
{
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	//int f_size = p_proc_current->task.filp[fd]->fd_inode->i_size; 
	int f_size = *p_proc_current->task.filp[fd]->fd_node.fd_ram->size; 
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
	return rf_open(pathname, O_CREAT); // modified by xu for return value
	// return find_path(pathname, NULL, O_CREAT, RF_F) != NULL ? OK : -1;
}

int rf_create_dir(const char *dirname)
{
	return find_path(dirname, NULL, O_CREAT, RF_D, NULL) != NULL ? 0 : -1;
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
		return -EMFILE;
	for(i = 0;i < NR_FILE_DESC;i++)
		if(f_desc_table[i].flag == 0)
			break;
	if(i == NR_FILE_DESC)
		return -ENFILE;
	p_rf_inode fd_ram = find_path(dirname, NULL, O_RDWR, RF_D, NULL);
	if(fd_ram == NULL)
		return -ENOENT;
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
	p_rf_inode pREC = find_path(filename, NULL, 0, RF_F, NULL);
	if(pREC){
		if(*pREC->link_cnt >= 1)
			return -1; // todo: wait errno
		do_free((u32)pREC->size, sizeof(u32));
		do_free((u32)pREC->link_cnt, sizeof(u32));
		pREC->record_type = RF_NONE;
		u32 clu = pREC->start_cluster;
		rf_free_clu(clu);
		return 0;
	}
	return -ENOENT;
}

int rf_delete_dir(const char *dirname)
{
	// panic("todo: check empty in rf_delete_dir");
	p_rf_inode pREC = find_path(dirname, NULL, 0, RF_D, NULL);
	if(pREC){
		pREC->record_type = RF_NONE;
		if(*pREC->link_cnt >= 1)
			return -1; // todo: wait errno
		do_free((u32)pREC->size, sizeof(u32));
		do_free((u32)pREC->link_cnt, sizeof(u32));
		if(check_dir_size(pREC->start_cluster) > 2*sizeof(rf_inode)) {
			return -ENOTEMPTY;
		}
		u32 clu = pREC->start_cluster;
		rf_free_clu(clu);
		return 0;
	}
	return -ENOENT;
}

// unlink: delete a name and possibly the file it refers to
int rf_unlink(const char *pathname)
{
	// p_rf_inode pREC_f = find_path(pathname, NULL, 0, RF_F);
	p_rf_inode prec_f = find_path(pathname, NULL, 0, RF_F, NULL);
	if(prec_f) {
		if(*prec_f->link_cnt == 0)
			return rf_delete(pathname);
		*prec_f->link_cnt--; 
		prec_f->record_type = RF_NONE;
		return 0;
	}
	p_rf_inode prec_d = find_path(pathname, NULL, 0, RF_D, NULL);
	if(prec_d) {
		if(*prec_d->link_cnt == 0)
			return rf_delete_dir(pathname);
		*prec_d->link_cnt--;
		prec_d->record_type = RF_NONE;
		return 0;
	}
	return -ENOENT;
}

// link: make a new name for a file
int rf_link(const char *oldpath, const char *newpath)
{
	p_rf_inode old_inode_f = find_path(oldpath, NULL, 0, RF_F, NULL);
	if(old_inode_f) {
		*old_inode_f->link_cnt++;
		// return rf_create(newpath);
		return find_path(newpath, NULL, 0, RF_F, old_inode_f) == NULL ? -1 : 0;
	}
	p_rf_inode old_inode_d = find_path(oldpath, NULL, 0, RF_D, NULL);
	if(old_inode_d) {
		*old_inode_d->link_cnt++;
		return find_path(newpath, NULL, 0, RF_D, old_inode_d) == NULL ? -1 : 0;
	}
	return -ENOENT;
}


// 用于打印数字
int num2str(char *buf, int num)
{
	int i = 0;
	while(num) {
		buf[i++] = num%10 + '0';
		num /= 10;
	}
	int j = 0;
	for(j = 0; j < i/2; j++) {
		char tmp = buf[j];
		buf[j] = buf[i-j-1];
		buf[i-j-1] = tmp;
	}
	buf[i] = '\0';
	return i;
}
// 用于ls
int rf_readdir(int fd, void *buf, int length)
{
	rf_lseek(fd, 0, SEEK_SET);
	char tmp_buf[512];
	int pos = 0;
	char num_buf[15];
	while(rf_read(fd, tmp_buf, sizeof(rf_inode)) == sizeof(rf_inode)) {
		p_rf_inode p = (p_rf_inode)tmp_buf;
		if(p->record_type == RF_F) {
			memcpy(buf+pos, "file: ", strlen("file: "));
			pos += strlen("file: ");
			
		} else if(p->record_type == RF_D) {
			memcpy(buf+pos, "dir: ", strlen("dir: "));
			pos += strlen("dir: ");
		}
		if(p->record_type == RF_F || p->record_type == RF_D) {
			memcpy(buf+pos, p->name, strlen(p->name));
			pos += strlen(p->name);
			memcpy(buf+pos, " size: ", strlen(" size: "));
			pos += strlen(" size: ");
			// memcpy(buf+pos, *p->size, sizeof(u32));
			num2str(num_buf, *p->size);
			memcpy(buf+pos, num_buf, strlen(num_buf));
			pos += strlen(num_buf);
			memcpy(buf+pos, "\n", strlen("\n"));
			pos += strlen("\n");
		}
	}
	*(char*)(buf+pos) = '\0';
	return pos;
}