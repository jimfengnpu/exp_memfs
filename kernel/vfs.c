/**********************************************************
*	vfs.c       //added by mingxuan 2019-5-17
***********************************************************/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "fs_const.h"
#include "hd.h"
#include "fs.h"
#include "fs_misc.h"
#include "vfs.h"
#include "fat32.h"
#include "ramfs.h"
#include "stdio.h"
#include "assert.h"

//static struct device  device_table[NR_DEV];  //deleted by mingxuan 2020-10-18
static struct vfs  vfs_table[NR_FS];   //modified by mingxuan 2020-10-18

struct file_desc f_desc_table[NR_FILE_DESC];
struct super_block super_block[NR_SUPER_BLOCK]; //added by mingxuan 2020-10-30

//static struct file_op f_op_table[NR_fs]; //文件系统操作表
static struct file_op f_op_table[NR_FS_OP]; //modified by mingxuan 2020-10-18
static struct sb_op   sb_op_table[NR_SB_OP];   //added by mingxuan 2020-10-30

//static void init_dev_table();//deleted by mingxuan 2020-10-30
static void init_vfs_table();  //modified by mingxuan 2020-10-30
void init_file_desc_table();   //added by mingxuan 2020-10-30
void init_fileop_table();
void init_super_block_table();  //added by mingxuan 2020-10-30
void init_sb_op_table();

static int get_index(char path[]);

void init_vfs()
{

    init_file_desc_table();
    init_fileop_table();
  
    init_super_block_table();
    init_sb_op_table(); //added by mingxuan 2020-10-30

    //init_dev_table(); //deleted by mingxuan 2020-10-30
    init_vfs_table();   //modified by mingxuan 2020-10-30
}

//added by mingxuan 2020-10-30
void init_file_desc_table()
{
    int i;
	for (i = 0; i < NR_FILE_DESC; i++)
		memset(&f_desc_table[i], 0, sizeof(struct file_desc));
}

void init_fileop_table()
{
    // table[0] for tty 
    f_op_table[0].open = real_open;
    f_op_table[0].close = real_close;
    f_op_table[0].write = real_write;
    f_op_table[0].lseek = real_lseek;
    f_op_table[0].unlink = real_unlink;
    f_op_table[0].read = real_read;

    // table[1] for orange 
    f_op_table[1].open = real_open;
    f_op_table[1].close = real_close;
    f_op_table[1].write = real_write;
    f_op_table[1].lseek = real_lseek;
    f_op_table[1].unlink = real_unlink;
	f_op_table[1].delete = real_unlink; // 单纯为了实现delete语义
    f_op_table[1].read = real_read;

    // table[2] for fat32
    f_op_table[2].create = CreateFile;
    f_op_table[2].delete = DeleteFile;
    f_op_table[2].open = OpenFile;
    f_op_table[2].close = CloseFile;
    f_op_table[2].write = WriteFile;
    f_op_table[2].read = ReadFile;
    f_op_table[2].opendir = OpenDir;
    f_op_table[2].createdir = CreateDir;
    f_op_table[2].deletedir = DeleteDir;

    // table[3] for ramfs
    f_op_table[3].create = rf_create;
    f_op_table[3].open = rf_open;
    f_op_table[3].close = rf_close;
    f_op_table[3].read = rf_read;
    f_op_table[3].write = rf_write;
    f_op_table[3].lseek = rf_lseek;
    f_op_table[3].delete = rf_delete;
	f_op_table[3].unlink = rf_unlink;
    f_op_table[3].opendir = rf_open_dir;
    f_op_table[3].createdir = rf_create_dir;
    f_op_table[3].deletedir = rf_delete_dir;
	f_op_table[3].link = rf_link;
}

