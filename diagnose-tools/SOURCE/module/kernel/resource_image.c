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
#include <linux/mm_types.h>
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
#include <linux/hashtable.h>
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

#include "internal.h"
#include "pub/trace_file.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"

#include "uapi/resource_image.h"

struct diag_resource_image_settings resource_image_settings;

static unsigned int resource_image_alloced;
static struct diag_variant_buffer resource_image_variant_buffer;//缓冲区

static struct kprobe kprobe_finish_task_switch;
static atomic64_t diag_nr_running = ATOMIC64_INIT(0);
/*hash表的自定义*/
#define STAMP_BITS 16
#define STAMP_SIZE (1 << STAMP_BITS)
static struct hlist_head ht_start[STAMP_SIZE];
static struct hlist_head ht_total[STAMP_SIZE];
struct stamp_start {
	int key; 
	struct start_rsc value; 
	struct hlist_node node;
};
struct stamp_total {
    int key;
    struct resource_image_detail value;
    struct hlist_node node;
};

/*查找哈希*/
// 查找 start_rsc 的哈希表
static struct stamp_start *find_start_rsc(int key) {
    struct stamp_start *t;
    hash_for_each_possible(ht_start, t, node, hash_long(key, STAMP_BITS)) {
        if (key == t->key)
            return t;
    }
	// printk("find err start\n");
    return NULL;
}

// 查找 resource_image_detail 的哈希表
static struct stamp_total *find_resource_image_detail(int key) {
    struct stamp_total *t;
    hash_for_each_possible(ht_total, t, node, hash_long(key, STAMP_BITS)) {
        if (key== t->key)
            return t;
    }
	// printk("find err total\n");
    return NULL;
}
/*更新哈希表*/
static void update_start_rsc(int key, struct start_rsc *value) {
    struct stamp_start *t = find_start_rsc(key);
	if(t){
		t->value = *value;
		return;
	}
    t = kmalloc(sizeof(struct stamp_start), GFP_ATOMIC);
    t->value = *value;
    t->key = key;
    hash_add(ht_start, &t->node,hash_long(key, STAMP_BITS));
}
static void update_resource_image_detail(int key, struct resource_image_detail *value) {
    struct stamp_total *t = find_resource_image_detail(key);
	if(t){
		t->value = *value;
		// printk("pid : %d \t in update_resource_image_detail find success!\n",key);
		return;
	}
	// printk("in update_resource_image_detail find fail!  start to creat a new hashnode!\n");
    t = kmalloc(sizeof(struct stamp_total), GFP_ATOMIC);
    t->value = *value;
    t->key = key;
    hash_add(ht_total, &t->node, hash_long(key, STAMP_BITS));
}

/*删除哈希表*/
static void clean_start_rsc(void) {
	struct stamp_start *t;
	struct hlist_node *tmp;
	int i;
	hash_for_each_safe(ht_start, i, tmp, t, node) {
		hash_del(&t->node);
		kfree(t);
    }
}
static void clean_resource_image_detail(void) {
	struct stamp_total *t;
	struct hlist_node *tmp;
	int i;
	hash_for_each_safe(ht_start, i, tmp, t, node) {
		hash_del(&t->node);
		kfree(t);
    }
}

/*将数据向缓冲区提交*/
static void do_dump(void)
{
    unsigned long flags;
	struct stamp_total *t;
	struct hlist_node *tmp;
	int i,count=0;
	hash_for_each_safe(ht_total, i, tmp, t, node) {
		if(t->key == 0) {printk("pass\n");continue;}
		struct resource_image_detail * detail = &t->value;
		diag_variant_buffer_spin_lock(&resource_image_variant_buffer, flags);
		diag_variant_buffer_reserve(&resource_image_variant_buffer, sizeof(struct resource_image_detail));
		diag_variant_buffer_write_nolock(&resource_image_variant_buffer, detail, sizeof(struct resource_image_detail));
		diag_variant_buffer_seal(&resource_image_variant_buffer);
		diag_variant_buffer_spin_unlock(&resource_image_variant_buffer, flags);
		count++;
    }
	printk("count :%d\n",count);
}


/*
 *当进程在执行到finish_task_switch时，其实已经完成了上下文切换，cpu上运行的是next，prev已经下cpu
 *start_rsc 中记录进程上cpu时历史记载的读写量，当该进程下cpu时，则通过hash表找到start_rsc中记载的值
 *并进行逻辑计算。
 */
