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

#include "uapi/migrate_image.h"

#define cpu_rq(cpu)		(&per_cpu(runqueues, (cpu)))

struct diag_migrate_image_settings migrate_image_settings;

static unsigned int migrate_image_alloced;
static struct diag_variant_buffer migrate_image_variant_buffer;//缓冲区
static atomic64_t diag_nr_running = ATOMIC64_INIT(0);

/*hash表的自定义*/
#define STAMP_BITS 16
#define STAMP_SIZE (1 << STAMP_BITS)
static struct hlist_head ht[STAMP_SIZE];
struct stamp {
	int key; 
	struct migrate_image_detail value; 
	struct hlist_node node;
};

/*查找哈希*/
static struct stamp *find_hash(int key) {
    struct stamp *t;
    hash_for_each_possible(ht, t, node, hash_long(key, STAMP_BITS)) {
        if (key == t->key)
            return t;
    }
	// printk("find err start\n");
    return NULL;
}

/*更新哈希表*/
static void update_hash(int key, struct migrate_image_detail *value) {
    struct stamp *t = find_hash(key);
	if(t){
		t->value = *value;
		// printk("pid : %d \t in update_start_rsc find success!\n",key);
		return;
	}
	// printk("in update_start_rsc find fail!  start to creat a new hashnode!\n");
    t = kmalloc(sizeof(struct stamp), GFP_ATOMIC);
    t->value = *value;
    t->key = key;
    hash_add(ht, &t->node,hash_long(key, STAMP_BITS));
}

/*删除哈希表*/
static void clean_hash(void) {
	struct stamp *t;
	struct hlist_node *tmp;
	int i;
	hash_for_each_safe(ht, i, tmp, t, node) {
		hash_del(&t->node);
		kfree(t);
    }
}
/*将数据向缓冲区提交*/
static void do_dump(void)
{
    unsigned long flags;
	struct stamp *t;
	struct hlist_node *tmp;
	int i,count=0;
	hash_for_each_safe(ht, i, tmp, t, node) {
		if(t->key == 0) {printk("pass\n");continue;}
		struct migrate_image_detail * detail = &t->value;
		diag_variant_buffer_spin_lock(&migrate_image_variant_buffer, flags);
		diag_variant_buffer_reserve(&migrate_image_variant_buffer, sizeof(struct migrate_image_detail));
		diag_variant_buffer_write_nolock(&migrate_image_variant_buffer, detail, sizeof(struct migrate_image_detail));
		diag_variant_buffer_seal(&migrate_image_variant_buffer);
		diag_variant_buffer_spin_unlock(&migrate_image_variant_buffer, flags);
		hash_del(&t->node);
		kfree(t);		
		count++;
    }
	printk("count :%d\n",count);
}

/*
 *当进程在执行到finish_task_switch时，其实已经完成了上下文切换，cpu上运行的是next，prev已经下cpu
 *start_rsc 中记录进程上cpu时历史记载的读写量，当该进程下cpu时，则通过hash表找到start_rsc中记载的值
 *并进行逻辑计算。
 */
