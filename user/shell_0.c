// #include "type.h"
// #include "const.h"
// #include "protect.h"
// #include "string.h"
// #include "proc.h"
// #include "global.h"
// #include "proto.h"
// #include "stdio.h"

// int main(int arg, char *argv[])
// {
// 	int stdin = open("dev_tty0", O_RDWR);
// 	int stdout = open("dev_tty0", O_RDWR);
// 	int stderr = open("dev_tty0", O_RDWR);

// 	char buf[1024];
// 	int pid;
// 	int times = 0;
// 	exec("orange/file_test.bin");
// // 	while (1)
// // 	{
// // 		printf("\nminiOS:/ $ ");
// // 		// debug
// // 		// exec("orange/file_test.bin");
// // 		if (gets(buf) && strlen(buf) != 0)
// // 		{
			
// // 			if (exec(buf) != 0)
// // 			{
// // 				printf("exec failed: file not found!\n");
// // 				continue;
// // 			}
// // 		}
// // 	}
// }

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "stdio.h"
#include "vfs.h"
#include "assert.h"
#include "ramfs.h"

#define MAX_CMD_LEN 600
char cmd_buf[MAX_CMD_LEN];
#define TMP_BUF_LEN 600
char tmp[TMP_BUF_LEN];
#define DATA_BUF_LEN 600
char data_buf[DATA_BUF_LEN];

int pwd_u() {
	int ret = get_cwd(tmp);
	printf("cwd: %s\n", tmp);
	return ret;
}

int chdir_u() {
	strcpy(tmp, cmd_buf+3);
	if(chdir(tmp) == -1) {
		printf("chdir failed\n");
		return -1;
	}
	// pwd_u();
	return 0;
}

int mkdir_u() {
	strcpy(tmp, cmd_buf+6); // get file dir name
	if(mkdir(tmp) == -1) {
		printf("mkdir failed\n");
		return -1;
	}
	else 
		printf("mkdir %s finished\n", tmp);
	return 0;
}

int touch_u() {
	strcpy(tmp, cmd_buf+6);
	int fd = open(tmp, O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	printf("touch %s finished\n", tmp);
	return 0;
}

int write_u() {
	// this func can support this kind of sentence:
	// write a hello world
    int i;
	for(i = 6;i < strlen(cmd_buf);i++) {
		if(cmd_buf[i] != ' ') {
			tmp[i-6] = cmd_buf[i];
		} else {
			tmp[i-6] = '\0';
			break;
		}
	}
	int fd = open(tmp, O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	int ret = write(fd, cmd_buf+i+1, strlen(cmd_buf)-i-1);
	if (ret == -1) {
		printf("write failed\n");
		return -1;
	}
	close(fd);
	return 0;
}

int cat_u() {
	strcpy(tmp, cmd_buf+4);
    printf("cat %s\n", tmp);
	int fd = open(tmp, O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	int ret = read(fd, cmd_buf, MAX_CMD_LEN);
	if (ret == -1) {
		printf("read failed\n");
		return -1;
	}
	else if(ret >= 0) {
		cmd_buf[ret] = '\0';
		printf("%s", cmd_buf);
        printf("\n data len: %d\n", ret);
		close(fd);
    } else 
		return -1;
    
	return 0;
}

int ls_u() {
	get_cwd(tmp);
	if(cmd_buf[3]!= '\0') {
		strcpy(tmp, cmd_buf + 3);
	}
	int fd = opendir(tmp);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	while(read(fd, data_buf, sizeof(rf_record)) == sizeof(rf_record)) {
		rf_record *p = (rf_record *)data_buf;
		if(p->record_type == RF_F) {
			printf("file: %s size:%d\n", p->name, p->size);
		} else if(p->record_type == RF_D) {
			printf("dir: %s size:%d\n", p->name, p->size);
		}
	}
	close(fd);
	return 0; 
}

int exec_u() {
	exec(cmd_buf+5);
	return 0;
}
// Make the function an element of the array
// so that it can be called by the index
#define CMD_NUM 8
char *cmd_name[CMD_NUM] = {
	"pwd",
	"cd",
	"mkdir",
	"touch",
	"write",
	"cat",
	"ls",
	"exec",
};
int (*cmd_table[CMD_NUM])() = {
	pwd_u,
	chdir_u,
	mkdir_u,
	touch_u,
	write_u,
	cat_u,
	ls_u,
	exec_u,
};

void easytest() {
	strcpy(cmd_buf, "mkdir ram/test");
	mkdir_u();
	strcpy(cmd_buf, "cd ram/test");
	chdir_u();
	strcpy(cmd_buf, "touch a");
	touch_u();
	strcpy(cmd_buf, "write a hello world");
    printf("started write\n");
	write_u();
	strcpy(cmd_buf, "cat a");
	cat_u();


	// printf("hello, this is file test\n");
	// int fd = open("ram/test.txt", O_RDWR | O_CREAT);
	// if (fd == -1) {
	// 	printf("open failed\n");
	// 	return;
	// }
	// int ret = write(fd, "hello world", 11);
	// if (ret == -1) {
	// 	printf("write failed\n");
	// 	return;
	// }
	// ret = lseek(fd, 0, SEEK_SET);
	// ret = read(fd, cmd_buf, 11);
	// cmd_buf[11] = '\0';
	// if (ret == -1) {
	// 	printf("read failed\n");
	// 	return;
	// }
	// printf("%s\n", cmd_buf);
	// printf("test finished\n");
}

void fake_shell() {
	while (1)
	{
		memset(cmd_buf, 0, MAX_CMD_LEN);
		get_cwd(tmp);
		printf("\nminiOS:%s $ ",tmp);
		if (gets(cmd_buf) && strlen(cmd_buf) != 0)
		{
			int i;
			for (i = 0; i < CMD_NUM; i++)
			{
				if(strlen(cmd_name[i]) <= strlen(cmd_buf) && strncmp(cmd_name[i], cmd_buf, strlen(cmd_name[i])) == 0)
				{
					cmd_table[i]();
					break;
				}
			}
			if (i == CMD_NUM)
			{
				printf("command not found\n");
			}
		}
	}
}


int main(int argc, char *argv[])
{
	int stdin = open("dev_tty0", O_RDWR);
	int stdout = open("dev_tty0", O_RDWR);
	int stderr = open("dev_tty0", O_RDWR);
	fake_shell();
	// easytest();
	// testDir();
	// while(1) {
	// 	fake_shell();
	// 	test();
	// }
}
