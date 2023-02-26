/*
 * To test if new kernel features work normally, and if old features still 
 * work normally with new features added.
 * added by xw, 18/4/27
 */
#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "fs.h"
#include "vfs.h"
#include "string.h"

/**
 * @struct posix_tar_header
 * Borrowed from GNU `tar'
 */
//added by mingxuan 2019-5-18
struct posix_tar_header
{				/* byte offset */
	char name[100];		/*   0 */
	char mode[8];		/* 100 */
	char uid[8];		/* 108 */
	char gid[8];		/* 116 */
	char size[12];		/* 124 */
	char mtime[12];		/* 136 */
	char chksum[8];		/* 148 */
	char typeflag;		/* 156 */
	char linkname[100];	/* 157 */
	char magic[6];		/* 257 */
	char version[2];	/* 263 */
	char uname[32];		/* 265 */
	char gname[32];		/* 297 */
	char devmajor[8];	/* 329 */
	char devminor[8];	/* 337 */
	char prefix[155];	/* 345 */
	/* 500 */
};

/*****************************************************************************
 *                                untar
 * added by mingxuan 2019-5-18
 *****************************************************************************/
/**
 * Extract the tar file and store them.
 * 
 * @param filename The tar file.
 *****************************************************************************/
static void untar(const char * filename)
{
	printf("[extract %s \n", filename);
	int fd = do_vopen(filename, O_RDWR);//modified by mingxuan 2019-5-20

	char buf[512 * 16];
	int chunk = sizeof(buf);
	int i = 0;

	while (1) {
		do_vread(fd, buf, 512); //modified by mingxuan 2019-5-21
		if (buf[0] == 0) {
			if (i == 0)
				printf("    need not unpack the file.\n");
			break;
		}
		i++;

		struct posix_tar_header * phdr = (struct posix_tar_header *)buf;

		/* calculate the file size */
		char * p = phdr->size;
		int f_len = 0;
		while (*p)
			f_len = (f_len * 8) + (*p++ - '0'); /* octal */

		int bytes_left = f_len;
		char full_name[30] = "orange/";
		strcat(full_name,phdr->name);
		int fdout = do_vopen(full_name, O_CREAT | O_RDWR );	//modified by mingxuan 2019-5-20
		if (fdout == -1) {
			printf("    failed to extract file: %s\n", phdr->name);
			printf(" aborted]\n");
			do_vclose(fd);	//modified by mingxuan 2019-5-20
			return;
		}
		printf("    %s \n", full_name);	//deleted by mingxuan 2019-5-22
		while (bytes_left) {
			int iobytes = min(chunk, bytes_left);
			
			do_vread(fd, buf, ((iobytes - 1) / 512 + 1) * 512);	//modified by mingxuan 2019-5-21
			
			do_vwrite(fdout, buf, iobytes); //modified by mingxuan 2019-5-21
			bytes_left -= iobytes;
		}
		do_vclose(fdout);	//modified by mingxuan 2019-5-20
	}

	if (i) {
		do_vlseek(fd, 0, SEEK_SET); //modified by mingxuan 2019-5-20
		buf[0] = 0;
		do_vwrite(fd, buf, 1);	//modified by mingxuan 2019-5-20
	}

	do_vclose(fd);	//modified by mingxuan 2019-5-21

	printf(" done, %d files extracted]\n", i);
}

// 对ramdisk的单元测试
void mod_test_RWSector() {
	BYTE buf[512];
	for(int i = 0;i < 1000;i++) buf[i] = (i%26)+'a';
	WriteSector(buf, 28);
	BYTE checkbuf[512];
	ReadSector(checkbuf, 28);
	for(int i = 0;i < 512;i++) 
		if(buf[i] != checkbuf[i]) {
			printf("mod_test_RWSector failed at %d\n", i);
			return;
		}
	printf("mod_test_RWSector passed\n");
}


void fat_on_ram_easy_test() {
	int fd = do_vopen("/fat0/test.txt", O_CREAT | O_RDWR);
	char buf[1100];
	// lseek(fd, 0, SEEK_SET);
	do_vwrite(fd, "hello world", 11);
	do_vclose(fd);
	fd = do_vopen("/fat0/test.txt", O_RDWR);
	do_vread(fd, buf, 11);
	buf[11] = '\0';
	printf("read from fat0/test: %s\n", buf);
	do_vclose(fd);
	if(strcmp(buf, "hello world") == 0) {
		printf("fat_on_ram_easy_test passed\n");
	} else {
		printf("fat_on_ram_easy_test failed\n");
		while (1);
	}
}

void orange_on_ram_easy_test() {
	int fd = do_vopen("/orange/test.txt", O_CREAT | O_RDWR);
	char buf[1100];
	do_vlseek(fd, 0, SEEK_SET);
	do_vwrite(fd, "hello world", 11);
	do_vclose(fd);
	fd = do_vopen("/orange/test.txt", O_RDWR);
	do_vread(fd, buf, 11);
	printf("read from orange/test: %s\n", buf);
	do_vclose(fd);
	if(strcmp(buf, "hello world") == 0) {
		printf("orange_on_ram_easy_test passed\n");
	} else {
		printf("orange_on_ram_easy_test failed\n");
		while (1);
	}
}

void initial()
{
	
	int stdin = do_vopen("dev_tty0",O_RDWR);
	int stdout= do_vopen("dev_tty0",O_RDWR);
	int stderr= do_vopen("dev_tty0",O_RDWR);

	//untar(INSTALL_FILENAME);
	//modified by mingxuan 2019-5-21
	char full_name[30]="orange/";
	// p_proc_current->task.cwd = "orange";
	// memcpy(p_proc_current->task.cwd, "orange\0", strlen("orange\0"));
	printf("untar:%s\n",full_name);
	strcat(full_name,INSTALL_FILENAME);
	untar(full_name);

	// do_vclose(stdin);
	// do_vclose(stdout);
	// do_vclose(stderr);

	int pid = fork();
	if(!pid) {
		exec("orange/shell_0.bin");
	}
	else {
		int st;
		while(1) {
			wait(&st);
		}
	}


}