//added by mingxuan 2020-10-30
void init_super_block_table(){
    struct super_block * sb = super_block;						//deleted by mingxuan 2020-10-30

    //super_block[0] is tty0, super_block[1] is tty1, uper_block[2] is tty2
    for(; sb < &super_block[3]; sb++) {   
        sb->sb_dev =  DEV_CHAR_TTY;
        sb->fs_type = TTY_FS_TYPE;
    }

    //super_block[3] is orange's superblock
    sb->sb_dev = DEV_HD;
    sb->fs_type = ORANGE_TYPE; 
    sb++;

    //super_block[4] is fat32's superblock
    sb->sb_dev = DEV_HD;
    sb->fs_type = FAT32_TYPE; 
    sb++;

    //super_block[5] is ramfs's superblock
    sb->sb_dev = DEV_HD;
    sb->fs_type = RAM_FS_TYPE; 
    sb++;

    //another super_block are free
    for (; sb < &super_block[NR_SUPER_BLOCK]; sb++) {
	sb->sb_dev = NO_DEV;
        sb->fs_type = NO_FS_TYPE;
    } 
}

//added by mingxuan 2020-10-30
void init_sb_op_table(){
    //orange
    sb_op_table[0].read_super_block = read_super_block;
    sb_op_table[0].get_super_block = get_super_block;

    //fat32 and tty
    sb_op_table[1].read_super_block = NULL;
    sb_op_table[1].get_super_block = NULL;
}

//static void init_dev_table(){
static void init_vfs_table(){  // modified by mingxuan 2020-10-30

    // 我们假设每个tty就是一个文件系统
    // tty0
    // device_table[0].dev_name="dev_tty0";
    // device_table[0].op = &f_op_table[0];
    vfs_table[0].fs_name = "dev_tty0"; //modifed by mingxuan 2020-10-18
    vfs_table[0].op = &f_op_table[0];
    vfs_table[0].sb = &super_block[0];  //每个tty都有一个superblock //added by mingxuan 2020-10-30
    vfs_table[0].s_op = &sb_op_table[1];    //added by mingxuan 2020-10-30

    // tty1
    //device_table[1].dev_name="dev_tty1";
    //device_table[1].op =&f_op_table[0];
    vfs_table[1].fs_name = "dev_tty1"; //modifed by mingxuan 2020-10-18
    vfs_table[1].op = &f_op_table[0];
    vfs_table[1].sb = &super_block[1];  //每个tty都有一个superblock //added by mingxuan 2020-10-30
    vfs_table[1].s_op = &sb_op_table[1];    //added by mingxuan 2020-10-30

    // tty2
    //device_table[2].dev_name="dev_tty2";
    //device_table[2].op=&f_op_table[0];
    vfs_table[2].fs_name = "dev_tty2"; //modifed by mingxuan 2020-10-18
    vfs_table[2].op = &f_op_table[0];
    vfs_table[2].sb = &super_block[2];  //每个tty都有一个superblock //added by mingxuan 2020-10-30
    vfs_table[2].s_op = &sb_op_table[1];    //added by mingxuan 2020-10-30

    // fat32
    //device_table[3].dev_name="fat0";
    //device_table[3].op=&f_op_table[2];
    vfs_table[3].fs_name = "fat0"; //modifed by mingxuan 2020-10-18
    vfs_table[3].op = &f_op_table[2];
    vfs_table[3].sb = &super_block[4];      //added by mingxuan 2020-10-30
    vfs_table[3].s_op = &sb_op_table[1];    //added by mingxuan 2020-10-30

    // orange
    //device_table[4].dev_name="orange";
    //device_table[4].op=&f_op_table[1];
    vfs_table[4].fs_name = "orange"; //modifed by mingxuan 2020-10-18
    vfs_table[4].op = &f_op_table[1];
    vfs_table[4].sb = &super_block[3];      //added by mingxuan 2020-10-30
    vfs_table[4].s_op = &sb_op_table[0];    //added by mingxuan 2020-10-30

    //ramfs
    vfs_table[VFS_INDEX_RAMFS].fs_name = "ram";
    vfs_table[VFS_INDEX_RAMFS].op = &f_op_table[3];
    vfs_table[VFS_INDEX_RAMFS].sb = &super_block[5];
    vfs_table[VFS_INDEX_RAMFS].s_op = &sb_op_table[1];
}

