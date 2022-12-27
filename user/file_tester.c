#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "stdio.h"
char buf[256];
int main(int argc, char *argv[])
{
	printf("cmd_char [path]\n");
	printf("w write\nr read\nd delete\n");
	while (1)
	{
		gets(buf);
		switch (buf[0])
		{
		case 'r':
			int fd = open(buf + 2, O_RDWR);
			if(fd==-1)
				printf("err,maybe not exist");
			else{
				read(fd, buf, 25);
				printf("%s\n", buf);
			}
			break;
		case 'w':
			int fd = open(buf + 2, O_RDWR|O_CREAT);
			gets(buf);
			write(fd, buf, strlen(buf));
			break;
		case 'd':
			delete (buf + 2);
			break;
		default:
			break;
		}
	}
}