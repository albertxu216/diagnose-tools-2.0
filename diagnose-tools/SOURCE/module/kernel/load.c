/*
 * Linux内核诊断工具--内核态load-monitor功能
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
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/sched/loadavg.h>
#include <linux/seq_file.h>

#include <asm/irq_regs.h>

#include "internal.h"
#include "mm_tree.h"
#include "pub/trace_file.h"
#include "pub/variant_buffer.h"
#include "pub/trace_point.h"

#include "uapi/load_monitor.h"


#define task_contributes_to_load(task)	\
				((task->__state & TASK_UNINTERRUPTIBLE) != 0 && \
				 (task->flags & PF_FROZEN) == 0)
#ifndef FSHIFT
#define FSHIFT		11		/* nr of bits of precision */
#endif
#ifndef FIXED_1
#define FIXED_1		(1<<FSHIFT)	/* 1.0 as fixed-point */
#endif
#ifndef LOAD_INT
#define LOAD_INT(x) ((x) >> FSHIFT)
#endif
#ifndef LOAD_FRAC
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)
#endif
#define INTERVAL_NS  (5ULL * NSEC_PER_SEC)   //采样间隔s
#define BUFF_SIZE 2048            //缓冲区大小

static struct hrtimer load_hrtimer;
static struct timer_list duration_timer;
struct load_time_buffer buffer; 
struct proc_dir_entry * load_trend_entry = NULL;
static char * entry_name = "load_trend";
static int diag_load_time_init(void);
static void diag_load_time_exit(void);

static atomic64_t diag_nr_running = ATOMIC64_INIT(0);

static struct diag_load_monitor_settings load_monitor_settings;

static unsigned int load_monitor_alloced;

static struct mm_tree mm_tree;

unsigned long *orig_avenrun_r;
unsigned long *orig_avenrun;

static struct load_monitor_cpu_run ld_mon_cpu_run[NR_CPUS];
static struct diag_variant_buffer load_monitor_variant_buffer;

static ktime_t last_dump;

static void __maybe_unused clean_data(void)
{
	cleanup_mm_tree(&mm_tree);
}

#if defined(UPSTREAM_4_19_32)
void diag_load_timer(struct diag_percpu_context *context)
{
	return;
}
#else
struct load_time_data{
    s64 timestamp; 
    unsigned int load_1[2]; 
    unsigned int load_5[2];
    unsigned int load_15[2];  
};
struct load_time_buffer {
    struct load_time_data datas[BUFF_SIZE];
    int front;  
    int rear;
    spinlock_t lock;   
};

static void * load_trend_start(struct seq_file *m, loff_t *pos)
{
    return (*pos < 1) ? pos : NULL;
}

static int load_trend_show(struct seq_file *m, void *p)
{
    unsigned long flags;
    int i = buffer.front;

    spin_lock_irqsave(&buffer.lock, flags);
    seq_printf(m,"Timestamp(s)\tLoad_1\t\tLoad_5\t\tLoad_15\n");
    while (i != buffer.rear) {
        seq_printf(m, "%llu\t\t%d.%02d\t\t%d.%02d\t\t%d.%02d\n", buffer.datas[i].timestamp, buffer.datas[i].load_1[0], buffer.datas[i].load_1[1],
        buffer.datas[i].load_5[0], buffer.datas[i].load_5[1],buffer.datas[i].load_15[0], buffer.datas[i].load_15[1]);
        i = (i + 1) % BUFF_SIZE;
    }
    spin_unlock_irqrestore(&buffer.lock, flags);
    return 0;
}

static void * load_trend_next(struct seq_file *m, void *p, loff_t *pos)
{
    (*pos)++;

    if (*pos >= 1)
        return NULL;

    return pos;
}

static void load_trend_stop(struct seq_file *m, void *p)
{
      return;
}

static struct seq_operations load_trend_seq_ops =
{
    .start = load_trend_start,
    .next = load_trend_next,
    .stop = load_trend_stop,
    .show = load_trend_show,
};

