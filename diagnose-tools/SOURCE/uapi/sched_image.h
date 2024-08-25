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

#ifndef UAPI_SCHED_IMAGE_H
#define UAPI_SCHED_IMAGE_H

#include <linux/ioctl.h>
int sched_image_syscall(struct pt_regs *regs, long id);

#define DIAG_SCHED_IMAGE_SET (DIAG_BASE_SYSCALL_SCHED_IMAGE)
#define DIAG_SCHED_IMAGE_SETTINGS (DIAG_SCHED_IMAGE_SET + 1)
#define DIAG_SCHED_IMAGE_DUMP (DIAG_SCHED_IMAGE_SETTINGS + 1)
#define TIF_NEED_RESCHED 3
#define _TIF_NEED_RESCHED	(1<<TIF_NEED_RESCHED)

#define BITS_PER_LEN	64

typedef unsigned long long u64;
typedef unsigned int u32;

struct diag_sched_image_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int tgid;
	unsigned int pid;
	unsigned int threshold_ms;
	char comm[TASK_COMM_LEN];
};

//每个进程的迁移信息;
struct sched_image_detail {
	int et_type;
	struct diag_timespec tv;
    int cpu_id;
    int pid;
    u64 pre_sched_time;
	unsigned long delay_ns;
	struct diag_task_detail task;
	struct diag_kern_stack_detail kern_stack;
	struct diag_user_stack_detail user_stack;
	// struct diag_proc_chains_detail proc_chains;
	struct diag_raw_stack_detail raw_stack;
};



#define CMD_SCHED_IMAGE_SET (0)
#define CMD_SCHED_IMAGE_SETTINGS (CMD_SCHED_IMAGE_SET + 1)
#define CMD_SCHED_IMAGE_DUMP (CMD_SCHED_IMAGE_SETTINGS + 1)
#define DIAG_IOCTL_SCHED_IMAGE_SET _IOWR(DIAG_IOCTL_TYPE_SCHED_IMAGE, CMD_SCHED_IMAGE_SET, struct diag_sched_image_settings)
#define DIAG_IOCTL_SCHED_IMAGE_SETTINGS _IOWR(DIAG_IOCTL_TYPE_SCHED_IMAGE, CMD_SCHED_IMAGE_SETTINGS, struct diag_sched_image_settings)
#define DIAG_IOCTL_SCHED_IMAGE_DUMP _IOWR(DIAG_IOCTL_TYPE_SCHED_IMAGE, CMD_SCHED_IMAGE_DUMP, struct diag_ioctl_dump_param)

#endif /*UAPI_SCHED_IMAGE_H*/

