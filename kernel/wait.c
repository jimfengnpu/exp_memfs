#include "type.h"
#include "const.h"
#include "protect.h"
#include "proc.h"
#include "global.h"
#include "proto.h"
#include "memman.h"
#include "errno.h"
#include "x86.h"
#include "assert.h"
#include "string.h"
// modified from ch6  by jianfeng 23/2/12
static void free_mem_child(PROCESS_0 *p_child) {
	u32 addr_lin;
	//复制代码，代码是共享的，直接将物理地址挂载在子进程的页表上
	//简单说明一下 如果子进程(待回收)的父进程不是亲父进程,那么一定是亲父进程先退出,初始进程回收退出后的子进程,此时可以释放代码页
	for(addr_lin = p_child->memmap.text_lin_base ; addr_lin < p_child->memmap.text_lin_limit ; addr_lin+=num_4K )
	{
		if(p_child->info.ppid != p_child->info.real_ppid){
			// lin_mapping_phy(addr_lin,//线性地址
			// 				get_page_phy_addr(ppid,addr_lin),//物理地址，为MAX_UNSIGNED_INT时，由该函数自动分配物理内存
			// 				pid,//要挂载的进程的pid，子进程的pid
			// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
			// 				PG_P  | PG_USU | PG_RWR);//页表属性，代码是只读的
			assert(phy_exist(p_child->cr3, addr_lin));
			do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));
		}
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
	}
	//复制数据，数据不共享，子进程需要申请物理地址，并复制过来
	for(addr_lin = p_child->memmap.data_lin_base ; addr_lin < p_child->memmap.data_lin_limit ; addr_lin+=num_4K )
	{	
		// lin_mapping_phy(SharePageBase,0,ppid,PG_P  | PG_USU | PG_RWW,0);//使用前必须清除这个物理页映射
		// lin_mapping_phy(SharePageBase,MAX_UNSIGNED_INT,ppid,PG_P  | PG_USU | PG_RWW,PG_P  | PG_USU | PG_RWW);//利用父进程的共享页申请物理页
		// memcpy((void*)SharePageBase,(void*)(addr_lin&0xFFFFF000),num_4K);//将数据复制到物理页上,注意这个地方是强制一页一页复制的
		// lin_mapping_phy(addr_lin,//线性地址
		// 				get_page_phy_addr(ppid,SharePageBase),//物理地址，获取共享页的物理地址，填进子进程页表
		// 				pid,//要挂载的进程的pid，子进程的pid
		// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
		// 				PG_P  | PG_USU | PG_RWW);//页表属性，数据是可读写的	
		if(phy_exist(p_child->cr3, addr_lin)){
		do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));	
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
		}
	}
	//复制保留内存，保留内存不共享，子进程需要申请物理地址，并复制过来
	for(addr_lin = p_child->memmap.vpage_lin_base ; addr_lin < p_child->memmap.vpage_lin_limit ; addr_lin+=num_4K )
	{
		// lin_mapping_phy(SharePageBase,0,ppid,PG_P  | PG_USU | PG_RWW,0);//使用前必须清除这个物理页映射
		// lin_mapping_phy(SharePageBase,MAX_UNSIGNED_INT,ppid,PG_P  | PG_USU | PG_RWW,PG_P  | PG_USU | PG_RWW);//利用父进程的共享页申请物理页
		// memcpy((void*)SharePageBase,(void*)(addr_lin&0xFFFFF000),num_4K);//将数据复制到物理页上,注意这个地方是强制一页一页复制的
		// lin_mapping_phy(addr_lin,//线性地址
		// 				get_page_phy_addr(ppid,SharePageBase),//物理地址，获取共享页的物理地址，填进子进程页表
		// 				pid,//要挂载的进程的pid，子进程的pid
		// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
		// 				PG_P  | PG_USU | PG_RWW);//页表属性，保留内存是可读写的	
		assert(phy_exist(p_child->cr3, addr_lin));	
		do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
	}
	
	//复制堆，堆不共享，子进程需要申请物理地址，并复制过来
	for(addr_lin = p_child->memmap.heap_lin_base ; addr_lin < p_child->memmap.heap_lin_limit ; addr_lin+=num_4K )
	{
		// lin_mapping_phy(SharePageBase,0,ppid,PG_P  | PG_USU | PG_RWW,0);//使用前必须清除这个物理页映射
		// lin_mapping_phy(SharePageBase,MAX_UNSIGNED_INT,ppid,PG_P  | PG_USU | PG_RWW,PG_P  | PG_USU | PG_RWW);//利用父进程的共享页申请物理页
		// memcpy((void*)SharePageBase,(void*)(addr_lin&0xFFFFF000),num_4K);//将数据复制到物理页上,注意这个地方是强制一页一页复制的
		// lin_mapping_phy(addr_lin,//线性地址
		// 				get_page_phy_addr(ppid,SharePageBase),//物理地址，获取共享页的物理地址，填进子进程页表
		// 				pid,//要挂载的进程的pid，子进程的pid
		// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
		// 				PG_P  | PG_USU | PG_RWW);//页表属性，堆是可读写的
		assert(phy_exist(p_child->cr3, addr_lin));		
		do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
	}	
	
	//复制栈，栈不共享，子进程需要申请物理地址，并复制过来(注意栈的复制方向)
	for(addr_lin = p_child->memmap.stack_lin_base ; addr_lin > p_child->memmap.stack_lin_limit ; addr_lin-=num_4K )
	{
		// lin_mapping_phy(SharePageBase,0,ppid,PG_P  | PG_USU | PG_RWW,0);//使用前必须清除这个物理页映射
		// lin_mapping_phy(SharePageBase,MAX_UNSIGNED_INT,ppid,PG_P  | PG_USU | PG_RWW,PG_P  | PG_USU | PG_RWW);//利用父进程的共享页申请物理页
		// memcpy((void*)SharePageBase,(void*)(addr_lin&0xFFFFF000),num_4K);//将数据复制到物理页上,注意这个地方是强制一页一页复制的
		// lin_mapping_phy(addr_lin,//线性地址
		// 				get_page_phy_addr(ppid,SharePageBase),//物理地址，获取共享页的物理地址，填进子进程页表
		// 				pid,//要挂载的进程的pid，子进程的pid
		// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
		// 				PG_P  | PG_USU | PG_RWW);//页表属性，栈是可读写的
		if(phy_exist(p_child->cr3, addr_lin)){
		do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));		
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
		}
	}
	
	//复制参数区，参数区不共享，子进程需要申请物理地址，并复制过来
	for(addr_lin = p_child->memmap.arg_lin_base ; addr_lin < p_child->memmap.arg_lin_limit ; addr_lin+=num_4K )
	{
		// lin_mapping_phy(SharePageBase,0,ppid,PG_P  | PG_USU | PG_RWW,0);//使用前必须清除这个物理页映射
		// lin_mapping_phy(SharePageBase,MAX_UNSIGNED_INT,ppid,PG_P  | PG_USU | PG_RWW,PG_P  | PG_USU | PG_RWW);//利用父进程的共享页申请物理页
		// memcpy((void*)SharePageBase,(void*)(addr_lin&0xFFFFF000),num_4K);//将数据复制到物理页上,注意这个地方是强制一页一页复制的
		// lin_mapping_phy(addr_lin,//线性地址
		// 				get_page_phy_addr(ppid,SharePageBase),//物理地址，获取共享页的物理地址，填进子进程页表
		// 				pid,//要挂载的进程的pid，子进程的pid
		// 				PG_P  | PG_USU | PG_RWW,//页目录属性，一般都为可读写
		// 				PG_P  | PG_USU | PG_RWW);//页表属性，参数区是可读写的
		assert(phy_exist(p_child->cr3, addr_lin));
		do_free_4k(get_page_phy_addr(p_child->pid, addr_lin));			
		lin_mapping_phy(addr_lin, 0, p_child->pid, PG_P  | PG_USU | PG_RWW,0);
	}
	u32 *cr3_table = (u32*)K_PHY2LIN(p_child->cr3);
	for(int i = 0; i < num_1K; i++) {
		if(cr3_table[i]&1) { 
			do_free_4k(cr3_table[i] & 0xFFFFF000);
		}
	}
	memset(cr3_table,0, num_4K);
	do_free_4k(p_child->cr3);
}

