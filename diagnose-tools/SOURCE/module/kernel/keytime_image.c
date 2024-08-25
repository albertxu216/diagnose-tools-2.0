/*
 * Linux内核诊断工具--内核态sys-cost功能
 *
 * Copyright (C) 2020 Alibaba Ltd.
 *
 * 作者: Baoyou Xie <baoyou.xie@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */

#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>
#include <trace/events/irq.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <trace/events/napi.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/rbtree.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/percpu_counter.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <linux/context_tracking.h>
#endif
#include <linux/sort.h>
#include <linux/bits.h>

#include <asm/irq_regs.h>
#include <asm/unistd.h>

#if !defined(DIAG_ARM64)
#include <asm/asm-offsets.h>
#endif

//#include <asm/traps.h>

#include "internal.h"
#include "pub/trace_file.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"

#include "uapi/keytime_image.h"

struct diag_keytime_image_settings keytime_image_settings;

static unsigned int keytime_image_alloced;
static struct diag_variant_buffer keytime_image_variant_buffer;



static void clean_data(void)
{

}

/*记录数据并写入缓存区*/
void record(struct events *e){

    do_diag_gettimeofday(&e->tv);
	e->et_type = et_keytime_image_detail;
    unsigned long flags;
	// if(keytime_image_settings.verbose == 1){
		
	// 	diag_task_kern_stack(current, &e->kern_stack);
	// 	diag_task_user_stack(current, &e->user_stack);
	// 	diag_task_raw_stack(current, &e->raw_stack);
	// 	// dump_proc_chains_argv(keytime_image_settings.style, &mm_tree, current, &e->proc_chains);
	// }else e->et_type = et_keytime_image_summary;
	
	// diag_task_brief(current, &e->task);
	diag_variant_buffer_spin_lock(&keytime_image_variant_buffer, flags);
	diag_variant_buffer_reserve(&keytime_image_variant_buffer, sizeof(struct events));
	diag_variant_buffer_write_nolock(&keytime_image_variant_buffer, e, sizeof(struct events));
	diag_variant_buffer_seal(&keytime_image_variant_buffer);
	diag_variant_buffer_spin_unlock(&keytime_image_variant_buffer, flags);
	printk("record yes\n");
}




#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static void trace_sched_switch(struct rq *rq, struct task_struct *prev, struct task_struct *next)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
static void trace_sched_switch(void *__data,
		struct task_struct *prev, struct task_struct *next)
#else
static void trace_sched_switch_hit(void *__data, bool preempt,
		struct task_struct *prev, struct task_struct *next)
#endif
{
	if(keytime_image_settings.pid != prev->pid && keytime_image_settings.pid !=next->pid && keytime_image_settings.pid) //设置了目标pid，但与当前上下cpu的进程不同
		return;
    if(keytime_image_settings.tgid && keytime_image_settings.tgid!=prev->tgid && keytime_image_settings.tgid!=prev->tgid)//设置了目标tgid，但与当前上下cpu的进程不同
		return;
	if(!keytime_image_settings.pid && !keytime_image_settings.tgid)
		return;

	int cpu_id = smp_processor_id();
	struct events on_off_cpu;
	long long now = ktime_to_ns(ktime_get());
	/*prev进程下cpu*/
	if((keytime_image_settings.pid && keytime_image_settings.pid ==prev->pid)
		||(keytime_image_settings.tgid && keytime_image_settings.tgid ==prev->tgid))
	{
		on_off_cpu.head.pid=prev->pid;
		on_off_cpu.head.tgid=prev->tgid;
		on_off_cpu.head.type=1;
		on_off_cpu.head.time = now;
		strncpy(on_off_cpu.head.comm, prev->comm, TASK_COMM_LEN);
		on_off_cpu.head.comm[TASK_COMM_LEN - 1] = 0;
		on_off_cpu.evnt.on_off_cpu.cpu=cpu_id;
		on_off_cpu.evnt.on_off_cpu.flag=0;
		printk("%d offcpu\n",prev->pid);
	}
	/*next进程上cpu*/
	if((keytime_image_settings.pid && keytime_image_settings.pid ==next->pid)
		||(keytime_image_settings.tgid && keytime_image_settings.tgid ==next->tgid))
	{
		on_off_cpu.head.pid=prev->pid;
		on_off_cpu.head.tgid=prev->tgid;
		on_off_cpu.head.type=1;
		on_off_cpu.head.time = now;
		strncpy(on_off_cpu.head.comm, next->comm, TASK_COMM_LEN);
		on_off_cpu.head.comm[TASK_COMM_LEN - 1] = 0;
		on_off_cpu.evnt.on_off_cpu.cpu=cpu_id;
		on_off_cpu.evnt.on_off_cpu.flag=1;
		printk("%d oncpu\n",next->pid);		
	}
	record(&on_off_cpu);
}

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
static void trace_sched_process_fork_hit(void *__data, struct task_struct *parent, struct task_struct *child)
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
static void trace_sched_process_fork_hit(void *__data, struct task_struct *parent, struct task_struct *child)
#else
static void trace_sched_process_fork_hit(struct task_struct *parent, struct task_struct *child)
#endif
{
	if(keytime_image_settings.pid != parent->pid && keytime_image_settings.pid) 
		return;
    if(keytime_image_settings.tgid && keytime_image_settings.tgid!=parent->tgid)
		return;
	if(!keytime_image_settings.pid && !keytime_image_settings.tgid)
		return;
	printk("PID %d here is fork father pid:%d child pid :%d\n",current->pid,parent->pid,child->pid);

	struct events fork;
	long long now = ktime_to_ns(ktime_get());

	fork.head.pid=parent->pid;
	fork.head.tgid=parent->tgid;
	fork.head.type=2;
	fork.head.time = now;
	strncpy(fork.head.comm, parent->comm, TASK_COMM_LEN);
	fork.head.comm[TASK_COMM_LEN - 1] = 0;

	fork.evnt.process_fork.child_pid = child->pid;
	fork.evnt.process_fork.child_tgid = child->tgid;
	strncpy(fork.evnt.process_fork.child_comm, child->comm, TASK_COMM_LEN);
	fork.evnt.process_fork.child_comm[TASK_COMM_LEN - 1] = 0;
	record(&fork);
}


