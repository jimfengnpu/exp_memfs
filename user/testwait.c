#include "type.h"
#include "errno.h"
#include "stdio.h"
#include "protect.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "assert.h"
#ifndef MINIOS_USER_WAIT_H
#define MINIOS_USER_WAIT_H

#define WEXITSTATUS(s) (((s) & 0xff00) >> 8)
#define WTERMSIG(s) ((s) & 0x7f)
#define WSTOPSIG(s) WEXITSTATUS(s)
#define WCOREDUMP(s) ((s) & 0x80)
#define WIFEXITED(s) (!WTERMSIG(s))
#define WIFSTOPPED(s) ((short)((((s)&0xffff)*0x10001)>>8) > 0x7f00)
#define WIFSIGNALED(s) (((s)&0xffff)-1U < 0xffu)
#define WIFCONTINUED(s) ((s) == 0xffff)

void
_panic(const char *file, int line, const char *fmt,...){
	va_list ap;

	// 确保CPU核不受外界中断的影响
	asm volatile("cli");
	asm volatile("cld");

	va_start(ap, fmt);
	printf("kernel panic at %s:%d: ", file, line);
	vprintf(fmt, ap);
	printf("\n");
	va_end(ap);
	// 休眠CPU核，直接罢工
	while(1)
		asm volatile("hlt");
}
#endif /* MINIOS_USER_WAIT_H */
void test_fork_wait1(void)
{
	ssize_t pid = fork();

	assert(pid >= 0);

	if (pid == 0)
		exit(114);
	
	int wstatus;
	assert(pid == wait(&wstatus));

	assert(WEXITSTATUS(wstatus) == 114);

	printf("test_fork_wait1 passed!\n");
}

void test_fork_wait2(void)
{
	ssize_t pid = fork();

	assert(pid >= 0);

	if (pid == 0)
		exit(514);
	
	assert(pid == wait(NULL));

	printf("test_fork_wait2 passed!\n");
}

void test_empty_wait(void)
{
	assert(wait(NULL) == -ECHILD);

	printf("test_empty_wait passed!\n");
}

void test_fork_limit(void)
{
	int xor_sum = 0;

	for (int i = 1 ; i <= 3 ; i++) {
		ssize_t pid = fork();

		assert(pid >= 0);

		if (pid == 0)
			exit(0);
		
		xor_sum ^= pid;
	}

	// assert(fork() == -EAGAIN);
	
	int wait_cnt = 0, wait_pid;
	while ((wait_pid = wait(NULL)) >= 0)
		wait_cnt++, xor_sum ^= wait_pid;
	
	assert(wait_cnt == 3);
	assert(xor_sum == 0);
	
	printf("test_fork_limit passed!\n");
}

void test_wait_is_sleeping(void)
{
	ssize_t rt_pid = get_pid();

	for (int i = 1 ; i <= 2 ; i++) {
		ssize_t pid = fork();

		assert(pid >= 0);
		
		if (pid > 0) {
			int wstatus;
			assert(pid == wait(&wstatus));
			assert(WEXITSTATUS(wstatus) == 42);
			if (get_pid() != rt_pid)
				exit(42);
			break;
		}
	}

	if (get_pid() != rt_pid) {
		ssize_t la_ticks = get_ticks();
		
		for (int i = 1 ; i <= (int)5e6 ; i++) {
			ssize_t now_ticks = get_ticks();
			assert(now_ticks - la_ticks <= 2);
			la_ticks = now_ticks;
		}

		exit(42);
	}

	printf("test_wait_is_sleeping passed!\n");
}

int main()
{
	test_fork_wait1();
	test_fork_wait2();
	test_empty_wait();
	// test_fork_limit();
	test_wait_is_sleeping();
	printf("all tests passed!\n");
	exit(0);
}