static void trace_sched_migrate_task_hit(void *__data,
		struct task_struct *p, int dest_cpu)
{
	if(!migrate_image_settings.pid && !migrate_image_settings.tgid)
		return;	
	long long time = ktime_to_ns(ktime_get());
	int orig_cpu = smp_processor_id();
	struct stamp *stamp = find_hash(p->pid);
	int pid =p->pid;
	/*未对该进程进行记录,则对该进程建立struct migrate_image_detail */
	if((migrate_image_settings.pid == pid && pid !=0 )||
		(migrate_image_settings.tgid == p->tgid && p->tgid !=0)||
		(migrate_image_settings.tgid == 0 && migrate_image_settings.pid == 0)){
		printk("pid:%d\t %d>>%d\n",pid,orig_cpu,dest_cpu);
		if(!stamp){
			struct migrate_image_detail migrate_event = {}; 
			migrate_event.et_type = et_migrate_image_detail;
			do_diag_gettimeofday(&migrate_event.tv);
    	    migrate_event.pid = pid;
    	    migrate_event.prio = p->prio;
    	    migrate_event.count = 1;
    	    migrate_event.migrate_info[0].time = time;
    	    migrate_event.migrate_info[0].orig_cpu = orig_cpu;
    	    migrate_event.migrate_info[0].dest_cpu = dest_cpu; 
    	   	migrate_event.migrate_info[0].pload_avg = p->se.avg.load_avg;//进程的量化负载；
    	    migrate_event.migrate_info[0].putil_avg = p->se.avg.util_avg;//进程的实际算力；
    	    migrate_event.migrate_info[0].mem_usage = p->mm->total_vm << PAGE_SHIFT;
	
    	    migrate_event.migrate_info[0].read_bytes = p->ioac.read_bytes;
    	    migrate_event.migrate_info[0].write_bytes = p->ioac.write_bytes;
	
    	    migrate_event.migrate_info[0].context_switches = p->nvcsw + p->nivcsw;
    	    // per_migrate.runtime =  BPF_CORE_READ(task,se.sum_exec_runtime);
			update_hash(p->pid,&migrate_event);	
		}else if(stamp->value.count < MAX_MIGRATE && stamp->value.count >=0){//找到了
			struct migrate_image_detail migrate_event = stamp->value;
    		migrate_event.migrate_info[migrate_event.count].time = time;
        	migrate_event.migrate_info[migrate_event.count].orig_cpu = orig_cpu;
        	migrate_event.migrate_info[migrate_event.count].dest_cpu = dest_cpu; 
       		migrate_event.migrate_info[migrate_event.count].pload_avg = p->se.avg.load_avg;//进程的量化负载；
        	migrate_event.migrate_info[migrate_event.count].putil_avg = p->se.avg.util_avg;//进程的实际算力；
        	migrate_event.migrate_info[migrate_event.count].mem_usage = p->mm->total_vm << PAGE_SHIFT;

        	migrate_event.migrate_info[migrate_event.count].read_bytes = p->ioac.read_bytes;
        	migrate_event.migrate_info[migrate_event.count].write_bytes = p->ioac.write_bytes;
        	migrate_event.migrate_info[migrate_event.count++].context_switches = p->nvcsw + p->nivcsw;
        	// per_migrate.runtime =  BPF_CORE_READ(task,se.sum_exec_runtime);
			update_hash(p->pid,&migrate_event);			
		} else if(stamp->value.count >= MAX_MIGRATE ){
			struct migrate_image_detail migrate_event = stamp->value;
			int tmp = migrate_event.count % MAX_MIGRATE;
    		migrate_event.migrate_info[tmp].time = time;
    	    migrate_event.migrate_info[tmp].orig_cpu = orig_cpu;
    	    migrate_event.migrate_info[tmp].dest_cpu = dest_cpu; 
    	   	migrate_event.migrate_info[tmp].pload_avg = p->se.avg.load_avg;//进程的量化负载；
    	    migrate_event.migrate_info[tmp].putil_avg = p->se.avg.util_avg;//进程的实际算力；
    	    migrate_event.migrate_info[tmp].mem_usage = p->mm->total_vm << PAGE_SHIFT;

    	    migrate_event.migrate_info[tmp].read_bytes = p->ioac.read_bytes;
    	    migrate_event.migrate_info[tmp].write_bytes = p->ioac.write_bytes;
    	    migrate_event.migrate_info[tmp].context_switches = p->nvcsw + p->nivcsw;
    	    // per_migrate.runtime =  BPF_CORE_READ(task,se.sum_exec_runtime);
			migrate_event.count++;
			migrate_event.rear++;
			update_hash(p->pid,&migrate_event);				
		}
	}
}
/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_migrate_image(void)
{
	int ret;

	ret = alloc_diag_variant_buffer(&migrate_image_variant_buffer);//为缓冲区申请空间
	if (ret)
		goto out_variant_buffer;
	migrate_image_alloced = 1;
	hash_init(ht);
    hook_tracepoint("sched_migrate_task", trace_sched_migrate_task_hit, NULL);
	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_migrate_image(void)
{
	unhook_tracepoint("sched_migrate_task", trace_sched_migrate_task_hit, NULL);
	synchronize_sched();
	msleep(20);
	while (atomic64_read(&diag_nr_running) > 0)
		msleep(10);
    // clean_start_rsc();
    // clean_migrate_image_detail();
}

int activate_migrate_image(void)
{
	if (!migrate_image_settings.activated)
		migrate_image_settings.activated = __activate_migrate_image();

	return migrate_image_settings.activated;
}

int deactivate_migrate_image(void)
{
	if (migrate_image_settings.activated)
		__deactivate_migrate_image();
	migrate_image_settings.activated = 0;

	return 0;
}


int migrate_image_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_migrate_image_settings settings;
	int cpu;

	switch (id) {
	case DIAG_MIGRATE_IMAGE_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_migrate_image_settings)) {
			ret = -EINVAL;
		} else if (migrate_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				migrate_image_settings = settings;
			}
		}
		break;
	case DIAG_MIGRATE_IMAGE_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_migrate_image_settings)) {
			ret = -EINVAL;
		} else {
			settings = migrate_image_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
		}
		break;
	case DIAG_MIGRATE_IMAGE_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);
		if (!migrate_image_alloced) {
			ret = -EINVAL;
		} else {
			do_dump();
			ret = copy_to_user_variant_buffer(&migrate_image_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("migrate-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

long diag_ioctl_migrate_image(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_migrate_image_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_MIGRATE_IMAGE_SET:
		if (migrate_image_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_migrate_image_settings));
			if (!ret) {
				migrate_image_settings = settings;
			}
		}
		break;
	case CMD_MIGRATE_IMAGE_SETTINGS:
		settings = migrate_image_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_migrate_image_settings));
		break;
	case CMD_MIGRATE_IMAGE_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!migrate_image_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			do_dump();
			ret = copy_to_user_variant_buffer(&migrate_image_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("migrate-image");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

/*migrate_image入口*/
int diag_migrate_image_init(void)
{
	init_diag_variant_buffer(&migrate_image_variant_buffer, 1 * 1024 * 1024);//初始化缓冲区

	if (migrate_image_settings.activated)//激活该功能
		migrate_image_settings.activated = __activate_migrate_image();//调用__activate_migrate_image激活该功能（挂载到挂载点）

	return 0;
}

void diag_migrate_image_exit(void)
{
	if (migrate_image_settings.activated)
		deactivate_migrate_image();//撤销挂载点
	migrate_image_settings.activated = 0;
    clean_hash();
	destroy_diag_variant_buffer(&migrate_image_variant_buffer);//释放缓冲区
}