#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exec_hit(void *__data,
	struct task_struct *p,
	pid_t old_pid,
	struct linux_binprm *bprm)
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exec_hit(void *__data,
	struct task_struct *p,
	pid_t old_pid,
	struct linux_binprm *bprm)
#endif
#if KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
{
	if(keytime_image_settings.pid != p->pid && keytime_image_settings.pid) 
		return;
    if(keytime_image_settings.tgid && keytime_image_settings.tgid!=p->tgid)
		return;
	if(!keytime_image_settings.pid && !keytime_image_settings.tgid)
		return;	
	printk("here is process_exec  pid:%d old_pid :%d\n",p->pid,old_pid);

	struct events exe;
	long long now = ktime_to_ns(ktime_get());

	exe.head.pid=p->pid;
	exe.head.tgid=p->tgid;
	exe.head.type=3;
	exe.head.time = now;
	strncpy(exe.head.comm, p->comm, TASK_COMM_LEN);
	exe.head.comm[TASK_COMM_LEN - 1] = 0;

	const char *filename = bprm->filename;
	strncpy(exe.evnt.execve.filename, filename, TASK_COMM_LEN);
	exe.evnt.execve.filename[TASK_COMM_LEN - 1] = 0;
	record(&exe);

}
#endif

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exit_hit(void *__data, struct task_struct *p)
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exit_hit(void *__data, struct task_struct *p)
#else
__maybe_unused static void trace_sched_process_exit_hit(struct task_struct *p)
#endif
{
	if(keytime_image_settings.pid != p->pid && keytime_image_settings.pid) 
		return;
    if(keytime_image_settings.tgid && keytime_image_settings.tgid!=p->tgid)
		return;
	if(!keytime_image_settings.pid && !keytime_image_settings.tgid)
		return;	
	printk("here is process_exit  pid:%d \n",p->pid);

	struct events exit;
	long long now = ktime_to_ns(ktime_get());

	exit.head.pid=p->pid;
	exit.head.tgid=p->tgid;
	exit.head.type=4;
	exit.head.time = now;
	strncpy(exit.head.comm, p->comm, TASK_COMM_LEN);
	exit.head.comm[TASK_COMM_LEN - 1] = 0;

	exit.evnt.exit.exit = 1;
	record(&exit);
}
/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_keytime_image(void)
{
	int ret;

	ret = alloc_diag_variant_buffer(&keytime_image_variant_buffer);
	if (ret)
		goto out_variant_buffer;
	keytime_image_alloced = 1;

	hook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);
	hook_tracepoint("sched_process_fork", trace_sched_process_fork_hit, NULL);
	hook_tracepoint("sched_process_exec", trace_sched_process_exec_hit, NULL);
	hook_tracepoint("sched_process_exit", trace_sched_process_exit_hit, NULL);
	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_keytime_image(void)
{
	unhook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);
	unhook_tracepoint("sched_process_fork", trace_sched_process_fork_hit, NULL);
	unhook_tracepoint("sched_process_exec", trace_sched_process_exec_hit, NULL);
	unhook_tracepoint("sched_process_exit", trace_sched_process_exit_hit, NULL);
	synchronize_sched();
    clean_data();
    keytime_image_settings.activated = 0;
}

int activate_keytime_image(void)
{
	if (!keytime_image_settings.activated)
		keytime_image_settings.activated = __activate_keytime_image();

	return keytime_image_settings.activated;
}

int deactivate_keytime_image(void)
{
	if (keytime_image_settings.activated)
		__deactivate_keytime_image();
	keytime_image_settings.activated = 0;

	return 0;
}


int keytime_image_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_keytime_image_settings settings;
	int cpu;

	switch (id) {
	case DIAG_KEYTIME_IMAGE_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_keytime_image_settings)) {
			ret = -EINVAL;
		} else if (keytime_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				keytime_image_settings = settings;
			}
		}
		break;
	case DIAG_KEYTIME_IMAGE_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_keytime_image_settings)) {
			ret = -EINVAL;
		} else {
			settings = keytime_image_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
		}
		break;
	case DIAG_KEYTIME_IMAGE_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		if (!keytime_image_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&keytime_image_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("sched-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

long diag_ioctl_keytime_image(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_keytime_image_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_KEYTIME_IMAGE_SET:
		if (keytime_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_keytime_image_settings));
			if (!ret) {
				keytime_image_settings = settings;
			}
		}
		break;
	case CMD_KEYTIME_IMAGE_SETTINGS:
		settings = keytime_image_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_keytime_image_settings));
		break;
	case CMD_KEYTIME_IMAGE_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!keytime_image_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			ret = copy_to_user_variant_buffer(&keytime_image_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("sched-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

int diag_keytime_image_init(void)
{
	init_diag_variant_buffer(&keytime_image_variant_buffer, 1 * 1024 * 1024);

	if (keytime_image_settings.activated)
		keytime_image_settings.activated = __activate_keytime_image();

	return 0;
}

void diag_keytime_image_exit(void)
{
	if (keytime_image_settings.activated)
		deactivate_keytime_image();
	keytime_image_settings.activated = 0;

	msleep(20);
	destroy_diag_variant_buffer(&keytime_image_variant_buffer);
}
