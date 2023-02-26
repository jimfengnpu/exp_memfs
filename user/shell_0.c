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
#include "shell.h"

#define MAX_CMD_LEN 600
char cmd_buf[MAX_CMD_LEN];
#define TMP_BUF_LEN 600
char tmp[TMP_BUF_LEN];
#define DATA_BUF_LEN 1020000
char data_buf[DATA_BUF_LEN];

int check_num = 0; // 每做一次check_expr，check_num就加1，以便快速定位错误
int check_expr(int val) {
	check_num++;
	if(val != 1) {
		printf("check %d failed\n", check_num);
		while(1);
		return -1;
	}
	return 0;
}


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
	int fd = create(tmp/*, O_RDWR | O_CREAT*/);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	printf("touch %s finished\n", tmp);
	close(fd);
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
	// lseek(fd, 0, SEEK_SET);fat0 has no lseek
	int ret = write(fd, cmd_buf+i+1, strlen(cmd_buf)-i-1);
	if (ret == -1) {
		printf("write failed\n");
		return -1;
	}
	close(fd);
	return ret;
}

int cat_u() {
	strcpy(tmp, cmd_buf+4);
	int fd = open(tmp, O_RDWR);
	if (fd == -1) {
		printf("open failed\n");
		return -1;
	}
	// lseek(fd, 0, SEEK_SET);fat0 has no lseek
	int ret = read(fd, cmd_buf, MAX_CMD_LEN);
	if (ret == -1) {
		printf("read failed\n");
		return -1;
	}
	else if(ret >= 0) {
		cmd_buf[ret] = '\0';
		printf("%s\n", cmd_buf);
		close(fd);
    } else {
		close(fd);
		return -1;
	}
	return ret;
}

int ls_u() {
	get_cwd(tmp);
	if(cmd_buf[3]!= '\0') { // 支持直接ls与ls dir
		strcpy(tmp, cmd_buf + 3);
	}
	int fd = opendir(tmp);
	readdir(fd, data_buf, DATA_BUF_LEN);
	printf("%s\n", data_buf);
	close(fd);
	return 0; 
}

int exit_u() {
	exit(0);
	return 0;
}

int rm_u() {
	if(unlink(cmd_buf+3) < 0) {
		printf("rm failed\n");
		return -1;
	} else {
		printf("rm %s finished\n", cmd_buf+3);
		return 0;
	}
}

int rmdir_u() {
	if(unlink(cmd_buf+6) < 0) {
		printf("rm failed\n");
		return -1;
	} else {
		printf("rm %s finished\n", cmd_buf+6);
		return 0;
	}
}

int link_u() {
	char oldpath[512], newpath[512];
	int i;
	for(i = 5;i < strlen(cmd_buf);i++) {
		if(cmd_buf[i] != ' ') {
			oldpath[i-5] = cmd_buf[i];
		} else {
			oldpath[i-5] = '\0';
			break;
		}
	}
	if(cmd_buf[i+1] == '\0') {
		printf("link failed\n");
		return -1;
	}
	strcpy(newpath, cmd_buf+i+1);
	printf("oldpath: %s newpath: %s\n", oldpath, newpath);
	if(link(oldpath, newpath) < 0) {
		printf("link failed\n");
		return -1;
	} else {
		printf("link finished\n");
		return 0;
	}
}

// Make the function an element of the array 
// so that it can be called by the index
#define CMD_NUM 11
char *cmd_name[CMD_NUM] = {
	"pwd",
	"cd",
	"mkdir",
	"touch",
	"write",
	"cat",
	"ls",
	"exit",
	"rm",
	"rmdir",
	"link",
};
int (*cmd_table[CMD_NUM])() = {
	pwd_u,
	chdir_u,
	mkdir_u,
	touch_u,
	write_u,
	cat_u,
	ls_u,
	exit_u,
	rm_u,
	rmdir_u,
	link_u,
};

int cmd_parser() {
	printf("cmd_buf: %s\n", cmd_buf);
	int i;
	for (i = 0; i < CMD_NUM; i++)
	{
		if(strlen(cmd_name[i]) <= strlen(cmd_buf) && strncmp(cmd_name[i], cmd_buf, strlen(cmd_name[i])) == 0
		&& (cmd_buf[strlen(cmd_name[i])] == ' ' || cmd_buf[strlen(cmd_name[i])] == '\0'))
		// 关于这parser，第2行是为了避免因为前缀匹配导致的不一致，如rm和rmdir
		{
			return cmd_table[i]();
		}
	}
	// if (i == CMD_NUM)
	// {
	// 	printf("command not found\n");
	// }
	return -CMD_NOFOUND;
}

