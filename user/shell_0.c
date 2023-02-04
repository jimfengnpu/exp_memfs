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
#define DATA_BUF_LEN 2000005
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
	pwd_u();
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
		printf("\ncat finished\n");
		close(fd);
    } else {
		close(fd);
		return -1;
	}
    
	return 0;
}

int ls_u() {
	get_cwd(tmp);
	int fd = opendir(tmp);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	lseek(fd, 0, SEEK_SET);
	while(read(fd, data_buf, sizeof(RAM_FS_RECORD)) == sizeof(RAM_FS_RECORD)) {
		RAM_FS_RECORD *p = (RAM_FS_RECORD *)data_buf;
		if(p->record_type == RF_F) {
			printf("file: %s\n", p->name);
		} else if(p->record_type == RF_D) {
			printf("dir: %s\n", p->name);
		}
	}
	close(fd);
	return 0; 
}

// Make the function an element of the array
// so that it can be called by the index
#define CMD_NUM 7
char *cmd_name[CMD_NUM] = {
	"pwd",
	"cd",
	"mkdir",
	"touch",
	"write",
	"cat",
	"ls",
};
int (*cmd_table[CMD_NUM])() = {
	pwd_u,
	chdir_u,
	mkdir_u,
	touch_u,
	write_u,
	cat_u,
	ls_u,
};


void cmd_parser() {
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

void fake_shell() {
	while (1)
	{
		printf("\nminiOS:/ $ ");
		if (gets(cmd_buf) && strlen(cmd_buf) != 0)
		{
			cmd_parser();
		}
	}
}

void cmd_dostr(char *str) {
	strcpy(cmd_buf, str);
	printf("test : %s\n", cmd_buf);
	cmd_parser();
}

// 进行基本的功能测试
void easytest() {
	cmd_dostr("mkdir ram");
	cmd_dostr("cd ram");
	cmd_dostr("touch a");
	cmd_dostr("write a hello world");
	cmd_dostr("cat a");
	cmd_dostr("write a change");
	cmd_dostr("cat a");
	cmd_dostr("cd /ram");
	cmd_dostr("mkdir b");
	cmd_dostr("mkdir c");
	// 检查是否会访问错误文件
	cmd_dostr("cd d");
	fake_shell();
}
// 高压力读写测试
void high_rw_test() {
	// 全部写入字符'a'
	for(int i = 0;i < 2e6;i++) data_buf[i] = 'a';
	data_buf[2000000] = '\0';
	int start_ticks = get_ticks();
	cmd_dostr("mkdir ram");
	cmd_dostr("cd ram");
	printf("write 2e6 bytes to file \"ram/all_a\"\n");
	int fd = open("all_a", O_CREAT | O_RDWR);
	lseek(fd, 0, SEEK_SET);
	int ret = -1;
	if((ret = write(fd, data_buf, strlen(data_buf))) != 2000000) {
		printf("write failed\n");
	} else {
		printf("successfully written\n");
	}
	// 随机读测试
	lseek(fd, 0, SEEK_SET);
	// 读入1000个bytes检查
	char tmp[10];
	int cnt = 0;
	while(read(fd, tmp, 1) == 1 && cnt != 1000) {
		cnt++;
		if(tmp[0] != 'a') {
			printf("find a read problem in 'all_a' test\n");
			return;
		}
	}
	if(cnt != 1000) {
		printf("test error\n");
		return;
	}
	printf("\"Read 1000 bytes from head\" test is passed\n");
	lseek(fd, 114514, SEEK_CUR); // 随机读取测试
	cnt = 0;
	while(read(fd, tmp, 1) == 1 && cnt != 1000) {
		cnt++;
		if(tmp[0] != 'a') {
			printf("find a read problem in 'all_a' test\n");
			return;
		}
	}
	if(cnt != 1000) {
		printf("test error\n");
		return;
	}
	printf("\"Randomly read 1000 bytes from head\" test is passed\n");
	lseek(fd, -5000, SEEK_END);
	cnt = 0;
	while(read(fd, tmp, 1) == 1 && cnt != 1000) {
		cnt++;
		if(tmp[0] != 'a') {
			printf("find a read problem in 'all_a' test\n");
			return;
		}
	}
	if(cnt != 1000) {
		printf("test error\n");
	}
	printf("\"READ 1000 bytes from end\" test is passed\n");
	int end_ticks = get_ticks();
	close(fd);
	printf("all_a test is passed\n");
	printf("This test use %d ticks\n", end_ticks-start_ticks);
	cmd_dostr("cd /");
	fd = open("orange/test", O_CREAT | O_RDWR);
	lseek(fd, 0, SEEK_SET);
	start_ticks = get_ticks();
	write(fd, data_buf, strlen(data_buf));
	end_ticks = get_ticks();
	printf("Wriet 2e6 bytes to hard disk: This test use %d ticks\n", end_ticks-start_ticks);
}

// int fattest() {
// 	char filename[] = "orange/test";
// 	int fd = open(filename, O_CREAT | O_RDWR);
// 	if(fd == -1) {
// 		printf("open failed\n");
// 	} else {
// 		printf("oepn %s success\n", filename);
// 	}
// 	lseek(fd, 0, SEEK_SET);
// 	write(fd, "hello world", strlen("hello world"));
// 	lseek(fd, 0, SEEK_SET);
// 	read(fd, data_buf, DATA_BUF_LEN);
// 	printf("data buf : %s\n", data_buf);
// 	return 0;
// }

int main(int argc, char *argv[])
{
	int stdin = open("dev_tty0", O_RDWR);
	int stdout = open("dev_tty0", O_RDWR);
	int stderr = open("dev_tty0", O_RDWR);
	// fattest();
	// fake_shell();
	// easytest();
	high_rw_test();


	// testDir();
	// while(1) {
	// 	fake_shell();
	// 	test();
	// }
}