//path: /xxx/yyy  ==>  /xxx/yyy
//path: xxx/yyy   ==>  /cwd/xxx/yyy
//path: xx/./a    ==>  /cwd/xx/a
//path: xx/../a   ==>  /cwd/a
// 处理相对路径，得到绝对路径
static void process_relative_path(char *path)
{
    if(path[0] == '/') return; // 无需拼接，直接返回
    int path_len = strlen(path);
    int cwd_len = strlen(p_proc_current->task.cwd);
    if(strcmp(p_proc_current->task.cwd, "/") == 0) {
		cwd_len = 0;
    }
    int i = path_len - 1;
    for(int j = i + cwd_len + 1; j > cwd_len; j--)
    {
		path[j] = path[i--];
    }
    memcpy(path, p_proc_current->task.cwd, cwd_len);
    path[cwd_len] = '/';
    path[path_len + cwd_len +1] = '\0';
	int offs = 0, dir_stack[MAX_PATH], top = 0;
	for( i = 0; i <= path_len + cwd_len + 1; i++) { // 维护"."和".."的栈
		if((path[i] == '/' || i == path_len + cwd_len + 1) && path[i - 1] != '/') {
			if(top) {
				if(strncmp(path + dir_stack[top], "/.", i - dir_stack[top]) == 0) {
					offs += i - dir_stack[top];
				}else if(strncmp(path + dir_stack[top], "/..", i - dir_stack[top]) == 0) {
					if(top > 1) top--;
					offs += i - dir_stack[top];
				}
			}
			dir_stack[++top] = i;
		}
		path[i - offs] = path[i];
	}
	if(strcmp(path, "") == 0) {
		strcpy(path, "/");
	}
}

static int get_index(char path[]){
    process_relative_path(path);
    int pathlen = strlen(path);
    //char dev_name[DEV_NAME_LEN];
    char fs_name[DEV_NAME_LEN];   //modified by mingxuan 2020-10-18
    int len = (pathlen < DEV_NAME_LEN) ? pathlen : DEV_NAME_LEN;
    
    int i,a=1,j=1;
    for(i=0;i<pathlen;i++,j++){
        if( path[j] == '/'){
            a=j;
            a++;
            break;
        }
        else {
            //dev_name[i] = path[i];
            fs_name[i] = path[j];   //modified by mingxuan 2020-10-18
        }
    }
    //dev_name[i] = '\0';
    fs_name[i] = '\0';  //modified by mingxuan 2020-10-18
    for(i=0;i<pathlen-a;i++)
        path[i] = path[i+a];
    path[pathlen-a] = '\0';

//     for(i=0;i<NR_DEV;i++)
    for(i=0;i<NR_FS;i++)    //modified by mingxuan 2020-10-29
    {
        // if(!strcmp(dev_name, device_table[i].dev_name))
        if(!strcmp(fs_name, vfs_table[i].fs_name)) //modified by mingxuan 2020-10-18
            return i;
    }

    return -1;
}


/*======================================================================*
                              sys_* 系列函数
 *======================================================================*/

int sys_open(void *uesp)
{
    return do_vopen((const char *)get_arg(uesp, 1), get_arg(uesp, 2));
}

int sys_close(void *uesp)
{
    return do_vclose(get_arg(uesp, 1));
}

int sys_read(void *uesp)
{
    return do_vread(get_arg(uesp, 1), (char *)get_arg(uesp, 2), get_arg(uesp, 3));
}

int sys_write(void *uesp)
{
    return do_vwrite(get_arg(uesp, 1), (const char *)get_arg(uesp, 2), get_arg(uesp, 3));
}