void fake_shell() {
	while (1)
	{
		memset(cmd_buf, 0, MAX_CMD_LEN);
		get_cwd(tmp);
		printf("\nminiOS:%s $ ",tmp);
		if (gets(cmd_buf) && strlen(cmd_buf) != 0)
		{
			cmd_parser();
		}
	}
}

// int cmd_dostr(char *str) {
// 	strcpy(cmd_buf, str);
// 	printf("command : %s\n", cmd_buf);
// 	return cmd_parser();
// }

// 进行基本的功能测试
int easytest() {
	// check_expr(cmd_dostr("cd qweqw") < 0); // 测试用户输入的目录不存在
	// check_expr(cmd_dostr("cd ram") >= 0);
	// check_expr(cmd_dostr("touch a") >= 0);
	// check_expr(cmd_dostr("write a hello world") == strlen("hello world"));
	// check_expr(cmd_dostr("cat a") >= 0);
	// check_expr(cmd_dostr("write a change") == strlen("change"));
	// check_expr(cmd_dostr("cat a") >= 0);
	// check_expr(cmd_dostr("cd /ram") >= 0);
	// check_expr(cmd_dostr("mkdir b") >= 0);
	// check_expr(cmd_dostr("mkdir c") >= 0);
	// check_expr(cmd_dostr("cd b") >= 0);
	// check_expr(cmd_dostr("cd /ram/d") < 0); 	// 检查访问不存在的目录
	// check_expr(cmd_dostr("cd /ram") >= 0);
	// // 删除文件夹
	// check_expr(cmd_dostr("rm /ram/a") >= 0);
	// check_expr(cmd_dostr("rmdir /ram/b") >= 0);
	// check_expr(cmd_dostr("rmdir /ram/c") >= 0);
	strcpy(cmd_buf, "cd qweqw"); check_expr(cmd_parser() < 0); // 测试用户输入的目录不存在
	strcpy(cmd_buf, "cd ram"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "touch a"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "write a hello world"); check_expr(cmd_parser() == strlen("hello world"));
	strcpy(cmd_buf, "cat a"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "write a change"); check_expr(cmd_parser() == strlen("change"));
	strcpy(cmd_buf, "cat a"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "cd /ram"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "mkdir b"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "mkdir c"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "cd b"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "cd /ram/d"); check_expr(cmd_parser() < 0); 	// 检查访问不存在的目录
	strcpy(cmd_buf, "cd /ram"); check_expr(cmd_parser() >= 0);
	// 删除文件夹
	strcpy(cmd_buf, "rm /ram/a"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "rmdir /ram/b"); check_expr(cmd_parser() >= 0);
	strcpy(cmd_buf, "rmdir /ram/c"); check_expr(cmd_parser() >= 0);
	printf("easy_test pass!!!\n");
	return 0;
}


// 向ram/all_a写入2e6个字符'a'后，随机读取文件，进行检测。
int all_a_test() {
	int tot_len = 5e5;
	for(int i = 0;i < tot_len;i++) data_buf[i] = 'a';
	data_buf[tot_len] = '\0';
	check_expr(chdir("/ram") >= 0); // 切换到ram目录
	int fd = -1;
	fd = open("all_a", O_CREAT | O_RDWR);
	check_expr(fd >= 0); // 创建文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	check_expr(write(fd, data_buf, strlen(data_buf)) == tot_len); // 写入2e6个字符'a'
	check_expr(close(fd) >= 0); // 关闭文件
	fd = open("all_a", O_RDWR);
	check_expr(fd >= 0); // 重新打开文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	int ret;
	for(int i = 0;i < 100;i++) {
		ret = read(fd, data_buf, 1);
		check_expr(ret == 1); // 读取一个字符
		check_expr(data_buf[0] == 'a'); // 检查读取的字符是否为'a'
	}
	check_expr(lseek(fd, 11414, SEEK_SET) >= 0);
	for(int i = 0;i < 100;i++) {
		ret = read(fd, data_buf, 1);
		check_expr(ret == 1); // 读取一个字符
		check_expr(data_buf[0] == 'a'); // 检查读取的字符是否为'a'
	}
	check_expr(lseek(fd, -1000, SEEK_END) >= 0);
	for(int i = 0;i < 100;i++) {
		ret = read(fd, data_buf, 1);
		check_expr(ret == 1); // 读取一个字符
		check_expr(data_buf[0] == 'a'); // 检查读取的字符是否为'a'
	}
	check_expr(delete("all_a") >= 0); // 删除文件
	printf("all_a_test pass!!!\n");
	return 0;
}

