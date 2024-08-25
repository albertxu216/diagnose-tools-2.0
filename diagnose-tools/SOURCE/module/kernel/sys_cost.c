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

#include "uapi/sys_cost.h"

struct diag_sys_cost_settings sys_cost_settings;

static unsigned int sys_cost_alloced;
static struct diag_variant_buffer sys_cost_variant_buffer;

static void do_clean(void *info)
{
	struct diag_percpu_context *context;

	context = get_percpu_context();
	memset(&context->sys_cost, 0, sizeof(context->sys_cost));
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

static int need_trace(struct task_struct *tsk)
{
	int cpu;

	if (!sys_cost_settings.activated)
		return 0;

	cpu = smp_processor_id();
	//orig_idle_task指向该cpu的idle任务
	if (orig_idle_task && orig_idle_task(cpu) == tsk)
		return 0;

	//目标进程或目标线程组中
	if (sys_cost_settings.tgid) {
		if (tsk->tgid != sys_cost_settings.tgid)
			return 0;
	}

	if (sys_cost_settings.pid) {
		if (tsk->pid != sys_cost_settings.pid)
			return 0;
	}

	if (sys_cost_settings.comm[0]) {
		struct task_struct *leader = tsk->group_leader ? tsk->group_leader : tsk;
		//组领导进程是否是目标进程
		if (strcmp(leader->comm, sys_cost_settings.comm) != 0)
			return 0;
	}

	return 1;
}

static void start_trace_syscall(struct task_struct *tsk)
{
	struct diag_percpu_context *context;
	struct pt_regs *regs = task_pt_regs(tsk);//获取任务的用户态寄存器上下文
	unsigned long id;

	if (!need_trace(current))
		return;
	if (regs == NULL)
		return;

	id = SYSCALL_NO(regs);//获取当前系统调用号
	if (id >= NR_syscalls_virt)
		return;
	context = get_percpu_context();
	context->sys_cost.start_time = sched_clock();
}

static void stop_trace_syscall(struct task_struct *tsk)
{
	unsigned long id;
	struct pt_regs *regs = task_pt_regs(tsk);
	struct diag_percpu_context *context;
	u64 start;
	u64 now;
	u64 delta_ns;
	int i;

	context = get_percpu_context();
	start = context->sys_cost.start_time;
	context->sys_cost.start_time = 0;

	if (!need_trace(current))
		return;

	if (regs == NULL)
		return;

	id = SYSCALL_NO(regs);
	if (id >= NR_syscalls_virt)
		return;

	
	if (start == 0)
		return;

	now = sched_clock();
	if (now > start)
		delta_ns = now - start;
	else
		delta_ns = 0;
	/*指定pid*/
	if (sys_cost_settings.pid!=0 && sys_cost_settings.tgid==0 && tsk->pid == sys_cost_settings.pid) {
		context->tgid_rcd_scid.tgid=-1;
		context->tgid_rcd_scid.nr_pid = 1;
		struct record_syscall_id *syscall_id = &context->tgid_rcd_scid.syscall_id[0];
		syscall_id->pid = tsk->pid;
		if (syscall_id->count <MAX_SYSCALL_COUNT){
			syscall_id->record_scid[syscall_id->count++] = id;
		}
		else{
			syscall_id->front++;
			syscall_id->record_scid[syscall_id->count % MAX_SYSCALL_COUNT] = id;
			syscall_id->count++;
		}
	}
	/*指定tgid*/
	if ((sys_cost_settings.tgid!=0 && sys_cost_settings.pid==0 && tsk->tgid == sys_cost_settings.tgid)) {
		context->tgid_rcd_scid.tgid = tsk->tgid;
		struct record_syscall_id *syscall_id = &context->tgid_rcd_scid.syscall_id[0];
		for(i=0;i<context->tgid_rcd_scid.nr_pid;i++) {
			if(tsk->pid == syscall_id[i].pid)
				break;
		}
		if(i==context->tgid_rcd_scid.nr_pid){//当前进程未记录过
			syscall_id[context->tgid_rcd_scid.nr_pid].pid = tsk->pid;
			syscall_id[context->tgid_rcd_scid.nr_pid].record_scid[syscall_id[context->tgid_rcd_scid.nr_pid].count++] = id;
			context->tgid_rcd_scid.nr_pid++;
		}
		else{//当前进程已经被记录过;
			if(syscall_id[i].count<MAX_SYSCALL_COUNT){
				syscall_id[i].record_scid[syscall_id[i].count++] = id;
			}
			else{
				syscall_id[i].front++;
				syscall_id[i].record_scid[syscall_id[i].count % MAX_SYSCALL_COUNT] = id;
				syscall_id[i].count++;				
			}
		}
	}

	context->sys_cost.count[id]++;//某一系统调用被引用次数;
	context->sys_cost.cost[id] += delta_ns;//某一系统调用被调用总时长;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static void trace_sys_enter_hit(struct pt_regs *regs, long id)
#else
static void trace_sys_enter_hit(void *__data, struct pt_regs *regs, long id)
#endif
{
	if (!need_trace(current))
		return;
	start_trace_syscall(current);
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
	stop_trace_syscall(prev);
	start_trace_syscall(next);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
static void trace_sys_exit_hit(struct pt_regs *regs, long ret)
#else
static void trace_sys_exit_hit(void *__data, struct pt_regs *regs, long ret)
#endif
{
	stop_trace_syscall(current);
}
/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_sys_cost(void)
{
	int ret;

	ret = alloc_diag_variant_buffer(&sys_cost_variant_buffer);//申请空间
	if (ret)
		goto out_variant_buffer;
	sys_cost_alloced = 1;

	clean_data();

	hook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	hook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	hook_tracepoint("sched_switch", trace_sched_switch, NULL);

	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_sys_cost(void)
{
	unhook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	unhook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	unhook_tracepoint("sched_switch", trace_sched_switch, NULL);

	synchronize_sched();
}

int activate_sys_cost(void)
{
	if (!sys_cost_settings.activated)
		sys_cost_settings.activated = __activate_sys_cost();

	return sys_cost_settings.activated;
}

int deactivate_sys_cost(void)
{
	if (sys_cost_settings.activated)
		__deactivate_sys_cost();
	sys_cost_settings.activated = 0;

	return 0;
}

static void do_dump(void *info)
{
	struct diag_percpu_context *context;
	struct sys_cost_detail *detail;
	unsigned long flags;
	int i;

	context = get_percpu_context();
	detail = &context->sys_cost.detail;
	memset(detail, 0, sizeof(struct sys_cost_detail));
	
	detail->et_type = et_sys_cost_detail;
	do_diag_gettimeofday(&detail->tv);
	detail->cpu = smp_processor_id();
	printk("detail_cpu:%d\n",detail->cpu);
	for (i = 0; i < NR_syscalls_virt && i < USER_NR_syscalls_virt; i++) {
		detail->count[i] = context->sys_cost.count[i];
		detail->cost[i] = context->sys_cost.cost[i];

		context->sys_cost.count[i] = 0;
		context->sys_cost.cost[i] = 0;
	}
	
	detail->tgid_rcd_scid.nr_pid = context->tgid_rcd_scid.nr_pid;
	detail->tgid_rcd_scid.tgid = context->tgid_rcd_scid.tgid;
	for(int j=0;j<detail->tgid_rcd_scid.nr_pid && j<MAX_PID;j++){//遍历tgid中每一个pid;
		struct record_syscall_id *dtl = &detail->tgid_rcd_scid.syscall_id[j];
		struct record_syscall_id *cntxt = &context->tgid_rcd_scid.syscall_id[j];
		dtl->pid = cntxt->pid;
		dtl->front = cntxt->front;
		dtl->count = cntxt->count;
		if(dtl->front == 0)//遍历每一个pid的syscall_id数组;
			for(int k=0;k<dtl->count&& k<MAX_SYSCALL_COUNT;k++){
				dtl->record_scid[k] = cntxt->record_scid[k];
				cntxt->record_scid[k] = 0;
				// printk("DUMP:cpu:%d tgid:%d pid:%d syscall_id %d\n",detail->cpu,detail->tgid_rcd_scid.tgid,dtl->pid,dtl->record_scid[k]);
			}
		else
			for(int k=0;k<MAX_SYSCALL_COUNT;k++){
				dtl->record_scid[k] = cntxt->record_scid[k];
				cntxt->record_scid[k] = 0;
				// printk("DUMP:cpu:%d tgid:%d pid:%d syscall_id %d\n",detail->cpu,detail->tgid_rcd_scid.tgid,dtl->pid,dtl->record_scid[k]);
			}
		cntxt->pid = 0;
		cntxt->front = 0;
		cntxt->count = 0;
	}
	context->tgid_rcd_scid.nr_pid = 0;
	context->tgid_rcd_scid.tgid = 0;
	diag_variant_buffer_spin_lock(&sys_cost_variant_buffer, flags);//即spin_lock_irqsave,禁止本地中断;
	diag_variant_buffer_reserve(&sys_cost_variant_buffer, sizeof(struct sys_cost_detail));
	diag_variant_buffer_write_nolock(&sys_cost_variant_buffer, detail, sizeof(struct sys_cost_detail));
	diag_variant_buffer_seal(&sys_cost_variant_buffer);
	diag_variant_buffer_spin_unlock(&sys_cost_variant_buffer, flags);//即spin_unlock_irqrestore,恢复之前的中断状态;
}

int sys_cost_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_sys_cost_settings settings;
	int cpu;

	switch (id) {
	case DIAG_SYS_COST_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_sys_cost_settings)) {
			ret = -EINVAL;
		} else if (sys_cost_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);//从用户空间将数据copy到settings中
			if (!ret) {//拷贝成功
				sys_cost_settings = settings;//sys_cost_settings是全局变量
			}
		}
		break;
	case DIAG_SYS_COST_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_sys_cost_settings)) {
			ret = -EINVAL;
		} else {
			settings = sys_cost_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);//将settings拷贝到用户空间；
		}
		break;
	case DIAG_SYS_COST_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		for_each_possible_cpu(cpu) {//cpu是当前遍历到的cpu
			if (cpu == smp_processor_id()) {
				printk("start dump cpuid: %d  now_cpu:%d \n",cpu,smp_processor_id());
				do_dump(NULL);
				printk("end dump cpuid: %d \n",cpu);
			} else {
				printk("start call_dump cpuid: %d  now_cpu:%d \n",cpu,smp_processor_id());
				smp_call_function_single(cpu, do_dump, NULL, 1);
				printk("end call_dump cpuid: %d \n",cpu);
			}
		}
		
		if (!sys_cost_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&sys_cost_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			if(ret<0) printk("cpu: %d copy_to_user erro,%d\n",cpu);
			record_dump_cmd("sys-cost");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

long diag_ioctl_sys_cost(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_sys_cost_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_SYS_COST_SET:
		if (sys_cost_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_sys_cost_settings));
			if (!ret) {
				sys_cost_settings = settings;
			}
		}
		break;
	case CMD_SYS_COST_SETTINGS:
		settings = sys_cost_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_sys_cost_settings));
		break;
	case CMD_SYS_COST_DUMP:
		for_each_possible_cpu(cpu) {
			if (cpu == smp_processor_id()) {
				printk("start dump cpuid: %d  now_cpu:%d \n",cpu,smp_processor_id());
				do_dump(NULL);
				printk("end call_dump cpuid: %d \n",cpu);
			} else {
				printk("start call_dump cpuid: %d  now_cpu:%d \n",cpu,smp_processor_id());
				smp_call_function_single(cpu, do_dump, NULL, 1);
				printk("end call_dump cpuid: %d \n",cpu);
			}
		}
		
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!sys_cost_alloced) {
			ret = -EINVAL;
		} else if (!ret){ 
			ret = copy_to_user_variant_buffer(&sys_cost_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			if(ret<0) printk("cpu: %d copy_to_user erro,%d\n",cpu,ret);
			else printk("cpu: %d copy_to_user yes,%d\n",cpu,ret);
			record_dump_cmd("sys-cost");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

int diag_sys_cost_init(void)
{
	init_diag_variant_buffer(&sys_cost_variant_buffer, 10 * 1024 * 1024);

	if (sys_cost_settings.activated)
		sys_cost_settings.activated = __activate_sys_cost();

	return 0;
}

void diag_sys_cost_exit(void)
{
	if (sys_cost_settings.activated)
		deactivate_sys_cost();
	sys_cost_settings.activated = 0;

	msleep(20);
	destroy_diag_variant_buffer(&sys_cost_variant_buffer);
}