ssize_t
kern_wait(int *wstatus)
{
	// 相比于fork来说，wait的实现简单很多
	// 语义实现比较清晰，没有fork那么多难点要处理，所以这里并不会给大家太多引导
	// 需要大家自己思考wait怎么实现。
	ssize_t ret = 0;
	PROCESS_0 *p_proc = &p_proc_current->task, *p_son;
	struct son_node *son;
	while(1){
		disable_int();
		// if(xchg(&p_proc->lock, 1)==1){
		// 	// kprintf("w%d:r%d ",p_proc->pid,p_proc->pid);
		// 	goto loop;
		// }
		// printlock(p_proc->pid,p_proc->pid,'w');
		if(p_proc->info.child_p_num == 0){
			ret = -ECHILD;
			goto end;
		}
		// for(son = p_proc->fork_tree.sons;son;son = son->nxt)
		int find_flag = 0;
		for(int i = 0; i < p_proc->info.child_p_num; i++)
		{
			// while(xchg(&son->p_son->lock, 1) == 1){
				// kprintf("w%d:r%d ",p_proc->pid,son->p_son->pid);
				// schedule();
			// }
			// printlock(p_proc->pid,son->p_son->pid,'w');
			// if(son->p_son->statu == ZOMBIE){
			if(proc_table[p_proc->info.child_process[i]].task.stat == KILLED){	
				p_son = &proc_table[p_proc->info.child_process[i]].task;
				p_proc->info.child_p_num--;
				find_flag = 1;
				// goto find;
			}
			if(find_flag) {
				p_proc->info.child_process[i] = p_proc->info.child_process[i + 1];
			}
			// xchg(&son->p_son->lock, 0);
			// printlock(p_proc->pid,son->p_son->pid,'w');
		}
		if(find_flag == 1) {
			goto find;
		}
		// DISABLE_INT();
		// p_proc->statu = SLEEP;
		p_proc->stat = SLEEPING;
		// kprintf("startsleep");
		// xchg(&p_proc->lock, 0);
		// printlock(p_proc->pid,p_proc->pid,'w');
		// ENABLE_INT();
		enable_int();
loop:
		// schedule();
		sched();
	}
find:
	// 在实现之前你必须得读一遍文档`man 2 wait`
	// 了解到wait大概要做什么
	// panic("Unimplement! Read The F**king Manual");
	// xchg(&p_proc->lock, 0);
	// assert(p_proc->lock != 0);
	// assert(p_son->lock != 0);
	// if(son->pre != NULL)
	// 	son->pre->nxt = son->nxt;
	// else p_proc->fork_tree.sons = son->nxt;
	// if(son->nxt != NULL)
	// 	son->nxt->pre = son->pre;
	// kfree(son);	

	if(wstatus != NULL)
		(*wstatus) = p_son->exit_code;
	ret = p_son->pid;
	// 当然读文档是一方面，最重要的还是代码实现
	// wait系统调用与exit系统调用关系密切，所以在实现wait之前需要先读一遍exit为好
	// 可能读完exit的代码你可能知道wait该具体做什么了
	// panic("Unimplement! Read The F**king Source Code");
	// recycle_pages(p_son->page_list);
	free_mem_child(p_son);
	p_son->stat = IDLE;
	// 接下来就是你自己的实现了，我们在设计的时候这段代码不会有太大问题
	// 在实现完后你任然要对自己来个灵魂拷问
	// 1. 上锁上了吗？所有临界情况都考虑到了吗？（永远要相信有各种奇奇怪怪的并发问题）
	// 2. 所有错误情况都判断到了吗？错误情况怎么处理？（RTFM->`man 2 wait`）
	// 3. 是否所有的资源都正确回收了？
	// 4. 你写的代码真的符合wait语义吗？
	// panic("Unimplement! soul torture");
	// xchg(&p_son->lock, 0);
	// printlock(p_proc->pid,p_son->pid,'w');
end:
	enable_int();
	// xchg(&p_proc->lock, 0);
	// printlock(p_proc->pid,p_proc->pid,'w');
	return ret;
}

ssize_t
do_wait(int *wstatus)
{
	// assert((uintptr_t)wstatus < KERNBASE);
	// assert((uintptr_t)wstatus + sizeof(wstatus) < KERNBASE);
	return kern_wait(wstatus);
}

int sys_wait(char* arg) {
	return do_wait((int*)get_arg(arg, 1));
}