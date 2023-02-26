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
#include "spinlock.h"
#include "errno.h"

static int ramfs_dev;
static int RAM_FS_NR_CLU;
static int RAM_FS_DATA_CLU;
static p_rf_fat RF_FAT_ROOT;
static rf_clu rec;
static int clu_rec;
extern struct file_desc f_desc_table[NR_FILE_DESC];

static struct spinlock ramfs_lock;

static void write_fat(int clu) {
	rw_sector(DEV_WRITE, ramfs_dev, clu * sizeof(rf_fat), sizeof(rf_fat),
	p_proc_current->task.pid, (void*)RF_FAT_ROOT + clu);
}

static void read_fat(int clu) {
	rw_sector(DEV_WRITE, ramfs_dev, clu * sizeof(rf_fat), sizeof(rf_fat),
	p_proc_current->task.pid, (void*)RF_FAT_ROOT + clu);
}

// 向ramdisk对应簇号写入数据
static void write_data(int clu, void* buf) {
	rw_sector_sched(DEV_WRITE, ramfs_dev, 
	(clu + RAM_FS_DATA_CLU)*RAM_FS_CLUSTER_SIZE, RAM_FS_CLUSTER_SIZE, 
	p_proc_current->task.pid, buf);
}

// 从ramdisk对应簇号读取数据
static void read_data(int clu, void* buf) {
	rw_sector_sched(DEV_READ, ramfs_dev, 
	(clu + RAM_FS_DATA_CLU)*RAM_FS_CLUSTER_SIZE, RAM_FS_CLUSTER_SIZE, 
	p_proc_current->task.pid, buf);
}

static void sync_inode(p_rf_inode inode) {
	rw_sector(DEV_WRITE, ramfs_dev, 
	(inode->index/RF_NR_REC + RAM_FS_DATA_CLU)*RAM_FS_CLUSTER_SIZE+ (inode->index%RF_NR_REC)*sizeof(rf_inode), 
	sizeof(rf_inode), p_proc_current->task.pid, inode);
}

static int rf_alloc_clu(int clu){
	RF_FAT_ROOT[clu].next_cluster = MAX_UNSIGNED_INT;
	// memset((void*)RF_FAT_ROOT[clu].addr, 0, sizeof(rf_clu));
	char buf[RAM_FS_CLUSTER_SIZE] = {0};
	write_data(clu, buf);
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
		write_fat(clu);
		clu = nxt_clu;
	}
}