int check_alphabet(int fd, int *ret, int *cur_pos) {
	for(int i = 0;i < 100;i++) {
		*ret = read(fd, data_buf, 1);
		if(*ret != 1 || data_buf[0] != 'a'+((*cur_pos)%26))
			return 0;
		(*cur_pos)++;
	}
	return 1;
}
/*
 *向ram/alphabet_copy文件写入字母表，并读取。
 *检查方法：根据下表位置取模得到应为何字符，并对比。
**/
int alphabet_copy_test() {
	check_expr(chdir("/ram") >= 0); // 切换到ram目录
	int fd = -1;
	int tot_len = 5e5;
	fd = open("alphabet_copy", O_CREAT | O_RDWR);
	check_expr(fd >= 0); // 创建文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	for(int i = 0;i < tot_len;i++) {
		data_buf[i] = 'a' + (i%26);
	}
	data_buf[tot_len] = '\0';
	check_expr(write(fd, data_buf, strlen(data_buf)) == tot_len); // 写入tot_len个字符
	check_expr(close(fd) >= 0); // 关闭文件
	fd = open("alphabet_copy", O_RDWR);
	check_expr(fd >= 0); // 重新打开文件
	int check_state = 0, cur_pos = 0, ret = -1;
// 将文件指针移动到文件开头
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); 
	cur_pos = 0;
	check_expr(check_alphabet(fd, &ret, &cur_pos) == 1);
// 将文件指针移动到文件中间
	check_expr(lseek(fd, 12133, SEEK_SET) >= 0);
	cur_pos = 121323;
	check_expr(check_alphabet(fd, &ret, &cur_pos) == 1);
// 将文件指针移动到文件末尾
	check_expr(lseek(fd, -1000, SEEK_END) >= 0);
	cur_pos = tot_len - 1000;
	check_expr(check_alphabet(fd, &ret, &cur_pos) == 1);
	check_expr(close(fd) >= 0); // 关闭文件
	check_expr(delete("alphabet_copy") >= 0); // 删除文件
	printf("alphabet_copy_test pass!!!\n");
	return 0;
}

int test_write_times(char *filename, int tot_len) {
	int start_ticks = get_ticks();
	int fd = open(filename, O_CREAT | O_RDWR);
	check_expr(fd >= 0); // 创建文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	check_expr(write(fd, data_buf, strlen(data_buf)) == tot_len); // 写入tot_len个字符
	check_expr(close(fd) >= 0); // 关闭文件
	int end_ticks = get_ticks();
	close(fd);
	return end_ticks - start_ticks;
}
int test_read_times(char *filename, int tot_len) {
	int start_ticks = get_ticks();
	int fd = open(filename, O_RDWR);
	check_expr(fd >= 0);
	check_expr(lseek(fd, 0, SEEK_SET) >= 0);
	check_expr(read(fd, data_buf, tot_len) == tot_len);
	check_expr(close(fd) >= 0);
	int end_ticks = get_ticks();
	close(fd);
	delete(filename);
	return end_ticks - start_ticks;
}
/*
 * 分别测试在ramfs和在orangefs上，写2e6 bytes的用时和读2e6 bytes的用时
**/
int rw_cmp_test() {
	// 准备数据
	int tot_len = 5e5;
	for(int i = 0;i < tot_len;i++) data_buf[i] = 'a'+(i%26);
	data_buf[tot_len] = '\0';
	int ram_w_5e5 = test_write_times("/ram/rw_cmp", tot_len), ram_r_5e5 = test_read_times("/ram/rw_cmp", tot_len);
	int orange_w_5e5 = test_write_times("/orange/rw_cmp", tot_len), orange_r_5e5 = test_read_times("/orange/rw_cmp", tot_len);
	printf("ramfs write %d bytes: %d ticks\n", tot_len, ram_w_5e5);
	printf("ramfs read %d bytes: %d ticks\n", tot_len, ram_r_5e5);
	printf("orangefs write %d bytes: %d ticks\n", tot_len, orange_w_5e5);
	printf("orangefs read %d bytes: %d ticks\n", tot_len, orange_r_5e5);
	printf("rw_cmp_test pass!!!\n");
	return 0;
}