int sys_lseek(void *uesp)
{
    return do_vlseek(get_arg(uesp, 1), get_arg(uesp, 2), get_arg(uesp, 3));
}

int sys_unlink(void *uesp) {
    return do_vunlink((const char *)get_arg(uesp, 1));
}

int sys_create(void *uesp) {
    return do_vcreate((char *)get_arg(uesp, 1));
}

int sys_delete(void *uesp) {
    return do_vdelete((char *)get_arg(uesp, 1));
}

int sys_opendir(void *uesp) {
	return do_vopendir((char *)get_arg(uesp, 1));
	// return do_vopen()
    // return do_vopendir((char *)get_arg(uesp, 1), (struct dir_ent*)get_arg(uesp, 2), (int)get_arg(uesp, 3));
}

int sys_createdir(void *uesp) {
    return do_vcreatedir((char *)get_arg(uesp, 1));
}

int sys_deletedir(void *uesp) {
    return do_vdeletedir((char *)get_arg(uesp, 1));
}

int sys_chdir(char *arg) 
{
	return do_vchdir((const char*)get_arg(arg, 1));
}

int sys_mkdir(char *arg)
{
	return do_vmkdir((char*)get_arg(arg, 1));
	// return rf_create_dir((const char*)get_arg(arg, 1));
}

int sys_link(void *arg)
{
	return do_vlink((const char*)get_arg(arg, 1), (const char*)get_arg(arg, 2));
}

int sys_readdir(void *arg) {
	return do_vreaddir((int)get_arg(arg, 1), (char *)get_arg(arg, 2), (int)get_arg(arg, 3));
}

/*======================================================================*
                              do_v* 系列函数
 *======================================================================*/

int do_vopen(const char *path, int flags) {

    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,(char *)path);
    pathname[pathlen] = 0;

	process_relative_path(pathname); // get absolute path

    int index;
    int fd = -1;
    index = get_index(pathname);
    if(index == -1){ // to check if the device/fs is exist
        kprintf("pathname error! path: %s\n", path);
        return -1;
    }
    fd = vfs_table[index].op->open(pathname, flags);    //modified by mingxuan 2020-10-18
    if(fd != -1)
    {
        p_proc_current -> task.filp[fd] -> dev_index = index;
    } else {
        kprintf("          error!\n");
    }
                   
    return fd;    
}


int do_vclose(int fd) {
    int index = p_proc_current->task.filp[fd]->dev_index;
    return vfs_table[index].op->close(fd);  //modified by mingxuan 2020-10-18
}

int do_vread(int fd, char *buf, int count) {
    int index = p_proc_current->task.filp[fd]->dev_index;
    return vfs_table[index].op->read(fd, buf, count);   //modified by mingxuan 2020-10-18
}

int do_vwrite(int fd, const char *buf, int count) {
    //modified by mingxuan 2019-5-23
    char s[512];
    int index = p_proc_current->task.filp[fd]->dev_index;
    const char *fsbuf = buf;
    int f_len = count;
    int bytes;
    while(f_len)
    {
        int iobytes = min(512, f_len); // todo : avoid hard code
        int i=0;
        for(i=0; i<iobytes; i++)
        {
            s[i] = *fsbuf;
            fsbuf++;
        }
        //bytes = device_table[index].op->write(fd,s,iobytes);
        bytes = vfs_table[index].op->write(fd,s,iobytes);   //modified by mingxuan 2020-10-18
        if(bytes != iobytes)
        {
            return bytes;
        }
        f_len -= bytes;
    }
    return count;
}

int do_vunlink(const char *path) {
    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,(char *)path);
    pathname[pathlen] = 0;

	process_relative_path(pathname); // get absolute path

    int index;
    index = get_index(pathname);
    if(index==-1){
        kprintf("pathname error!\n");
        return -1;
    }
    
    //return device_table[index].op->unlink(pathname);
    return vfs_table[index].op->unlink(pathname);   //modified by mingxuan 2020-10-18
}