static u32 check_dir_size(u32 dir_clu)
{
	rf_clu buf;
	u32 dir_size = 0;
	while(RF_FAT_ROOT[dir_clu].next_cluster != MAX_UNSIGNED_INT) {
		dir_size += RAM_FS_CLUSTER_SIZE;
		dir_clu = RF_FAT_ROOT[dir_clu].next_cluster;
	}
	read_data(dir_clu, &buf);
	// p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
	int i;
	for (i = RF_NR_REC - 1; i >= 0; i--) {
		// if(rec[i].record_type != RF_NONE){
		if(buf.entry[i].record_type != RF_NONE){
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
	// p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
	rf_clu rec = {0};
	strcpy(rec.entry[0].name, ".");
	rec.entry[0].record_type = RF_D;
	rec.entry[0].index = dir_clu * RF_NR_REC;
	// rec->size = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
	// rec->link_cnt = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32)));
	rec.entry[0].size = sizeof(rf_inode);
	rec.entry[0].link_cnt = 0;
	rec.entry[0].start_cluster = dir_clu;
	if(parent_rec != NULL) {
		rec.entry[0].size += sizeof(rf_inode);
		strcpy(rec.entry[1].name, "..");
		rec.entry[1].record_type = RF_D;
		rec.entry[1].index = dir_clu * RF_NR_REC;
		rec.entry[1].size = parent_rec->size;
		rec.entry[1].start_cluster = parent_rec->start_cluster;
		rec.entry[1].link_cnt = parent_rec->link_cnt;
	}
	write_data(dir_clu, &rec);
}

static p_rf_inode alloc_rf_inode(p_rf_inode tmp_inode) {
	p_rf_inode ret_inode = (p_rf_inode)K_PHY2LIN(do_malloc(sizeof(rf_inode)));
	memcpy(ret_inode, tmp_inode, sizeof(rf_inode)); 
	return ret_inode;
}
// 对于文件夹,传入的size无用
// dir_rec: 待写入的文件所在文件夹记录项(在上级目录文件数据中)
// p_fa表示是否有父亲inode，若为NULL则创建，否则指向父亲
static p_rf_inode rf_write_record(p_rf_inode dir_rec, const char *name, u32 entClu, u32 type, p_rf_inode p_fa)
{
	u32 dir_clu = 0;
	if(dir_rec != NULL) {
		dir_clu = dir_rec->start_cluster;
	}
	// p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr); // 文件夹记录项
	int i, inew;
	read_data(dir_clu, &rec);
	for (i = 0; i < RF_NR_REC;){
		if(rec.entry[i].record_type == RF_NONE){
			break;
		}
		i++;
		if(i == RF_NR_REC) {
			inew = rf_find_first_free_alloc();
			if(inew<0)
				return NULL;
			RF_FAT_ROOT[dir_clu].next_cluster = inew;
			write_fat(dir_clu);
			// return rf_write_record(i, name, entClu, type, size);
			dir_clu = inew;
			read_data(dir_clu, &rec);
			i = 0;
		}
	}
	
	strcpy(rec.entry[i].name, name);
	rec.entry[i].record_type = type;
	u32 new_size = check_dir_size(dir_clu);
	rec.entry[0].size = new_size;
	rec.entry[i].index = dir_clu*RF_NR_REC + i;
	if(p_fa != NULL) {
		rec.entry[i].record_type = p_fa->record_type;
		rec.entry[i].start_cluster = p_fa->start_cluster;
		rec.entry[i].size = p_fa->size;
		rec.entry[i].link_cnt = p_fa->link_cnt;
	}
	else if(type == RF_F) {
		rec.entry[i].size = 0;
		rec.entry[i].shared_cnt = 0;
		rec.entry[i].link_cnt = 0;
		rec.entry[i].start_cluster = entClu; // 文件内容所在簇号
	}
	else if(type == RF_D) {
		init_dir_record(entClu, &rec.entry[0]); // link_cnt已经维护
		// rec[i].size = (u32*)K_PHY2LIN(do_kmalloc(sizeof(u32))); // 初次分配
		rec.entry[i].size = check_dir_size(entClu);
		rec.entry[i].link_cnt = 0;
		rec.entry[i].start_cluster = entClu; // 文件内容所在簇号
	}
	// pRF_REC parent = find_path("..", dir_clu, 0, RF_D);
	if(dir_rec != NULL) {
		dir_rec->size = new_size;
	}
	clu_rec = dir_clu;
	write_data(clu_rec, &rec);
	return &rec.entry[i];
}

// 初始化ram文件系统
void init_ram_fs()
{
	ramfs_dev = get_fs_dev(RAMDISK_DRV, RAM_FS_TYPE);
	/* get the geometry of ROOTDEV */
	MESSAGE driver_msg;
	struct part_info geo;
	driver_msg.type		= DEV_IOCTL;
	//driver_msg.DEVICE	= MINOR(FAT_DEV);	//deleted by mingxuan 2020-10-27
	driver_msg.DEVICE	= MINOR(ramfs_dev);	//modified by mingxuan 2020-10-27

	driver_msg.REQUEST	= DIOCTL_GET_GEO;
	driver_msg.BUF		= &geo;
	driver_msg.PROC_NR	= proc2pid(p_proc_current);
	hd_ioctl(&driver_msg);
	disp_str("dev size: ");
	disp_int(geo.size);
	disp_str(" sectors\n");
	RAM_FS_NR_CLU = geo.size * SECTOR_SIZE/(RAM_FS_CLUSTER_SIZE + sizeof(rf_fat));
	RAM_FS_DATA_CLU = (RAM_FS_NR_CLU*sizeof(rf_fat) + RAM_FS_CLUSTER_SIZE)/RAM_FS_CLUSTER_SIZE;
	//in syscall we can only use 3G~3G+128M so we init the two at the beginning
	RF_FAT_ROOT = (p_rf_fat)K_PHY2LIN(do_kmalloc(sizeof(rf_fat) * RAM_FS_NR_CLU));
	// RF_DATA_ROOT = (p_rf_clu)K_PHY2LIN(RAM_FS_DATA_BASE);
	rw_sector(DEV_READ, ramfs_dev, 0, sizeof(rf_fat) * RAM_FS_NR_CLU, p_proc_current->task.pid, RF_FAT_ROOT);
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
// 参数变更:去掉冗余参数dir_rec
p_rf_inode find_path(const char *path, int flag, int find_type, p_rf_inode p_fa) 
{
	u32 dir_clu = 0;
	char ent_name[RF_MX_ENT_NAME];
	rf_inode dir_rec = {0};
	int pathpos = 0, j, len = strlen(path);
	while(pathpos < len) {
		for(j = 0; pathpos < len;j++, pathpos++) {
			if(path[pathpos] == '/') break;
			ent_name[j] = path[pathpos];
		}
		ent_name[j] = '\0';
		// p_rf_inode rec = (p_rf_inode)(RF_FAT_ROOT[dir_clu].addr);
		read_data(dir_clu, &rec);
		int i;
		for(i = 0; i < RF_NR_REC; i++) {
			if(rec.entry[i].record_type == RF_NONE) continue;
			if(pathpos < len && rec.entry[i].record_type == RF_D && strcmp(rec.entry[i].name, ent_name) == 0) {
				rec.entry[i].size = check_dir_size(rec.entry[i].start_cluster);
				memcpy(&dir_rec, &rec.entry[i], sizeof(rf_inode));
				write_data(dir_clu, &rec);
				dir_clu = rec.entry[i].start_cluster;
				break;
			}
			if(pathpos == len && rec.entry[i].record_type != RF_NONE && strcmp(rec.entry[i].name, ent_name) == 0) {
				if(rec.entry[i].record_type == find_type) {
					if(rec.entry[i].record_type == RF_D)
						rec.entry[i].size = check_dir_size(rec.entry[i].start_cluster);
					clu_rec = dir_clu;
					return &rec.entry[i];
				}
				else return NULL;
			}
		}
		if(pathpos < len && i == RF_NR_REC) {
			if(RF_FAT_ROOT[dir_clu].next_cluster == MAX_UNSIGNED_INT)
				return NULL;
			write_data(dir_clu, &rec);
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
				//dir init move to write_record
				return rf_write_record(&dir_rec, ent_name, inew, find_type, p_fa);
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
	for (i = 0; i < NR_FILES; i++) {
		if (p_proc_current->task.filp[i] == 0) {
			fd = i;
			break;
		}
	}
	if(i == NR_FILES) {
		return -EMFILE;
	}
	assert(fd >= 0 && fd < NR_FILES);

	/* find a free slot in f_desc_table[] */
	for (i = 0; i < NR_FILE_DESC; i++)
		if (f_desc_table[i].flag == 0)
			break;
	if(i == NR_FILE_DESC) {
		return -ENFILE;
	}
	assert(i < NR_FILE_DESC);

	p_rf_inode fd_ram = find_path(pathname, flags, RF_F, NULL);//from root
	if(fd_ram) {
		fd_ram->shared_cnt++;
		/* connects proc with file_descriptor */
		p_proc_current->task.filp[fd] = &f_desc_table[i];
		f_desc_table[i].flag = 1;
		/* connects file_descriptor with inode */
		f_desc_table[i].fd_node.fd_ram = alloc_rf_inode(fd_ram);
		f_desc_table[i].dev_index = VFS_INDEX_RAMFS;
		f_desc_table[i].fd_mode = flags;
		f_desc_table[i].fd_pos = 0;
	}
	else{
		return -ENOENT;
	}
	return fd;
}

int rf_close(int fd)
{
	if(fd < 0 || fd > NR_FILES || p_proc_current->task.filp[fd] == 0) {
		return -1;
	}
	if(p_proc_current->task.filp[fd]->fd_node.fd_ram->shared_cnt > 0)
		p_proc_current->task.filp[fd]->fd_node.fd_ram->shared_cnt--;
	do_free((u32)K_LIN2PHY(p_proc_current->task.filp[fd]->fd_node.fd_ram), sizeof(rf_inode));
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
	rf_clu clu_data;
	read_data(st_clu, &clu_data);
	char *rf_data = (char *)(&clu_data) + cur_pos % RAM_FS_CLUSTER_SIZE;
	int bytes_read = 0;
	while(bytes_read < length && st_clu != MAX_UNSIGNED_INT) {
		// read数据
		int bytes_left = (char *)(&clu_data + 1) - rf_data;
		int bytes_to_read = min(bytes_left, length - bytes_read);
		bytes_to_read = min(bytes_to_read, pf_rec->size - cur_pos);
		memcpy((void *)va2la(proc2pid(p_proc_current), buf + bytes_read), rf_data, bytes_to_read);
		bytes_read += bytes_to_read;
		st_clu = RF_FAT_ROOT[st_clu].next_cluster;
		// clu_data = (p_rf_clu) RF_FAT_ROOT[st_clu].addr;
		read_data(st_clu, &clu_data);
		rf_data = (char *)(&clu_data);
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
			// assert(inew >= 0);
			if(inew < 0) // no more space
				return 0;
			RF_FAT_ROOT[cluster].next_cluster = inew;
			write_fat(cluster);
		}
		cluster = RF_FAT_ROOT[cluster].next_cluster;
	}
	// p_rf_clu clu_data = (p_rf_clu) RF_FAT_ROOT[cluster].addr;
	rf_clu clu_data;
	char *rf_data = (char *)(&clu_data) + pos % RAM_FS_CLUSTER_SIZE;
	int bytes_write = 0;
	read_data(cluster, &clu_data);
	//end of expected buf later than current cluster
	while ((char *)(&clu_data + 1) < rf_data + length-bytes_write)
	{
		int pref = (char *)(&clu_data + 1) - rf_data;//part in this cluster, fixed 23.1.10 jf
		memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf + bytes_write), pref);
		write_data(cluster, &clu_data);
		bytes_write += pref;
		if(RF_FAT_ROOT[cluster].next_cluster == MAX_UNSIGNED_INT){
			int inew = rf_find_first_free_alloc();
			// assert(inew >= 0);
			if(inew < 0) {// no more space
				p_proc_current->task.filp[fd]->fd_pos += bytes_write;
				if(p_proc_current->task.filp[fd]->fd_pos > frec->size) 
					frec->size = p_proc_current->task.filp[fd]->fd_pos;
				return bytes_write; // return bytes have been write
			}
			RF_FAT_ROOT[cluster].next_cluster = inew;
			write_fat(cluster);
		}
		cluster = RF_FAT_ROOT[cluster].next_cluster;
		// clu_data = (p_rf_clu) RF_FAT_ROOT[cluster].addr;
		read_data(cluster, &clu_data);
		rf_data = (char *)(&clu_data);
	}
	memcpy(rf_data, (void*)va2la(proc2pid(p_proc_current), (void*)buf+bytes_write), length - bytes_write);
	write_data(cluster, &clu_data);
	pos += length;
	p_proc_current->task.filp[fd]->fd_pos = pos;
	if(pos > frec->size) frec->size = pos;
	sync_inode(frec);
	return length;
}

int rf_lseek(int fd, int offset, int whence)
{
	int pos = p_proc_current->task.filp[fd]->fd_pos;
	//int f_size = p_proc_current->task.filp[fd]->fd_inode->i_size; 
	int f_size = p_proc_current->task.filp[fd]->fd_node.fd_ram->size; 
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
	return rf_open(pathname, O_CREAT); 
}

int rf_create_dir(const char *dirname)
{
	return find_path(dirname, O_CREAT, RF_D, NULL) != NULL ? 0 : -1;
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
	p_rf_inode fd_ram = find_path(dirname, O_RDWR, RF_D, NULL);
	if(fd_ram == NULL)
		return -ENOENT;
	// update f_desc_table
	p_proc_current->task.filp[fd] = &f_desc_table[i];
	f_desc_table[i].flag = 1;
	f_desc_table[i].fd_node.fd_ram = alloc_rf_inode(fd_ram);
	f_desc_table[i].dev_index = VFS_INDEX_RAMFS;
	f_desc_table[i].fd_mode = O_RDWR; 
	f_desc_table[i].fd_pos = 0;
	return fd;
}

int rf_delete(const char *filename)
{
	p_rf_inode pREC = find_path(filename, 0, RF_F, NULL);
	if(pREC){
		if(pREC->link_cnt >= 1)
			return -1; // todo: wait errno
		if(pREC->shared_cnt >= 1)
			return -1;
		// do_free(K_LIN2PHY((u32)pREC->link_cnt), sizeof(u32));
		pREC->record_type = RF_NONE;
		write_data(clu_rec, &rec);
		u32 clu = pREC->start_cluster;
		rf_free_clu(clu);
		return 0;
	}
	return -ENOENT;
}

int rf_delete_dir(const char *dirname)
{
	// panic("todo: check empty in rf_delete_dir");
	p_rf_inode pREC = find_path(dirname, 0, RF_D, NULL);
	if(pREC){
		if(pREC->link_cnt >= 1)
			return -1; // todo: wait errno
		// do_free(K_LIN2PHY((u32)pREC->size), sizeof(u32));
		// do_free(K_LIN2PHY((u32)pREC->link_cnt), sizeof(u32));
		if(check_dir_size(pREC->start_cluster) > 2*sizeof(rf_inode)) {
			return -ENOTEMPTY;
		}
		pREC->record_type = RF_NONE;
		write_data(clu_rec, &rec);
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
	p_rf_inode prec_f = find_path(pathname, 0, RF_F, NULL);
	if(prec_f) {
		if(prec_f->link_cnt == 0)
			return rf_delete(pathname);
		prec_f->link_cnt--; 
		prec_f->record_type = RF_NONE;
		write_data(clu_rec, &rec);
		return 0;
	}
	p_rf_inode prec_d = find_path(pathname, 0, RF_D, NULL);
	if(prec_d) {
		if(prec_d->link_cnt == 0)
			return rf_delete_dir(pathname);
		prec_d->link_cnt--;
		prec_d->record_type = RF_NONE;
		write_data(clu_rec, &rec);
		return 0;
	}
	return -ENOENT;
}

// link: make a new name for a file
int rf_link(const char *oldpath, const char *newpath)
{
	p_rf_inode old_inode_f = find_path(oldpath, 0, RF_F, NULL);
	rf_clu tmp;
	int clu = clu_rec;
	memcpy(&tmp, &rec, sizeof(rf_clu));
	old_inode_f = (p_rf_inode)(((char*)(&tmp) + (u32)old_inode_f - (u32)(&rec)));
	if(old_inode_f) {
		// *old_inode_f->link_cnt++;
		// return 
		p_rf_inode debug_inode = find_path(newpath, O_CREAT, old_inode_f->record_type, old_inode_f);
		if(debug_inode == NULL)
			return -1;
		else {
			old_inode_f->link_cnt++;
			sync_inode(old_inode_f);
			debug_inode->link_cnt++;
			sync_inode(debug_inode);
			return 0;
		}
	}
	// according to commom rule for hard link, dirs are usually not allowed
	// p_rf_inode old_inode_d = find_path(oldpath, NULL, 0, RF_D, NULL);
	// if(old_inode_d) {
	// 	if(find_path(newpath, NULL, O_CREAT, old_inode_d->record_type, old_inode_d) == NULL)
	// 		return -1;
	// 	else {
	// 		(*old_inode_d->link_cnt)++;
	// 		return 0;
	// 	}
	// }
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
	if(i == 0) {
		buf[i++] = '0';
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
			num2str(num_buf, p->size);
			memcpy(buf+pos, num_buf, strlen(num_buf));
			pos += strlen(num_buf);
			memcpy(buf+pos, "\n", strlen("\n"));
			pos += strlen("\n");
		}
	}
	*(char*)(buf+pos) = '\0';
	return pos;
}