// 将ram文件夹中的文件拷贝到orange中。
int ramfs2orange_test() {
	// 准备数据
	int tot_len = 5e5;
	for(int i = 0;i < tot_len;i++) data_buf[i] = 'a'+(i%26);
	data_buf[tot_len] = '\0';
	int fd = open("/ram/r2o", O_CREAT | O_RDWR);
	check_expr(fd >= 0); // 创建文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	check_expr(write(fd, data_buf, strlen(data_buf)) == tot_len); // 写入tot_len个字符
	check_expr(close(fd) >= 0); // 关闭文件
	int ramfd = open("/ram/r2o", O_RDWR);
	int orangefd = open("/orange/r2o", O_CREAT | O_RDWR);
	check_expr(ramfd >= 0);
	check_expr(orangefd >= 0);
	check_expr(lseek(ramfd, 0, SEEK_SET) >= 0);
	check_expr(lseek(orangefd, 0, SEEK_SET) >= 0);
	int ret = -1;
	ret = read(ramfd, data_buf, tot_len);
	check_expr(ret == tot_len);
	ret = write(orangefd, data_buf, tot_len);
	check_expr(ret == tot_len);
	check_expr(close(ramfd) >= 0);
	check_expr(close(orangefd) >= 0);
	// 检查文件内容是否一致
	ramfd = open("/ram/r2o", O_RDWR);
	orangefd = open("/orange/r2o", O_RDWR);
	check_expr(ramfd >= 0);
	check_expr(orangefd >= 0);
	check_expr(lseek(ramfd, 191020, SEEK_SET) >= 0);
	check_expr(lseek(orangefd, 191020, SEEK_SET) >= 0);
	int sample_len = 100; // 取样长度
	char sample_buf[105];
	ret = read(ramfd, data_buf, sample_len);
	check_expr(ret == sample_len);
	ret = read(orangefd, sample_buf, sample_len);
	int is_ok = 1;
	for(int i = 0;i < sample_len;i++) {
		if(data_buf[i] != sample_buf[i]) {
			is_ok = 0;
			break;
		}
	}
	check_expr(is_ok);
	check_expr(close(ramfd) >= 0);
	check_expr(close(orangefd) >= 0);
	check_expr(delete("/ram/r2o") >= 0);
	check_expr(delete("/orange/r2o") >= 0);
	printf("ramfs2orange_test pass!!!\n");
	return 0;
}

// 将orange文件夹中的文件拷贝到ram中。
int orange2ramfs_test() {
	// 准备数据
	int tot_len = 5e5;
	for(int i = 0;i < tot_len;i++) data_buf[i] = 'a'+(i%26);
	data_buf[tot_len] = '\0';
	int fd = open("/orange/o2r", O_CREAT | O_RDWR);
	check_expr(fd >= 0); // 创建文件
	check_expr(lseek(fd, 0, SEEK_SET) >= 0); // 将文件指针移动到文件开头
	check_expr(write(fd, data_buf, strlen(data_buf)) == tot_len); // 写入tot_len个字符
	check_expr(close(fd) >= 0); // 关闭文件
	int ramfd = open("/ram/o2r", O_CREAT | O_RDWR);
	int orangefd = open("/orange/o2r", O_RDWR);
	check_expr(ramfd >= 0); check_expr(orangefd >= 0);
	check_expr(lseek(ramfd, 0, SEEK_SET) >= 0);
	check_expr(lseek(orangefd, 0, SEEK_SET) >= 0);
	int ret = -1;
	ret = read(orangefd, data_buf, tot_len);
	check_expr(ret == tot_len);
	ret = write(ramfd, data_buf, tot_len);
	check_expr(ret == tot_len);
	check_expr(close(ramfd) >= 0);
	check_expr(close(orangefd) >= 0);
	// 检查文件内容是否一致
	ramfd = open("/ram/o2r", O_RDWR);
	orangefd = open("/orange/o2r", O_RDWR);
	check_expr(ramfd >= 0);
	check_expr(orangefd >= 0);
	check_expr(lseek(ramfd, 0, SEEK_SET) >= 0);
	check_expr(lseek(orangefd, 0, SEEK_SET) >= 0);
	int sample_len = 100; // 取样长度
	char sample_buf[105];
	ret = read(ramfd, data_buf, sample_len);
	check_expr(ret == sample_len);
	ret = read(orangefd, sample_buf, sample_len);
	int is_ok = 1;
	for(int i = 0;i < sample_len;i++) {
		if(data_buf[i] != sample_buf[i]) {
			is_ok = 0;
			break;
		}
	}
	check_expr(is_ok);
	check_expr(close(ramfd) >= 0);
	check_expr(close(orangefd) >= 0);
	check_expr(delete("/ram/o2r") >= 0);
	check_expr(delete("/orange/o2r") >= 0);
	printf("orange2ramfs_test pass!!!\n");
	return 0;
}


