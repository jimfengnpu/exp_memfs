#include "type.h"
#include "const.h"
#include "protect.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "x86.h"

void kern_exit(PROCESS_0 *p_proc, int exit_code)
{
	// 托孤，将所有子进程转移到初始进程下
	// transfer_orphans(p_proc);
	disable_int();
	for(int i = 0; i < p_proc->info.child_p_num; i++) {
		proc_table[NR_K_PCBS].task.info.child_process[proc_table[NR_K_PCBS].task.info.child_p_num++] 
			= p_proc->info.child_process[i];
		proc_table[p_proc->info.child_process[i]].task.info.ppid = NR_K_PCBS;
	}
	p_proc->info.child_p_num = 0;
	enable_int();
	// 上锁修改exit code
	// while (xchg(&p_proc->lock, 1) == 1)
		// schedule();
	// printlock(p_proc->pid,p_proc->pid,'e');
	p_proc->exit_code = exit_code;
	// xchg(&p_proc->lock, 0);
	// printlock(p_proc->pid,p_proc->pid,'e');
	// 下面两个操作会修改进程的状态，
	// 这是非常危险的，最好用开关中断保护上
	// DISABLE_INT();
	// 这个函数干了两件事，唤醒父进程，将自己状态置为僵尸进程
	// 关中断就相当于两件事同时干了
	// awake_father_and_become_zombie(p_proc);
	disable_int();
	p_proc->stat = KILLED;
	proc_table[p_proc->info.ppid].task.stat = READY;
	// kprintf("e%d:%x ",p_proc->pid,exit_code);
	// 在触发了调度之后这个进程在被回收之前永远无法被调度到
	enable_int();
	sched();

	// ENABLE_INT();
	// panic("exit failed!");
}

void do_exit(int status)
{
	// 为什么这个参数这么奇怪？你可能需要读读手册
	kern_exit(&p_proc_current->task, (status & 0xFF) << 8);
}

void sys_exit(char* arg) {
	do_exit(get_arg(arg, 1));
	return;
}