static int load_trend_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &load_trend_seq_ops);
}

static const struct proc_ops load_trend_fops =
{
    .proc_open = load_trend_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

void init_buffer(struct load_time_buffer *buf) {
    buf->front = 0;
    buf->rear = 0;
    spin_lock_init(&buf->lock);
    if (!buf) {
        printk(KERN_ERR "Invalid argument: buf is NULL\n");
        return;
    }
}

void add_data(struct load_time_buffer *buf, s64 timestamp) 
{
    // 如果缓冲区已满，则删除最早的时间和数据
    if ((buf->rear + 1) % BUFF_SIZE == buf->front) {
        buf->front = (buf->front + 1) % BUFF_SIZE;
    }
    // 将新数据添加到尾部
    buf->datas[buf->rear].timestamp = timestamp;
    buf->datas[buf->rear].load_1[0] = LOAD_INT(avenrun[0]);
    buf->datas[buf->rear].load_1[1] = LOAD_FRAC(avenrun[0]);
    buf->datas[buf->rear].load_5[0] = LOAD_INT(avenrun[1]);
    buf->datas[buf->rear].load_5[1] = LOAD_FRAC(avenrun[1]);
    buf->datas[buf->rear].load_15[0] = LOAD_INT(avenrun[2]);
    buf->datas[buf->rear].load_15[1] = LOAD_FRAC(avenrun[2]);
    buf->rear = (buf->rear + 1) % BUFF_SIZE;
}

void duration_timer_callback(struct timer_list  *timer)
{
    hrtimer_cancel(&load_hrtimer);
}

void deliver_in_proc(void)
{
    load_trend_entry = proc_create(entry_name, 0666, NULL, &load_trend_fops);
    if (!load_trend_entry)
    {
        printk("Create file \"%s\" failed.\n", entry_name);
        return ;
    }
    
}

enum hrtimer_restart load_hrtimer_callback(struct hrtimer *timer)
{
    s64 now_seconds;
    ktime_t now_ns;
    now_ns = ktime_get();
    now_seconds = ktime_to_ns(now_ns) / NSEC_PER_SEC;
    add_data(&buffer,now_seconds);
    hrtimer_forward_now(timer, ns_to_ktime(INTERVAL_NS));
    return HRTIMER_RESTART;
}

static void load_monitor_ipi(void *ignore)
{
	struct load_monitor_cpu_run *cpu_run;
	struct task_struct *tsk;
	unsigned long flags;
	int cpu;

	tsk = current;
	cpu = smp_processor_id();
	if (cpu >= NR_CPUS)
		return;

	cpu_run = &ld_mon_cpu_run[cpu];

	cpu_run->id = get_cycles();
	cpu_run->et_type = et_load_monitor_cpu_run;
	cpu_run->cpu = cpu;
	do_diag_gettimeofday(&cpu_run->tv);

	diag_task_brief(tsk, &cpu_run->task);
	diag_task_kern_stack(tsk, &cpu_run->kern_stack);
	diag_task_user_stack(tsk, &cpu_run->user_stack);

	diag_variant_buffer_spin_lock(&load_monitor_variant_buffer, flags);
	diag_variant_buffer_reserve(&load_monitor_variant_buffer, sizeof(struct load_monitor_cpu_run));
	diag_variant_buffer_write_nolock(&load_monitor_variant_buffer,
					 cpu_run, sizeof(struct load_monitor_cpu_run));
	diag_variant_buffer_seal(&load_monitor_variant_buffer);
	diag_variant_buffer_spin_unlock(&load_monitor_variant_buffer, flags);
}

void diag_load_timer(struct diag_percpu_context *context)
{
	u64 ms;
	bool scare = false;
	unsigned long load_d;
	struct task_struct *g, *p;
	static struct load_monitor_detail detail;
	static struct load_monitor_task tsk_info;

    static int load_flag=0;
    int load = LOAD_INT(avenrun[0]);	
    if(load >= load_monitor_settings.threshold_load && load_flag == 0) 
    {
        load_flag = 1;
        struct timespec64 ts;
        ktime_get_real_ts64(&ts);
        sprintf(detail.bad_time, "[%02lld:%02lld:%02lld]\n",
        ((ts.tv_sec / 3600)+8) % 24, (ts.tv_sec / 60) % 60, ts.tv_sec % 60);
        sprintf(detail.init_load, "%lu.%02lu, %lu.%02lu, %lu.%02lu\n",
        LOAD_INT(avenrun[0]), LOAD_FRAC(avenrun[0]),
        LOAD_INT(avenrun[1]), LOAD_FRAC(avenrun[1]),
        LOAD_INT(avenrun[2]), LOAD_FRAC(avenrun[2]));
    }else if(load < load_monitor_settings.threshold_load && load_flag == 1)
    {
        load_flag=0;
    }

	if (!load_monitor_settings.activated)
		return;

	if (!load_monitor_settings.threshold_load && !load_monitor_settings.threshold_load_r && !load_monitor_settings.threshold_load_d
		&& !load_monitor_settings.threshold_task_d)
		return;
	if (smp_processor_id() != 0)
		return;

	if (load_monitor_settings.threshold_load && orig_avenrun
			&& LOAD_INT(orig_avenrun[0]) >= load_monitor_settings.threshold_load)
		scare = true;
	if (load_monitor_settings.threshold_load_r && orig_avenrun_r
			&& LOAD_INT(orig_avenrun_r[0]) >= load_monitor_settings.threshold_load_r)
		scare = true;
	if (load_monitor_settings.threshold_load_d && orig_avenrun && orig_avenrun_r) {
		load_d = LOAD_INT(orig_avenrun[0] - orig_avenrun_r[0]);

		if (load_d >= load_monitor_settings.threshold_load_d && load_d < 999999)
			scare = true;
	}
	if (load_monitor_settings.threshold_task_d) {
		int nr_uninterrupt = 0;
		struct task_struct *g, *p;

		rcu_read_lock();

		do_each_thread(g, p) {
			if (task_contributes_to_load(p))
				nr_uninterrupt++;
		} while_each_thread(g, p);

		rcu_read_unlock();

		if (nr_uninterrupt >= load_monitor_settings.threshold_task_d)
			scare = true;
	}
	if (scare) {
		unsigned long flags;
		unsigned long event_id;
		ms = ktime_to_ms(ktime_sub(ktime_get(), last_dump));
		if (!load_monitor_settings.mass && ms < 10 * 1000)
			return;

		last_dump = ktime_get();
		
		if (orig_avenrun) {
			detail.load_1_1 = LOAD_INT(orig_avenrun[0]);
			detail.load_1_2 = LOAD_FRAC(orig_avenrun[0]);
			detail.load_5_1 = LOAD_INT(orig_avenrun[1]);
			detail.load_5_2 = LOAD_FRAC(orig_avenrun[1]);
			detail.load_15_1 = LOAD_INT(orig_avenrun[2]);
			detail.load_15_2 = LOAD_FRAC(orig_avenrun[2]);
		}
		if (orig_avenrun && orig_avenrun_r) {
			unsigned long l1, l2, l3;

			detail.load_r_1_1 = LOAD_INT(orig_avenrun_r[0]);
			detail.load_r_1_2 = LOAD_FRAC(orig_avenrun_r[0]);
			detail.load_r_5_1 = LOAD_INT(orig_avenrun_r[1]);
			detail.load_r_5_2 = LOAD_FRAC(orig_avenrun_r[1]);
			detail.load_r_15_1 = LOAD_INT(orig_avenrun_r[2]);
			detail.load_r_15_2 = LOAD_FRAC(orig_avenrun_r[2]);
			l1 = orig_avenrun[0] - orig_avenrun_r[0];
			l2 = orig_avenrun[1] - orig_avenrun_r[1];
			l3 = orig_avenrun[2] - orig_avenrun_r[2];
			detail.load_d_1_1 = LOAD_INT(l1);
			detail.load_d_1_2 = LOAD_FRAC(l1);
			detail.load_d_5_1 = LOAD_INT(l2);
			detail.load_d_5_2 = LOAD_FRAC(l2);
			detail.load_d_15_1 = LOAD_INT(l3);
			detail.load_d_15_2 = LOAD_FRAC(l3);
		}

		event_id = get_cycles();
		detail.id = event_id;
		detail.et_type = et_load_monitor_detail;
		do_diag_gettimeofday(&detail.tv);

		rcu_read_lock();
		diag_variant_buffer_spin_lock(&load_monitor_variant_buffer, flags);
		diag_variant_buffer_reserve(&load_monitor_variant_buffer, sizeof(struct load_monitor_detail));
		diag_variant_buffer_write_nolock(&load_monitor_variant_buffer, &detail, sizeof(struct load_monitor_detail));
		diag_variant_buffer_seal(&load_monitor_variant_buffer);
		diag_variant_buffer_spin_unlock(&load_monitor_variant_buffer, flags);
		do_each_thread(g, p) {
			if ((p->__state == TASK_RUNNING)
				|| task_contributes_to_load(p)) {
				tsk_info.et_type = et_load_monitor_task;
				tsk_info.id = event_id;
				tsk_info.tv = detail.tv;
				diag_task_brief(p, &tsk_info.task);
				diag_task_kern_stack(p, &tsk_info.kern_stack);
				dump_proc_chains_argv(load_monitor_settings.style, &mm_tree, p, &tsk_info.proc_chains);
				diag_variant_buffer_spin_lock(&load_monitor_variant_buffer, flags);
				diag_variant_buffer_reserve(&load_monitor_variant_buffer, sizeof(struct load_monitor_task));
				diag_variant_buffer_write_nolock(&load_monitor_variant_buffer,
					&tsk_info, sizeof(struct load_monitor_task));
				diag_variant_buffer_seal(&load_monitor_variant_buffer);
				diag_variant_buffer_spin_unlock(&load_monitor_variant_buffer, flags);
			}
		} while_each_thread(g, p);
		rcu_read_unlock();
		if (!load_monitor_settings.mass && load_monitor_settings.cpu_run) {
			smp_call_function(load_monitor_ipi, NULL, 1);
			load_monitor_ipi(NULL);
		}
	}
}
#endif

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exec_hit(void *__data,
	struct task_struct *tsk,
	pid_t old_pid,
	struct linux_binprm *bprm)
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
__maybe_unused static void trace_sched_process_exec_hit(void *__data,
	struct task_struct *tsk,
	pid_t old_pid,
	struct linux_binprm *bprm)
#endif
#if KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
{
	atomic64_inc_return(&diag_nr_running);
	diag_hook_exec(bprm, &mm_tree);
	atomic64_dec_return(&diag_nr_running);
}
#endif

#if KERNEL_VERSION(4, 9, 0) <= LINUX_VERSION_CODE
static void trace_sched_process_exit_hit(void *__data, struct task_struct *tsk)
#elif KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
static void trace_sched_process_exit_hit(void *__data, struct task_struct *tsk)
#else
static void trace_sched_process_exit_hit(struct task_struct *tsk)
#endif
{
	diag_hook_process_exit_exec(tsk, &mm_tree);
}

static int __activate_load_monitor(void)
{
	int ret = 0;

	clean_data();
	if (load_monitor_settings.time>0)
		diag_load_time_init();

	ret = alloc_diag_variant_buffer(&load_monitor_variant_buffer);
	if (ret)
		goto out_variant_buffer;
	load_monitor_alloced = 1;

	if (load_monitor_settings.style == 1) {
#if KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
		hook_tracepoint("sched_process_exec", trace_sched_process_exec_hit, NULL);
#endif
		hook_tracepoint("sched_process_exit", trace_sched_process_exit_hit, NULL);
	}
	//get_argv_processes(&mm_tree);

	return 1;
out_variant_buffer:
	return 0;
}

int activate_load_monitor(void)
{
	if (!load_monitor_settings.activated)
		load_monitor_settings.activated = __activate_load_monitor();

	return load_monitor_settings.activated;
}

static void __deactivate_load_monitor(void)
{
	if (load_monitor_settings.style == 1) {
#if KERNEL_VERSION(3, 10, 0) <= LINUX_VERSION_CODE
		unhook_tracepoint("sched_process_exec", trace_sched_process_exec_hit, NULL);
#endif
		unhook_tracepoint("sched_process_exit", trace_sched_process_exit_hit, NULL);
	}

	synchronize_sched();
	msleep(10);
	while (atomic64_read(&diag_nr_running) > 0) {
		msleep(10);
	}

	clean_data();
	if (load_monitor_settings.time > 0)
		diag_load_time_exit();
	load_monitor_settings.verbose = 0;
	load_monitor_settings.threshold_load = 0;
	load_monitor_settings.threshold_load_r = 0;
	load_monitor_settings.threshold_load_d = 0;
	load_monitor_settings.threshold_task_d = 0;
	load_monitor_settings.time=0;
	last_dump = ktime_set(0, 0);
}

int deactivate_load_monitor(void)
{
	if (load_monitor_settings.activated)
		__deactivate_load_monitor();
	load_monitor_settings.activated = 0;

	return load_monitor_settings.activated;
}

int load_monitor_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_load_monitor_settings settings;

	switch (id) {
	case DIAG_LOAD_MONITOR_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_load_monitor_settings)) {
			ret = -EINVAL;
		} else if (load_monitor_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				load_monitor_settings = settings;
			}
		}
		break;
	case DIAG_LOAD_MONITOR_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_load_monitor_settings)) {
			ret = -EINVAL;
		} else {
			settings = load_monitor_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
		}
		break;
	case DIAG_LOAD_MONITOR_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		if (!load_monitor_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&load_monitor_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("load-monitor");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

long diag_ioctl_load_monitor(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_load_monitor_settings settings;
	struct diag_ioctl_dump_param dump_param;

	switch (cmd) {
	case CMD_LOAD_MONITOR_SET:
		if (load_monitor_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_load_monitor_settings));
			if (!ret) {
				load_monitor_settings = settings;
			}
		}
		break;
	case CMD_LOAD_MONITOR_SETTINGS:
		settings = load_monitor_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_load_monitor_settings));
		break;
	case CMD_LOAD_MONITOR_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!load_monitor_alloced) {
			ret = -EINVAL;
		} else if (!ret) {
			ret = copy_to_user_variant_buffer(&load_monitor_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("load-monitor");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}
static int diag_load_time_init(void)
{
    printk("diag_load_time_init!");
    deliver_in_proc();
    init_buffer(&buffer);
    timer_setup(&duration_timer, duration_timer_callback, 0);
    hrtimer_init(&load_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    mod_timer(&duration_timer, jiffies + load_monitor_settings.time * HZ);
    load_hrtimer.function = &load_hrtimer_callback;
    hrtimer_start(&load_hrtimer, ns_to_ktime(INTERVAL_NS), HRTIMER_MODE_REL);
    return 0;
}

static void diag_load_time_exit(void)
{
    printk("diag_load_time_exit!");
    proc_remove(load_trend_entry);
	hrtimer_cancel(&load_hrtimer);
    del_timer(&duration_timer);
}
int diag_load_init(void)
{
	LOOKUP_SYMS_NORET(avenrun_r);
	LOOKUP_SYMS_NORET(avenrun);

	init_mm_tree(&mm_tree);
	init_diag_variant_buffer(&load_monitor_variant_buffer, 50 * 1024 * 1024);
	if (load_monitor_settings.activated)
		load_monitor_settings.activated = __activate_load_monitor();
	return 0;
}

void diag_load_exit(void)
{
	if (load_monitor_settings.activated)
		deactivate_load_monitor();
	load_monitor_settings.activated = 0;
	destroy_diag_variant_buffer(&load_monitor_variant_buffer);
}
