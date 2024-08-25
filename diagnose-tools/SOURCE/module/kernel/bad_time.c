#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/sched/loadavg.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/kprobes.h>

#include "internal.h"

int loadmax= 4;
module_param(loadmax,int,0);

struct hrtimer timer; /* 创建一个计时器 */

#define BACKTRACE_DEPTH 20

void *find_kallsyms_lookup_name(char *sym) {
    int ret;
    void *p; 
    struct kprobe kp = { 
        .symbol_name = sym,
    };
    if ((ret = register_kprobe(&kp)) < 0) { 
	    printk(KERN_INFO "register_kprobe failed, error\n", ret);
        return NULL;
	}
    
    p = kp.addr;
	printk(KERN_INFO "%s addr: %lx\n", sym, (unsigned long)p);
	unregister_kprobe(&kp);	
	return p;
}

unsigned int (*StackTraceSaveTask)(struct task_struct *tsk, unsigned long *store, unsigned int size, unsigned int skipnr); 

static void check_load(void) { /* 主要的计时器触发后的程序 */
    static int flag=0;
    int load = LOAD_INT(avenrun[0]);	
    if(load >= loadmax && flag == 0) 
    {
        printk("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        flag = 1;
        struct timespec64 ts;
        ktime_get_real_ts64(&ts);
        printk(KERN_INFO "bad time: %02d:%02d:%02d\n", 
            ((ts.tv_sec / 3600)+8) % 24, (ts.tv_sec / 60) % 60, ts.tv_sec % 60);
        
        struct task_struct *g, *p;
        unsigned long backtrace[BACKTRACE_DEPTH];
        unsigned int nr_bt;
        printk("Load: %lu.%02lu, %lu.%02lu, %lu.%02lu\n", /* 输出近期平均负载 */
            LOAD_INT(avenrun[0]), LOAD_FRAC(avenrun[0]),
            LOAD_INT(avenrun[1]), LOAD_FRAC(avenrun[1]),
            LOAD_INT(avenrun[2]), LOAD_FRAC(avenrun[2]));
        rcu_read_lock(); /* 为运行队列上锁 */
        printk("dump running task.\n");
        do_each_thread(g, p) { /* 遍历运行队列 */
            if(p->__state == TASK_RUNNING) {
                printk("running task, comm: %s, pid %d\n", p->comm, p->pid);
                nr_bt = StackTraceSaveTask(p, backtrace, BACKTRACE_DEPTH, 0); /* 保存栈 */ 
                stack_trace_print(backtrace, nr_bt, 0); /* 打印栈 */
            }
        } while_each_thread(g, p);
        printk("dump uninterrupted task.\n");
        do_each_thread(g, p) { /* 和上面的遍历类似 */
            if(p->__state & TASK_UNINTERRUPTIBLE) {
                printk("uninterrupted task, comm: %s, pid %d\n", p->comm, p->pid);
                nr_bt = StackTraceSaveTask(p, backtrace, BACKTRACE_DEPTH, 0); /* 保存栈 */ 
                stack_trace_print(backtrace, nr_bt, 0); /* 打印栈 */
            }
        } while_each_thread(g, p);
        rcu_read_unlock(); /* 为运行队列解锁 */
    }else if(load < loadmax && flag == 1)
    {
        flag=0;
    }
}

static enum hrtimer_restart monitor_handler(struct hrtimer *hrtimer) { /* 计时器到期后调用的程序 */
    enum hrtimer_restart ret = HRTIMER_RESTART;
    check_load();
    hrtimer_forward_now(&timer, ms_to_ktime(1000)); /* 延期1s后到期 */
    return ret;
}

static void start_timer(void) {			/*在系统中添加一个定时器，该定时器每1秒执行一次*/
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_PINNED); 
    timer.function = monitor_handler; /* 设定计时器到期回调函数 */
    hrtimer_start_range_ns(&timer, ms_to_ktime(1000), 0, HRTIMER_MODE_REL_PINNED);
}

int bad_time_init(void) {
    StackTraceSaveTask = find_kallsyms_lookup_name("stack_trace_save_tsk");
    if(!StackTraceSaveTask)
        return -EINVAL;
    start_timer();		/* 启动计时器 */
    printk("bad_time loaded.\n");
    return 0;
}

void bad_time_exit(void) { 
    hrtimer_cancel(&timer);				 /* 取消计时器 */
    printk("bad_time unloaded.\n");
}