int do_vlseek(int fd, int offset, int whence) {
    int index = p_proc_current->task.filp[fd]->dev_index;

    //return device_table[index].op->lseek(fd, offset, whence);
    return vfs_table[index].op->lseek(fd, offset, whence);  //modified by mingxuan 2020-10-18

}

//int do_vcreate(char *pathname) {
int do_vcreate(char *filepath) { //modified by mingxuan 2019-5-17
    //added by mingxuan 2019-5-17  
    int state;
    const char *path = filepath;

    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,(char *)path);
    pathname[pathlen] = 0;

	process_relative_path(pathname); // get absolute path

    int index;
    index = get_index(pathname);
    if(index == -1){
        kprintf("pathname error! path: %s\n", path);
        return -1;
    }
    state = vfs_table[index].op->create(pathname); //modified by mingxuan 2020-10-18
    // if (state == 1) {
    //     kprintf("          create file success!");
    // } else {
	// 	DisErrorInfo(state);
    // }
    return state;
}

int do_vdelete(char *path) {

    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,path);
    pathname[pathlen] = 0;

    int index;
    index = get_index(pathname);
    if(index==-1){
        kprintf("pathname error!\n");
        return -1;
    }
    //return device_table[index].op->delete(pathname);
    return vfs_table[index].op->delete(pathname);   //modified by mingxuan 2020-10-18
}
// int do_vopendir(char *path, struct dir_ent *dirent, int mx_ent) {
// int do_vopendir(char *path) {
//     int state;
//     int cnt = 0;
//     int pathlen = strlen(path);
//     char pathname[MAX_PATH];
    
//     strcpy(pathname,path);
//     pathname[pathlen] = 0;

// 	// xu: 这原本是找index？
//     // if(strcmp(pathname, "/") == 0){
// 	// 	while(cnt < NR_FS && cnt < mx_ent) {
// 	// 		strcpy(dirent[cnt].name, vfs_table[cnt].fs_name);
// 	// 		cnt++;
// 	// 	}
// 	// 	return 1;
//     // }
//     int index;
//     index = get_index(pathname); //(int)(pathname[1]-'0');
//     if(index==-1){
//         kprintf("pathname error!\n");
//         return -1;
//     }
//     //     for(int j=0;j<= pathlen-3;j++)
//     //     {
//     //         pathname[j] = pathname[j+3];
//     //     }
//     // state = f_op_table[index].opendir(pathname, dirent, mx_ent);
// 	state = vfs_table[index].op->opendir(pathname);
//     // if (state == 1) {
//     //     kprintf("          open dir success!");
//     // } else {
// 	// 	DisErrorInfo(state);
//     // }    
//     return state;
// }

int do_vcreatedir(char *path) {
    int state;

    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,path);
    pathname[pathlen] = 0;

    int index;
    index =  get_index(pathname); //(int)(pathname[1]-'0');
    if(index==-1){
        kprintf("pathname error!\n");
        return -1;
    }
//     for(int j=0;j<= pathlen-3;j++)
//     {
//         pathname[j] = pathname[j+3];
//     }
    state = f_op_table[index].createdir(pathname);
    if (state == 1) {
        kprintf("          create dir success!");
    } else {
		DisErrorInfo(state);
    }    
    return state;
}

int do_vdeletedir(char *path) {
    int state;
    int pathlen = strlen(path);
    char pathname[MAX_PATH];
    
    strcpy(pathname,path);
    pathname[pathlen] = 0;
	process_relative_path(pathname);

    int index;
    index =  get_index(pathname); //(int)(pathname[1]-'0');
    if(index==-1){
        kprintf("pathname error!\n");
        return -1;
    }
//     for(int j=0;j<= pathlen-3;j++)
//     {
//         pathname[j] = pathname[j+3];
//     }
    state = vfs_table[index].op->deletedir(pathname);
    if (state >= 0) {
        kprintf("          delete dir success!");
    } else {
		DisErrorInfo(state);
    }   
    return state;
}

