// #include "type.h"
// #include "const.h"
// #include "protect.h"
// #include "string.h"
// #include "proc.h"
// #include "global.h"
// #include "proto.h"
// #include "stdio.h"
// #include "vfs.h"
// #include "assert.h"

// #define MAX_CMD_LEN 256
// char cmd_buf[MAX_CMD_LEN];
// #define TMP_BUF_LEN 256

// int pwd_u() {
// 	char tmp[TMP_BUF_LEN];
// 	int ret = get_cwd(tmp);
// 	printf("cwd: %s\n", tmp);
// 	return ret;
// }

// int chdir_u() {
// 	char tmp[TMP_BUF_LEN];
// 	strcpy(tmp, cmd_buf+3);
// 	if(chdir(tmp) == -1) {
// 		printf("chdir failed\n");
// 		return -1;
// 	}
// 	pwd_u();
// 	return 0;
// }

// int mkdir_u() {
// 	char tmp[TMP_BUF_LEN];
// 	strcpy(tmp, cmd_buf+6);
// 	if(mkdir(tmp) == -1) {
// 		printf("mkdir failed\n");
// 		return -1;
// 	}
// 	else 
// 		printf("mkdir %s finished\n", tmp);
// 	return 0;
// }

// int touch_u() {
// 	char tmp[TMP_BUF_LEN];
// 	get_cwd(tmp);
// 	strcat(tmp, "/");
// 	strcat(tmp, cmd_buf+6);
// 	int fd = open(tmp, O_RDWR | O_CREAT);
// 	if (fd == -1) {
// 		printf("open failed\n");
// 		return -1;
// 	}
// 	else 
// 		printf("touch %s finished\n", tmp);
// 	return 0;
// }

// int write_u() {
// 	char tmp[TMP_BUF_LEN];
// 	strcpy(tmp, cmd_buf+6);
// 	int fd = open(tmp, O_RDWR | O_CREAT);
// 	if (fd == -1) {
// 		printf("open failed\n");
// 		return -1;
// 	}
// 	int ret = write(fd, "hello world", 11);
// 	if (ret == -1) {
// 		printf("write failed\n");
// 		return -1;
// 	}
// 	else 
// 		printf("write %s finished\n", tmp);
// 	return 0;
// }

// int cat_u() {
// 	char tmp[TMP_BUF_LEN];
// 	strcpy(tmp, cmd_buf+4);
// 	int fd = open(tmp, O_RDWR | O_CREAT);
// 	if (fd == -1) {
// 		printf("open failed\n");
// 		return -1;
// 	}
// 	lseek(fd, 0, SEEK_SET);
// 	int ret = read(fd, cmd_buf, MAX_CMD_LEN);
// 	if (ret == -1) {
// 		printf("read failed\n");
// 		return -1;
// 	}
// 	else if(ret >= 0) {
// 		cmd_buf[ret] = '\0';
// 		printf("%s", cmd_buf);
// 	} else 
// 		return -1;
// 	return 0;
// }

// // Make the function an element of the array
// // so that it can be called by the index
// #define CMD_NUM 6
// char *cmd_name[CMD_NUM] = {
// 	"pwd",
// 	"cd",
// 	"mkdir",
// 	"touch",
// 	"write",
// 	"cat",
// };
// int (*cmd_table[CMD_NUM])() = {
// 	pwd_u,
// 	chdir_u,
// 	mkdir_u,
// 	touch_u,
// 	write_u,
// 	cat_u,
// };

// void easytest() {
// 	printf("hello, this is file test\n");
// 	int fd = open("ram/test.txt", O_RDWR | O_CREAT);
// 	if (fd == -1) {
// 		printf("open failed\n");
// 		return;
// 	}
// 	int ret = write(fd, "hello world", 11);
// 	if (ret == -1) {
// 		printf("write failed\n");
// 		return;
// 	}
// 	ret = lseek(fd, 0, SEEK_SET);
// 	ret = read(fd, cmd_buf, 11);
// 	cmd_buf[11] = '\0';
// 	if (ret == -1) {
// 		printf("read failed\n");
// 		return;
// 	}
// 	printf("%s\n", cmd_buf);
// 	printf("test finished\n");
// }

// void fake_shell() {
// 	while (1)
// 	{
// 		printf("\nminiOS:/ $ ");
// 		if (gets(cmd_buf) && strlen(cmd_buf) != 0)
// 		{
// 			int i;
// 			for (i = 0; i < CMD_NUM; i++)
// 			{
// 				if(strlen(cmd_name[i]) <= strlen(cmd_buf) && strncmp(cmd_name[i], cmd_buf, strlen(cmd_name[i])) == 0)
// 				{
// 					cmd_table[i]();
// 					break;
// 				}
// 			}
// 			if (i == CMD_NUM)
// 			{
// 				printf("command not found\n");
// 			}
// 		}
// 	}
// }

// void testDir() {
// 	// mkdir("ram/test");
// 	if(mkdir("ram/test") == -1)
// 		printf("mkdir failed\n");
// 	else 
// 		printf("mkdir ram/test finished\n");
// 	if(chdir("ram/test") == -1) 
// 		printf("chdir failed\n");
// 	else 
// 		printf("chdir ram/test finished\n");
// 	get_cwd(cmd_buf);
// 	printf("cwd: %s\n", cmd_buf);
// 	printf("dir test finished\n");
// }

// int main(int argc, char *argv[])
// {
// 	printf("hello, this is file test\n");
// 	fake_shell();
// 	// testDir();
// 	// while(1) {
// 	// 	fake_shell();
// 	// 	test();
// 	// }

// 	// int fd;
// 	// printf("cmd_char [path]\n");
// 	// printf("w write\nr read\nd delete\n");
// 	// while (1)
// 	// {
// 	// 	gets(buf);
// 	// 	switch (buf[0])
// 	// 	{
// 	// 	case 'r':
// 	// 		fd =open(buf + 2, O_RDWR);
// 	// 		if(fd==-1)
// 	// 			printf("err,maybe not exist\n");
// 	// 		else{
// 	// 			read(fd, buf, 25);
// 	// 			printf("%s\n", buf);
// 	// 		}
// 	// 		break;
// 	// 	case 'w':
// 	// 		fd = open(buf + 2, O_RDWR|O_CREAT);
// 	// 		gets(buf);
// 	// 		write(fd, buf, strlen(buf));
// 	// 		break;
// 	// 	case 'd':
// 	// 		if(delete(buf + 2)!=1)deletedir(buf+2);
// 	// 		break;
// 	// 	default:
// 	// 		break;
// 	// 	}
// 	// }
// }

#include "stdio.h"

int main() {
	printf("hello world\n");
}