static void hook_finish_task_switch(struct task_struct *prev){
    // printk("readchar:%llu \t writechar:%llu \t cpu: %d \t",prev->ioac.rchar,prev->ioac.wchar,smp_processor_id());
	if(!resource_image_settings.pid && !resource_image_settings.tgid)
		return;
    pid_t prev_pid = prev->pid;
    int prev_cpu = smp_processor_id();
    int prev_tgid = prev->tgid;

    struct task_struct *next = current;
    pid_t next_pid = next->pid;
    int next_cpu = prev_cpu;
    int next_tgid = next->tgid;   

	/*next上cpu时,对初始数据进行记录*/
	if((resource_image_settings.pid == 0 && resource_image_settings.tgid == 0)|| //无目标pid tgid 
        (resource_image_settings.pid != 0 && next_pid == resource_image_settings.pid) //指定目标pid
        || (next_tgid==resource_image_settings.tgid))//指定目标tgid
        /*|| (resource_image_settings.pid==0 && next_pid==resource_image_settings.pid && next_cpu==resource_image_settings.cpuid)*/ 
    {	
		int key= next_pid;
		struct start_rsc next_start={};
		
		next_start.time = ktime_to_ns(ktime_get());
		next_start.readchar = next->ioac.rchar;
		next_start.writechar = next->ioac.wchar;
		update_start_rsc(key,&next_start);
	} 
    /*prev下cpu时,进行数据汇总*/ 
    if((resource_image_settings.pid == prev_pid && prev_pid != 0 )||//指定pid
        (prev_tgid == resource_image_settings.tgid && prev_tgid != 0)//指定tgid
        ||(resource_image_settings.pid == 0 && resource_image_settings.tgid == 0))//未指定pid或tgid
        /*|| (resource_image_settings.pid==0 && prev_pid==resource_image_settings.pid && prev_cpu==resource_image_settings.cpuid)*/ 
    {
        int key= prev_pid;

        struct stamp_start *prev_stamp_start = find_start_rsc(key);
        if(!prev_stamp_start) return ;
        struct start_rsc *prev_start = &prev_stamp_start->value;
        struct stamp_total *prev_stamp_total = find_resource_image_detail(key);
        if(!prev_stamp_total){//没找到对应的值
			struct resource_image_detail prev_total = {};
			long unsigned int memused;
			prev_total.et_type = et_resource_image_detail;
	        do_diag_gettimeofday(&prev_total.tv);
			struct mm_rss_stat *rss =&prev->mm->rss_stat;
			if(!rss)	return ;
			memused = atomic_long_read(&rss->count[0]) + atomic_long_read(&rss->count[1]) + atomic_long_read(&rss->count[3]);
			
			prev_total.pid = key;
			if(resource_image_settings.tgid != 0)	prev_total.tgid = prev_tgid;
			else	prev_total.tgid = 0;
			prev_total.cpuid = prev_cpu;
			prev_total.time = ktime_to_ns(ktime_get()) - prev_start->time;
			prev_total.memused = memused;
			prev_total.readchar = prev->ioac.rchar - prev_start->readchar;
			prev_total.writechar = prev->ioac.wchar - prev_start->writechar;
            update_resource_image_detail(key,&prev_total);
        }else{//找到了对应的值
			struct resource_image_detail *prev_total = &prev_stamp_total->value;
			if (prev_total == NULL) {
				return ;
			}
			long unsigned int memused;
			prev_total->et_type = et_resource_image_detail;
	        do_diag_gettimeofday(&prev_total->tv);
			struct mm_rss_stat *rss =&prev->mm->rss_stat;
			if(!rss)	return ;
			memused = atomic_long_read(&rss->count[0]) + atomic_long_read(&rss->count[1]) + atomic_long_read(&rss->count[3]);
			
			prev_total->cpuid = prev_cpu;
			prev_total->time += (ktime_to_ns(ktime_get()) - prev_start->time);
			prev_total->memused = memused;
			prev_total->readchar += prev->ioac.rchar - prev_start->readchar;
			prev_total->writechar += prev->ioac.wchar - prev_start->writechar;
            update_resource_image_detail(key,prev_total);//将prev进程在当前cpu上运行的数据放入ht_total哈希表中;
        }
		hash_del(&prev_stamp_start->node);
    }
}