void fat_on_ram_easy_test() {
	printf("fat_on_ram_easy_test start...\n");
	int fd = open("/fat0/test.txt", O_CREAT | O_RDWR);
	lseek(fd, 0, SEEK_SET);
	write(fd, "hello world", 11);
	close(fd);
	fd = open("/fat0/test.txt", O_RDWR);
	char buf[1100];
	read(fd, buf, 11);
	buf[11] = '\0';
	printf("read from fat0/test.txt: %s\n", buf);
	close(fd);
	if(strcmp(buf, "hello world") == 0) {
		printf("fat_on_ram_easy_test pass!!!\n");
	} else {
		printf("fat_on_ram_easy_test failed!!!\n");
		while(1);
	}
}

int all_a_read_check(int fd, int len) {
	// printf("read %d bytes from fd %d\n", len, fd);
	int ret = read(fd, data_buf, len);
	// printf("ret = %d\n", ret);
	check_expr(ret == len);
	// printf("check real data...\n");
	for(int i = 0;i < len;i++) check_expr(data_buf[i] == 'a');
	return 0;
} 

void fat_on_ram_all_a_test() {
	printf("fat_on_ram_all_a_test start...\n");
	for(int i = 0;i < 1e6;i++) data_buf[i] = 'a';
	data_buf[1000000] = '\0';
	check_expr(chdir("/fat0") >= 0);
	int fd = -1;
	fd = open("all_a.txt", O_CREAT | O_RDWR); check_expr(fd >= 0);
	check_expr(fd >= 0);
	check_expr(lseek(fd, 0, SEEK_SET) == 0);
	int ret = -1;
	printf("write 1e6 bytes to all_a.txt\n");
	ret = write(fd, data_buf, strlen(data_buf)); 
	// printf("ret = %d\n", ret);
	check_expr(ret == strlen(data_buf));
	check_expr(close(fd) >= 0);
	fd = open("all_a.txt", O_RDWR); check_expr(fd >= 0);
	// 开头的测试
	check_expr(lseek(fd, 0, SEEK_SET) >= 0);
	all_a_read_check(fd, 1000);
	// 中间的测试
	check_expr(lseek(fd, 88888, SEEK_CUR) >= 0);
	all_a_read_check(fd, 1000);
	// 结尾的测试
	check_expr(lseek(fd, -5000, SEEK_END) >= 0);
	all_a_read_check(fd, 1000);
	check_expr(close(fd) >= 0);
	printf("fat_on_ram_all_a_test pass!!!\n");
	return;
}

int alphabet_read_check(int fd, int pos, int len) {
	printf("read %d bytes from fd %d at pos %d\n", len, fd, pos);
	int ret = read(fd, data_buf, len);
	// printf("ret = %d\n", ret);
	check_expr(ret == len);
	// printf("check real data...\n");
	// printf("check num = %d\n", check_num);
	// for(int i = 0;i < 10;i++) printf("data : %c, %c\n", data_buf[i], 'a' + (pos + i) % 26);
	// printf("\n");
	for(int i = 0;i < len;i++) check_expr(data_buf[i] == 'a' + (pos + i) % 26);
	return 0;
}

