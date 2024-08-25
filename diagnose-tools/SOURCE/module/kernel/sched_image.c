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

#include "uapi/sched_image.h"

struct diag_sched_image_settings sched_image_settings;

static unsigned int sched_image_alloced;
static struct diag_variant_buffer sched_image_variant_buffer;

static void do_clean(void *info)
{
	struct diag_percpu_context *context;

	context = get_percpu_context();
	memset(&context->sched_image, 0, sizeof(context->sched_image));
    memset(&context->sched_image_detail, 0, sizeof(context->sched_image_detail));
}

static void clean_data(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (cpu == smp_processor_id()) {
			do_clean(NULL);
		} else {
			smp_call_function_single(cpu, do_clean, NULL, 1);
		}
	}
}

static inline int is_need_sched(struct task_struct *tsk , int flag){
    struct thread_info *thread_info;
#ifdef CONFIG_THREAD_INFO_IN_TASK
    thread_info = &tsk->thread_info;
#else
    thread_info = task_thread_info(tsk);
#endif
	int tmp = test_ti_thread_flag(thread_info, TIF_NEED_RESCHED);
	printk("PID:%d \t %d\n",current->pid,tmp?1111:0);
	return tmp;
	// return 1;

}
/*记录数据并写入缓存区*/
void record_info(struct diag_percpu_context *context,u64 delay){
	struct sched_image_detail *detail = &context->sched_image_detail;
    do_diag_gettimeofday(&detail->tv);
    detail->delay_ns = delay;
    detail->cpu_id = smp_processor_id();
    detail->pid = current->pid;
    detail->pre_sched_time = context->sched_image.pre_sched_time;
    unsigned long flags;
	if(sched_image_settings.verbose == 1){
		detail->et_type = et_sched_image_detail;
		diag_task_kern_stack(current, &detail->kern_stack);
		diag_task_user_stack(current, &detail->user_stack);
		diag_task_raw_stack(current, &detail->raw_stack);
		// dump_proc_chains_argv(sched_image_settings.style, &mm_tree, current, &detail->proc_chains);
	}else detail->et_type = et_sched_image_summary;
	
	diag_task_brief(current, &detail->task);
	diag_variant_buffer_spin_lock(&sched_image_variant_buffer, flags);
	diag_variant_buffer_reserve(&sched_image_variant_buffer, sizeof(struct sched_image_detail));
	diag_variant_buffer_write_nolock(&sched_image_variant_buffer, detail, sizeof(struct sched_image_detail));
	diag_variant_buffer_seal(&sched_image_variant_buffer);
	diag_variant_buffer_spin_unlock(&sched_image_variant_buffer, flags);
}
void sched_image_timer(struct diag_percpu_context *context)
{
    struct task_struct *tsk = current;
    int pid = tsk->pid;
    if(!sched_image_settings.activated || !tsk->pid) return;
    if(!is_need_sched(tsk,TIF_NEED_RESCHED))
        return;
    u64 now = sched_clock();
    if(context->sched_image.pre_sched_time){
        context->sched_image.no_sched++;
        if(sched_image_settings.threshold_ms && 
        ((now - context->sched_image.pre_hrtime)/1000 >sched_image_settings.threshold_ms)){
            record_info(context,now - context->sched_image.pre_sched_time);//记录信息;
            context->sched_image.pre_hrtime = now;
            context->sched_image.pre_hrtime = now;
        }
        context->sched_image.pre_sched_time = 0;
    }
    else{
        context->sched_image.pre_hrtime = now;
        context->sched_image.pre_sched_time = now;
        context->sched_image.no_sched = 0;
    }
}



#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static void trace_sched_switch(struct rq *rq, struct task_struct *prev, struct task_struct *next)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0)
static void trace_sched_switch(void *__data,
		struct task_struct *prev, struct task_struct *next)
#else
static void trace_sched_switch(void *__data, bool preempt,
		struct task_struct *prev, struct task_struct *next)
#endif
{
    struct diag_percpu_context *context;
    int cpu_id = smp_processor_id();
    context = get_percpu_context();
    if(context->sched_image.pre_sched_time){
        u64 now = sched_clock();
        if(sched_image_settings.threshold_ms && 
        ((now - context->sched_image.pre_sched_time)/1000 >sched_image_settings.threshold_ms)){
            record_info(context,now - context->sched_image.pre_sched_time);//记录信息;
            context->sched_image.pre_hrtime = now;
        }
        context->sched_image.pre_sched_time = 0;
    }
}

/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_sched_image(void)
{
	int ret;

	ret = alloc_diag_variant_buffer(&sched_image_variant_buffer);
	if (ret)
		goto out_variant_buffer;
	sched_image_alloced = 1;

	clean_data();

	hook_tracepoint("sched_switch", trace_sched_switch, NULL);
	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_sched_image(void)
{
	unhook_tracepoint("sched_switch", trace_sched_switch, NULL);
	synchronize_sched();
    clean_data();
    sched_image_settings.activated = 0;
}

int activate_sched_image(void)
{
	if (!sched_image_settings.activated)
		sched_image_settings.activated = __activate_sched_image();

	return sched_image_settings.activated;
}

int deactivate_sched_image(void)
{
	if (sched_image_settings.activated)
		__deactivate_sched_image();
	sched_image_settings.activated = 0;

	return 0;
}


int sched_image_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_sched_image_settings settings;
	int cpu;

	switch (id) {
	case DIAG_SCHED_IMAGE_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_sched_image_settings)) {
			ret = -EINVAL;
		} else if (sched_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				sched_image_settings = settings;
			}
		}
		break;
	case DIAG_SCHED_IMAGE_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_sched_image_settings)) {
			ret = -EINVAL;
		} else {
			settings = sched_image_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
		}
		break;
	case DIAG_SCHED_IMAGE_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		if (!sched_image_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&sched_image_variant_buffer,
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

long diag_ioctl_sched_image(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_sched_image_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_SCHED_IMAGE_SET:
		if (sched_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_sched_image_settings));
			if (!ret) {
				sched_image_settings = settings;
			}
		}
		break;
	case CMD_SCHED_IMAGE_SETTINGS:
		settings = sched_image_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_sched_image_settings));
		break;
	case CMD_SCHED_IMAGE_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!sched_image_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			ret = copy_to_user_variant_buffer(&sched_image_variant_buffer,
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

int diag_sched_image_init(void)
{
	init_diag_variant_buffer(&sched_image_variant_buffer, 1 * 1024 * 1024);

	if (sched_image_settings.activated)
		sched_image_settings.activated = __activate_sched_image();

	return 0;
}

void diag_sched_image_exit(void)
{
	if (sched_image_settings.activated)
		deactivate_sched_image();
	sched_image_settings.activated = 0;

	msleep(20);
	destroy_diag_variant_buffer(&sched_image_variant_buffer);
}