static int kprobe_finish_task_switch_pre(struct kprobe *p, struct pt_regs *regs)

{
    struct task_struct *prev = (struct task_struct *)ORIG_PARAM1(regs);
	hook_finish_task_switch(prev);
    return 0;
}
/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_resource_image(void)
{
	int ret;

	ret = alloc_diag_variant_buffer(&resource_image_variant_buffer);//为缓冲区申请空间
	if (ret)
		goto out_variant_buffer;
	resource_image_alloced = 1;
    hash_init(ht_total);
    hash_init(ht_start);
                /*kprobe结构体              挂载点                       回调函数                        NULL*/
	hook_kprobe(&kprobe_finish_task_switch,"finish_task_switch.isra.0", kprobe_finish_task_switch_pre, NULL);
	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_resource_image(void)
{
	unhook_kprobe(&kprobe_finish_task_switch);
	synchronize_sched();
	msleep(20);
	while (atomic64_read(&diag_nr_running) > 0)
		msleep(20);
    clean_start_rsc();
    clean_resource_image_detail();
}

int activate_resource_image(void)
{
	if (!resource_image_settings.activated)
		resource_image_settings.activated = __activate_resource_image();

	return resource_image_settings.activated;
}

int deactivate_resource_image(void)
{
	if (resource_image_settings.activated)
		__deactivate_resource_image();
	resource_image_settings.activated = 0;

	return 0;
}


int resource_image_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_resource_image_settings settings;
	int cpu;

	switch (id) {
	case DIAG_RESOURCE_IMAGE_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_resource_image_settings)) {
			ret = -EINVAL;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				resource_image_settings = settings;
				if(resource_image_settings.activated==0)
					resource_image_settings.prevt = ktime_to_ns(ktime_get());//获取开始执行时间;
			}
		}
		break;
	case DIAG_RESOURCE_IMAGE_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_resource_image_settings)) {
			ret = -EINVAL;
		} else {
			settings = resource_image_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
			printk("prevt:%ld \t cur:%ld \t delay:%ld\n",settings.prevt,settings.cur,settings.cur-settings.prevt);
			// resource_image_settings.prevt=resource_image_settings.cur;//获取当前时间；
		}
		break;
	case DIAG_RESOURCE_IMAGE_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);
		if (!resource_image_alloced) {
			ret = -EINVAL;
		} else {
			resource_image_settings.cur=ktime_to_ns(ktime_get());//获取当前时间；
			do_dump();
			ret = copy_to_user_variant_buffer(&resource_image_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("resource-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

long diag_ioctl_resource_image(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_resource_image_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_RESOURCE_IMAGE_SET:
		ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_resource_image_settings));
		if (!ret) {
			resource_image_settings = settings;
			if(resource_image_settings.activated==0)
				resource_image_settings.prevt = ktime_to_ns(ktime_get());//获取开始执行时间;
		}
		break;
	case CMD_RESOURCE_IMAGE_SETTINGS:
		settings = resource_image_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_resource_image_settings));

		printk("prevt:%ld \t cur :%ld \t delay :%ld \n",settings.prevt,settings.cur,settings.cur-settings.prevt);
		// resource_image_settings.prevt=resource_image_settings.cur;//获取当前时间；
		break;
	case CMD_RESOURCE_IMAGE_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!resource_image_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			resource_image_settings.cur=ktime_to_ns(ktime_get());//获取当前时间；
			do_dump();
			ret = copy_to_user_variant_buffer(&resource_image_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("resource-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

/*resource_image入口*/
int diag_resource_image_init(void)
{
	init_diag_variant_buffer(&resource_image_variant_buffer, 50 * 1024 * 1024);//初始化缓冲区

	if (resource_image_settings.activated)//激活该功能
		resource_image_settings.activated = __activate_resource_image();//调用__activate_resource_image激活该功能（挂载到挂载点）

	return 0;
}

void diag_resource_image_exit(void)
{
	if (resource_image_settings.activated)
		deactivate_resource_image();//撤销挂载点
	resource_image_settings.activated = 0;
    clean_start_rsc();
    clean_resource_image_detail();
	destroy_diag_variant_buffer(&resource_image_variant_buffer);//释放缓冲区
}