void fat_on_ram_alphabet_test() {
	printf("fat_on_ram_alphabet start...\n");
	for(int i = 0;i < 1e5;i++) data_buf[i] = 'a' + i % 26;
	data_buf[100000] = '\0';
	check_expr(chdir("/fat0") >= 0);
	int fd = -1;
	fd = open("alpha.txt", O_CREAT | O_RDWR); check_expr(fd >= 0);
	check_expr(fd >= 0);
	check_expr(lseek(fd, 0, SEEK_SET) == 0);
	int ret = -1;
	printf("write %d bytes to alphabet.txt\n", strlen(data_buf));
	ret = write(fd, data_buf, strlen(data_buf)); 
	printf("ret = %d\n", ret);
	check_expr(ret == strlen(data_buf));
	check_expr(close(fd) >= 0);
	fd = open("alpha.txt", O_RDWR); check_expr(fd >= 0);
	// 开头的测试
	check_expr(lseek(fd, 0, SEEK_SET) >= 0);
	alphabet_read_check(fd, 0, 1000);
	// 中间的测试
	// check_expr(lseek(fd, 60060, SEEK_SET) >= 0);
	// alphabet_read_check(fd, 60060, 1000);
	check_expr(lseek(fd, 65551, SEEK_SET) >= 0);
	alphabet_read_check(fd, 65551, 1000);
	// 结尾的测试
	// check_expr(lseek(fd, 66352, SEEK_SET) >= 0);
	// alphabet_read_check(fd, 66352, 1000);
	check_expr(lseek(fd, 65531, SEEK_SET) >= 0);
	alphabet_read_check(fd, 65531, 1000);
	printf("read finished\n");
	check_expr(close(fd) >= 0);
	printf("fat_on_ram_alphabet pass!!!\n");
	return;
}

void fat_on_ram_rw_cmp_test() {
	printf("fat_on_ram_rw_cmp_test start...\n");
	int tot_len = 5e5;
	for(int i = 0;i < tot_len;i++) data_buf[i] = 'a' + i % 26;
	data_buf[tot_len] = '\0';
	int fat_on_ram_w = test_write_times("/fat0/rw_cmp.txt", tot_len),
	    fat_on_ram_r = test_read_times("/fat0/rw_cmp.txt", tot_len);
	int orange_w = test_write_times("/orange/rw_cmp.txt", tot_len),
	    orange_r = test_read_times("/orange/rw_cmp.txt", tot_len);
	printf("fat on ram disk write 5e5 bytes to rw_cmp.txt %d times\n", fat_on_ram_w);
	printf("fat on ram disk read 5e5 bytes from rw_cmp.txt %d times\n", fat_on_ram_r);
	// printf("fat on hard disk write 5e5 bytes to rw_cmp.txt %d times\n", fat_on_ram_w);
	// printf("fat on hard disk read 5e5 bytes from rw_cmp.txt %d times\n", fat_on_ram_r);
	printf("orange write 5e5 bytes to rw_cmp.txt %d times\n", orange_w);
	printf("orange read 5e5 bytes from rw_cmp.txt %d times\n", orange_r);
	printf("rw_cmp_test pass!!!\n");
	return;
}

int main(int argc, char *argv[])
{
	int stdin = open("dev_tty0", O_RDWR);
	int stdout = open("dev_tty0", O_RDWR);
	int stderr = open("dev_tty0", O_RDWR);
	easytest();
	// all_a_test();
	// alphabet_copy_test();
	// rw_cmp_test();
	// ramfs2orange_test();
	// orange2ramfs_test();
	// fake_shell();
	// fat_on_ram_easy_test();
	// fat_on_ram_all_a_test();
	// fat_on_ram_alphabet_test();
	// fat_on_ram_rw_cmp_test();
	while(1) {
		memset(cmd_buf, 0, MAX_CMD_LEN);
		get_cwd(tmp);
		printf("\nminiOS:%s $ ",tmp);
		if (gets(cmd_buf) && strlen(cmd_buf) != 0)
		{
			if(cmd_parser() == -CMD_NOFOUND) {
				int pid = fork();
				int status;
				if(pid) {
					wait(&status);
				}else{
					exec(cmd_buf);
				}
			}
		}
	}
}



/*
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
*/