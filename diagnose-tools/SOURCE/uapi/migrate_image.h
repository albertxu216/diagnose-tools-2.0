/*
 * Linux内核诊断工具--用户接口API
 * 
 * Copyright (C) 2022 Alibaba Ltd.
 *
 * 作者: Yang Wei <albin.yangwei@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */

#ifndef UAPI_MIGRATE_IMAGE_H
#define UAPI_MIGRATE_IMAGE_H

#include <linux/ioctl.h>
int migrate_image_syscall(struct pt_regs *regs, long id);

#define DIAG_MIGRATE_IMAGE_SET (DIAG_BASE_SYSCALL_MIGRATE_IMAGE)
#define DIAG_MIGRATE_IMAGE_SETTINGS (DIAG_MIGRATE_IMAGE_SET + 1)
#define DIAG_MIGRATE_IMAGE_DUMP (DIAG_MIGRATE_IMAGE_SETTINGS + 1)
#define MAX_MIGRATE 64
typedef unsigned long long u64;
typedef unsigned int u32;

struct diag_migrate_image_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int tgid;
	unsigned int pid;
	char comm[TASK_COMM_LEN];
};

struct per_migrate{//每次迁移,记录该次迁移信息;
	u64 time;
	u32 orig_cpu;
	u32 dest_cpu;
	u64 orig_cpu_load;
	u64 dest_cpu_load;
	u64 pload_avg;
	u64 putil_avg;
	int on_cpu;
    u64 mem_usage;
    u64 read_bytes;
	u64 write_bytes;
	// u64 syscr;
	// u64 syscw;
    u64 context_switches;
    u64 runtime;
};
//每个进程的迁移信息;
struct migrate_image_detail{
	int et_type;
	struct diag_timespec tv;
	pid_t pid;
	int tgid;
	int prio;
	int count,rear;//迁移频率
	struct per_migrate migrate_info[MAX_MIGRATE];//该进程每次迁移信息;
};



#define CMD_MIGRATE_IMAGE_SET (0)
#define CMD_MIGRATE_IMAGE_SETTINGS (CMD_MIGRATE_IMAGE_SET + 1)
#define CMD_MIGRATE_IMAGE_DUMP (CMD_MIGRATE_IMAGE_SETTINGS + 1)
#define DIAG_IOCTL_MIGRATE_IMAGE_SET _IOWR(DIAG_IOCTL_TYPE_MIGRATE_IMAGE, CMD_MIGRATE_IMAGE_SET, struct diag_migrate_image_settings)
#define DIAG_IOCTL_MIGRATE_IMAGE_SETTINGS _IOWR(DIAG_IOCTL_TYPE_MIGRATE_IMAGE, CMD_MIGRATE_IMAGE_SETTINGS, struct diag_migrate_image_settings)
#define DIAG_IOCTL_MIGRATE_IMAGE_DUMP _IOWR(DIAG_IOCTL_TYPE_MIGRATE_IMAGE, CMD_MIGRATE_IMAGE_DUMP, struct diag_ioctl_dump_param)

#endif /*UAPI_MIGRATE_IMAGE_H*/

