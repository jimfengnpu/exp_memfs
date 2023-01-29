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
char buf[256];


void easytest() {
	printf("hello, this is file test\n");
	int fd = open("ram/test.txt", O_RDWR | O_CREAT);
	if (fd == -1) {
		printf("open failed\n");
		return;
	}
	int ret = write(fd, "hello world", 11);
	if (ret == -1) {
		printf("write failed\n");
		return;
	}
	ret = lseek(fd, 0, SEEK_SET);
	ret = read(fd, buf, 11);
	buf[11] = '\0';
	if (ret == -1) {
		printf("read failed\n");
		return;
	}
	printf("%s\n", buf);
	printf("test finished\n");
}

void pwd_u() {
	char tmp[40];
	get_cwd(tmp);
	printf("cwd: %s\n", tmp);
}

void chdir_u() {
	char tmp[40];
	strcpy(tmp, buf+3);
	if(chdir(tmp) == -1) {
		printf("chdir failed\n");
	}
	pwd_u();
}

void fake_shell() {
	while (1)
	{
		printf("\nminiOS:/ $ ");
		if (gets(buf) && strlen(buf) != 0)
		{
			if(strcmp(buf, "pwd") == 0) {
				pwd_u();
			}
			else if(exec(buf) != 0) {
				printf("exec %s failed\n", buf);
			}
		}
	}
}

void testDir() {
	// mkdir("ram/test");
	if(mkdir("ram/test") == -1)
		printf("mkdir failed\n");
	else 
		printf("mkdir ram/test finished\n");
	if(chdir("ram/test") == -1) 
		printf("chdir failed\n");
	else 
		printf("chdir ram/test finished\n");
	get_cwd(buf);
	printf("cwd: %s\n", buf);
	printf("dir test finished\n");
}

int main(int argc, char *argv[])
{
	testDir();
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
