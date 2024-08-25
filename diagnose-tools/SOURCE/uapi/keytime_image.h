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

#ifndef UAPI_KEYTIME_IMAGE_H
#define UAPI_KEYTIME_IMAGE_H

#include <linux/ioctl.h>
int keytime_image_syscall(struct pt_regs *regs, long id);

#define DIAG_KEYTIME_IMAGE_SET (DIAG_BASE_SYSCALL_KEYTIME_IMAGE)
#define DIAG_KEYTIME_IMAGE_SETTINGS (DIAG_KEYTIME_IMAGE_SET + 1)
#define DIAG_KEYTIME_IMAGE_DUMP (DIAG_KEYTIME_IMAGE_SETTINGS + 1)


#define BITS_PER_LEN	64

typedef unsigned long long u64;
typedef unsigned int u32;

struct diag_keytime_image_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int tgid;
	unsigned int pid;
	char comm[TASK_COMM_LEN];
};

struct head{
	int pid,tgid,type;//1代表上下cpu,2代表创建进程,3代表执行execve,4代表进程结束;
	u64 time;//当前时间点;
	char comm[TASK_COMM_LEN];
};
union event_union {
    struct {
        int cpu;
        bool flag; // 0是下CPU, 1是上CPU
    } on_off_cpu;

    struct {
        int child_pid, child_tgid;
        char child_comm[TASK_COMM_LEN];
    } process_fork;

    struct {
        char filename[TASK_COMM_LEN];
    } execve;

    struct {
        bool exit;
    } exit;
};
struct events{
	int et_type;
	struct diag_timespec tv;
	struct head head;
	union event_union evnt;
};
struct event_info{
	int type,tgid;
	char comm[TASK_COMM_LEN];
	union event_union *evnt;
};
// struct keytime_image_detail {
// 	int et_type;
// 	struct diag_timespec tv;
// 	struct events events;
// };



#define CMD_KEYTIME_IMAGE_SET (0)
#define CMD_KEYTIME_IMAGE_SETTINGS (CMD_KEYTIME_IMAGE_SET + 1)
#define CMD_KEYTIME_IMAGE_DUMP (CMD_KEYTIME_IMAGE_SETTINGS + 1)
#define DIAG_IOCTL_KEYTIME_IMAGE_SET _IOWR(DIAG_IOCTL_TYPE_KEYTIME_IMAGE, CMD_KEYTIME_IMAGE_SET, struct diag_keytime_image_settings)
#define DIAG_IOCTL_KEYTIME_IMAGE_SETTINGS _IOWR(DIAG_IOCTL_TYPE_KEYTIME_IMAGE, CMD_KEYTIME_IMAGE_SETTINGS, struct diag_keytime_image_settings)
#define DIAG_IOCTL_KEYTIME_IMAGE_DUMP _IOWR(DIAG_IOCTL_TYPE_KEYTIME_IMAGE, CMD_KEYTIME_IMAGE_DUMP, struct diag_ioctl_dump_param)

#endif /*UAPI_KEYTIME_IMAGE_H*/