int do_vopendir(char *path)
{
	int fd = -1;
	char pathname[MAX_PATH];
	strcpy(pathname, path);
	process_relative_path(pathname);
	// 特判"/"
	if(strcmp(pathname, "/") == 0) {
		return 0; // todo: 待和姜峰讨论一下回到根目录的返回值，这本质是fd
		//reply: 实际上0已经被用了，返回0可能与stdin 混淆
	}
	if(strcmp(pathname, "/ram") == 0) {
		fd = vfs_table[VFS_INDEX_RAMFS].op->opendir(".");
	}
	else {
		int index = get_index(pathname);
		if(index == -1 || vfs_table[index].op->opendir == NULL) { // check opendir function ok
			return -1;
		}
		fd = vfs_table[index].op->opendir(pathname);
	}
	return fd;
}

int do_vchdir(const char *dirname)
{
	int stat = -1;
	char pathname[MAX_PATH];
	char new_cwd[MAX_PATH];
	strcpy(pathname, dirname);
	process_relative_path(pathname);
	// 特判"/"
	if(strcmp(pathname, "/") == 0) {
		strcpy(p_proc_current->task.cwd, pathname);
		return 0; 
		//no no no, change cwd dont need fd
	}
	// if(strcmp(pathname, "/ram") == 0) {
	// 	fd = vfs_table[VFS_INDEX_RAMFS].op->opendir(pathname+1);
	// }
	strcpy(new_cwd, pathname);
	int i;
	for(i = 0; i < NR_FS; i++ ) { // 如果找到的路径是第一级(fs_name),直接通过
		if(strcmp(pathname + 1, vfs_table[i].fs_name) == 0) {
			stat = 0;break;
		}
	}
	if(stat == -1) {
		int index = get_index(pathname);
		if(index == -1)
			return -1; // 修复bug：有可能用户一开始就输入了错误的路径
		int fd = vfs_table[index].op->opendir(pathname);
		if(fd < 0) {
			return -1;
		}
		if(index == VFS_INDEX_RAMFS)
			vfs_table[index].op->close(fd);
	}
	strcpy(p_proc_current->task.cwd, new_cwd);
	return 0;
}

int do_vmkdir(char *path) {
	// 目前目录文件只支持ramfs
	int pathlen = strlen(path);
	char pathname[MAX_PATH];
	strcpy(pathname, path);
	pathname[pathlen] = 0;
	process_relative_path(pathname);
	/* 由于目前的系统没有挂载mount功能，/ram文件夹是ramfs的根目录 */
	int index = get_index(pathname);
	return vfs_table[index].op->createdir(pathname);
}

int do_vlink(const char *oldpath, const char *newpath) {
	int oldpathlen = strlen(oldpath);
	int newpathlen = strlen(newpath);
	char oldpathname[MAX_PATH];
	char newpathname[MAX_PATH];
	strcpy(oldpathname, oldpath);
	strcpy(newpathname, newpath);
	oldpathname[oldpathlen] = 0;
	newpathname[newpathlen] = 0;
	process_relative_path(oldpathname);
	process_relative_path(newpathname);
	int index = get_index(oldpathname);
	get_index(newpathname);
	if(index == -1) {
		return -1;
	}
	return vfs_table[index].op->link(oldpathname, newpathname);
}

int do_vreaddir(int fd, char *buf, int count) {
	int index = p_proc_current->task.filp[fd]->dev_index;
	if(index == VFS_INDEX_RAMFS)
		return rf_readdir(fd, buf, count);
	return -1;
}


// 截图演示用
// char *pathname;
// int index;
// int fd;

// int do_vopen(const char *path, int flags) {
// 	/*...*/
// 	process_relative_path(pathname); // get absolute path
// 	/*...*/
//     index = get_index(pathname);
//     fd = vfs_table[index].op->open(pathname, flags);
//     /*...*/
//     return fd;    
// }