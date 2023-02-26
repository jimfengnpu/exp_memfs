#include "type.h"
#include "const.h"
#include "string.h"
#include "fs_const.h"
#include "protect.h"
#include "proc.h"
#include "hd.h"
#include "global.h"
#include "proto.h"
#include "memman.h"
#include "spinlock.h"

#define RAMDISK_SIZE	2*num_4M
#define RAMDISK_SEC		RAMDISK_SIZE/SECTOR_SIZE
#define RAMDISK_PGNUM	RAMDISK_SIZE/num_4K
#define RAMDISK_FS		RAM_FS_TYPE
// #define RAMDISK_FS 		FAT32_TYPE
static char* p_ramdisk_root[RAMDISK_SIZE/num_4K];
static struct spinlock ramdisk_lock;

static int get_addr(int offset, char **addr) {
	int idx = offset/num_4K;
	*addr = 0;
	if(idx >= RAMDISK_PGNUM)return 0;
	*addr = p_ramdisk_root[idx] + offset % num_4K;
	return num_4K*(idx + 1) - offset;
}

void ramdisk_init() {
	initlock(&ramdisk_lock, "ramdisk");
	for(int i = 0;i < RAMDISK_PGNUM; i++) {
		p_ramdisk_root[i] = (char*) K_PHY2LIN(do_malloc_4k());
		memset(p_ramdisk_root[i], 0, num_4K);
	}
	hd_info[RAMDISK_DRV].open_cnt ++;
	hd_info[RAMDISK_DRV].primary[0].base = 0;
	hd_info[RAMDISK_DRV].primary[0].size = RAMDISK_SEC;
	hd_info[RAMDISK_DRV].primary[0].fs_type = RAMDISK_FS;
	// print_hdinfo(&hd_info[RAMDISK_DRV]);
}

int ram_rdwt(int io_type, int dev, u64 pos, int bytes, int proc_nr, void* buf) {
	acquire(&ramdisk_lock); // 确保原子
	char * addr;
	int bytes_step;
	while(bytes > 0) {
		bytes_step = min(bytes, num_4K);
		bytes_step = min(bytes_step, get_addr(pos, &addr));
		if(addr){
			if(io_type == DEV_READ) { // read
				memcpy((void*)va2la(proc_nr, buf), (void*)addr, bytes_step);
			}else if(io_type == DEV_WRITE) { // write
				memcpy((void*)addr, (void*)va2la(proc_nr, buf), bytes_step);
			}
		}else{
			disp_color_str("read ramdisk overflow!", 0x74);
		}
		pos += bytes_step;
		bytes -= bytes_step;
	}
	release(&ramdisk_lock);
	return 0;
}