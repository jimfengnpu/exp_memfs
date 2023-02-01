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

#define MAX_CMD_LEN 600
char cmd_buf[MAX_CMD_LEN];
#define TMP_BUF_LEN 600
char tmp[TMP_BUF_LEN];

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
	pwd_u();
	return 0;
}

int mkdir_u() {
	
	strcpy(tmp, cmd_buf+6);
	if(mkdir(tmp) == -1) {
		printf("mkdir failed\n");
		return -1;
	}
	else 
		printf("mkdir %s finished\n", tmp);
	return 0;
}

int touch_u() {
	
	get_cwd(tmp);
	strcat(tmp, "/");
	strcat(tmp, cmd_buf+6);
	printf("debug touch_u : tmp : %s\n", tmp);
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
	get_cwd(tmp);
	printf("%s\n", tmp);
	strcat(tmp, "/");
	int tmp_len = strlen(tmp);
    int pre_len = tmp_len;
	for(int i = 6; i < strlen(cmd_buf); i++) {
		tmp[tmp_len++] = cmd_buf[i];
		if(cmd_buf[i] == ' ') {
			tmp[tmp_len-1] = '\0';
			break;
		}
	}
	printf("tmp: %s\n");
	int fd = open(tmp, O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	int write_len = strlen(cmd_buf) - (tmp_len-pre_len+6);
    printf("write string: %s\n", cmd_buf+tmp_len-pre_len+6);
	int ret = write(fd, cmd_buf+tmp_len-pre_len+6, write_len);
	if (ret == -1) {
		printf("write failed\n");
		return -1;
	}
	else if(ret < write_len) {
		printf("write %s failed, only %d bytes written\n", tmp, ret);
		return -1;
	}
	else
		printf("write %s finished\n", tmp);
    close(fd);
	return 0;
}

int cat_u() {
	get_cwd(tmp);
	strcat(tmp, "/");
	strcat(tmp, cmd_buf+4);
    printf("cat tmp %s\n", tmp);
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
        close(fd);
    } else 
		return -1;
    
	return 0;
}

// Make the function an element of the array
// so that it can be called by the index
#define CMD_NUM 6
char *cmd_name[CMD_NUM] = {
	"pwd",
	"cd",
	"mkdir",
	"touch",
	"write",
	"cat",
};
int (*cmd_table[CMD_NUM])() = {
	pwd_u,
	chdir_u,
	mkdir_u,
	touch_u,
	write_u,
	cat_u,
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
		printf("\nminiOS:/ $ ");
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

	// int fd;
	// printf("cmd_char [path]\n");
	// printf("w write\nr read\nd delete\n");
	// while (1)
	// {
	// 	gets(buf);
	// 	switch (buf[0])
	// 	{
	// 	case 'r':
	// 		fd =open(buf + 2, O_RDWR);
	// 		if(fd==-1)
	// 			printf("err,maybe not exist\n");
	// 		else{
	// 			read(fd, buf, 25);
	// 			printf("%s\n", buf);
	// 		}
	// 		break;
	// 	case 'w':
	// 		fd = open(buf + 2, O_RDWR|O_CREAT);
	// 		gets(buf);
	// 		write(fd, buf, strlen(buf));
	// 		break;
	// 	case 'd':
	// 		if(delete(buf + 2)!=1)deletedir(buf+2);
	// 		break;
	// 	default:
	// 		break;
	// 	}
	// }
}
