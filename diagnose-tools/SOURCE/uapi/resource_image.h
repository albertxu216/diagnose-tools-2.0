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

#ifndef UAPI_RESOURCE_IMAGE_H
#define UAPI_RESOURCE_IMAGE_H

#include <linux/ioctl.h>
int resource_image_syscall(struct pt_regs *regs, long id);

#define DIAG_RESOURCE_IMAGE_SET (DIAG_BASE_SYSCALL_RESOURCE_IMAGE)
#define DIAG_RESOURCE_IMAGE_SETTINGS (DIAG_RESOURCE_IMAGE_SET + 1)
#define DIAG_RESOURCE_IMAGE_DUMP (DIAG_RESOURCE_IMAGE_SETTINGS + 1)

struct diag_resource_image_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int tgid;
	unsigned int pid;
	unsigned int prevt;
	unsigned int cur;
	char comm[TASK_COMM_LEN];
};
/*在这里定义要输出的结构体*/
// struct proc_id{
// 	int pid;
// 	int cpu_id;
// }; 
struct start_rsc{
	long long unsigned int time;
	long long unsigned int readchar;
	long long unsigned int writechar;
};

struct resource_image_detail {
	int et_type;
	struct diag_timespec tv;
	char comm[TASK_COMM_LEN];
    int pid;
    int tgid;
    int cpuid;
	long long unsigned int time;
	long unsigned int memused;
	long long unsigned int readchar;
	long long unsigned int writechar;
};

#define CMD_RESOURCE_IMAGE_SET (0)
#define CMD_RESOURCE_IMAGE_SETTINGS (CMD_RESOURCE_IMAGE_SET + 1)
#define CMD_RESOURCE_IMAGE_DUMP (CMD_RESOURCE_IMAGE_SETTINGS + 1)
#define DIAG_IOCTL_RESOURCE_IMAGE_SET _IOWR(DIAG_IOCTL_TYPE_RESOURCE_IMAGE, CMD_RESOURCE_IMAGE_SET, struct diag_resource_image_settings)
#define DIAG_IOCTL_RESOURCE_IMAGE_SETTINGS _IOWR(DIAG_IOCTL_TYPE_RESOURCE_IMAGE, CMD_RESOURCE_IMAGE_SETTINGS, struct diag_resource_image_settings)
#define DIAG_IOCTL_RESOURCE_IMAGE_DUMP _IOWR(DIAG_IOCTL_TYPE_RESOURCE_IMAGE, CMD_RESOURCE_IMAGE_DUMP, struct diag_ioctl_dump_param)

#endif /*UAPI_RESOURCE_IMAGE_H*/

