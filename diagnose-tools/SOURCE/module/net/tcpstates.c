/*
 * Linux内核诊断工具--内核态tcp-states功能
 *
 * Copyright (C) 2022 Alibaba Ltd.
 *
 * 作者: Yang Wei <albin.yangwei@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/protocol.h>
#include <linux/slab.h>
#include <linux/jhash.h>
#include <linux/spinlock.h>

#include "uapi/ali_diagnose.h"
#include "uapi/tcpstates.h"
#include "pub/variant_buffer.h"
#include "pub/trace_file.h"
#include "pub/trace_point.h"
#include "internal.h"

#define ADDR_PAIR_HASH_BITS 20
static atomic64_t diag_nr_running = ATOMIC64_INIT(0);
static struct diag_variant_buffer tcp_states_variant_buffer;
static struct diag_tcp_states_settings tcp_states_settings;
static unsigned int tcp_states_alloced;

__maybe_unused static atomic64_t diag_alloc_count = ATOMIC64_INIT(0);

__maybe_unused static struct rb_root diag_tcpstates_tree = RB_ROOT;
__maybe_unused static DEFINE_SPINLOCK(diag_tcpstates_tree_lock);

struct diag_tcpstates {
	struct rb_node rb_node;
	struct list_head list;
	struct sock *sk;
	int saddr;
	int daddr;
	int sport;
	int dport;
	__u64 time;
	int oldstate;
    int newstate;
};
__maybe_unused static void clean_data(void)
{
	unsigned long flags;
	struct list_head header;
	struct rb_node *node;

	INIT_LIST_HEAD(&header);
	spin_lock_irqsave(&diag_tcpstates_tree_lock, flags);

	for (node = rb_first(&diag_tcpstates_tree); node; node = rb_next(node)) {
		struct diag_tcpstates *this = container_of(node,
				struct diag_tcpstates, rb_node);

		rb_erase(&this->rb_node, &diag_tcpstates_tree);
		INIT_LIST_HEAD(&this->list);
		list_add_tail(&this->list, &header);
	}
	diag_tcpstates_tree = RB_ROOT;

	spin_unlock_irqrestore(&diag_tcpstates_tree_lock, flags);

	while (!list_empty(&header)) {
		struct diag_tcpstates *this = list_first_entry(&header, struct diag_tcpstates, list);

		list_del_init(&this->list);
		kfree(this);
	}
}
__maybe_unused static int compare_desc(struct diag_tcpstates *desc, struct diag_tcpstates *this)
{
	if (desc->sk < this->sk)
		return -1;
	if (desc->sk > this->sk)
		return 1;
	return 0;
}
__maybe_unused static struct diag_tcpstates *__find_alloc_desc(struct diag_tcpstates *desc)
{
	struct diag_tcpstates *this;
	struct rb_node **node, *parent;
	int compare_ret;
	
	node = &diag_tcpstates_tree.rb_node;
	parent = NULL;

	while (*node != NULL)
	{
		parent = *node;
		this = container_of(parent, struct diag_tcpstates, rb_node);
		compare_ret = compare_desc(desc, this);

		if (compare_ret < 0)
			node = &parent->rb_left;
		else if (compare_ret > 0)
			node = &parent->rb_right;
		else
		{
			printk("find the node %d",this->dport);
			return this;
		}
	}
	
	return 0;
}
__maybe_unused static struct diag_tcpstates *__add_alloc_desc(struct diag_tcpstates *desc)
{
	struct diag_tcpstates *this;
	struct rb_node **node, *parent;
	int compare_ret;
	printk("add the node");
	node = &diag_tcpstates_tree.rb_node;
	parent = NULL;

	while (*node != NULL)
	{
		parent = *node;
		this = container_of(parent, struct diag_tcpstates, rb_node);
		compare_ret = compare_desc(desc, this);

		if (compare_ret < 0)
			node = &parent->rb_left;
		else if (compare_ret > 0)
			node = &parent->rb_right;
		else
		{
			return this;
		}
	}
	this = kmalloc(sizeof(struct diag_tcpstates), GFP_ATOMIC);
	if (!this) {
		atomic64_inc_return(&diag_alloc_count);
		return this;
	}
	//找不到新加
	memset(this, 0, sizeof(struct diag_tcpstates));
	this->sk = desc->sk;
	this->saddr = desc->saddr;
	this->daddr = desc->daddr;
	this->sport = desc->sport;
	this->dport = desc->dport;
	this->time = desc->time;
	this->oldstate = desc->oldstate;
	this->newstate = desc->newstate;
	rb_link_node(&this->rb_node, parent, node);
	rb_insert_color(&this->rb_node, &diag_tcpstates_tree);

	return this;
}
static void inet_sock_set_state_hit(void *ignore, struct sock *sk,  int oldstate,  int newstate)
{
	unsigned long flags;
	struct tcp_states_detail event; //传递结构
	struct diag_tcpstates *desc;  //红黑树结构
	struct diag_tcpstates tmp; 
	if (sk->sk_protocol != IPPROTO_TCP)
        return ;
	//传递结构初始化
	event.et_type = et_tcp_states_detail;
	event.saddr=sk->__sk_common.skc_rcv_saddr;
	event.daddr=sk->__sk_common.skc_daddr;
	event.sport=sk->__sk_common.skc_num;
	event.dport=sk->__sk_common.skc_dport;
	event.oldstate = oldstate;
	event.newstate = newstate;
	event.time=0;
	do_diag_gettimeofday(&event.tv);

	struct tcp_sock *tp = (struct tcp_sock *)sk;                                                   
    event.bytes_acked =  tp-> bytes_acked;                
    event.bytes_sent =  tp-> bytes_sent;                
    event.bytes_received =  tp-> bytes_received;
    event.snd_cwnd =  tp-> snd_cwnd;                      
    event.rcv_wnd =  tp-> rcv_wnd;                                     
    event.total_retrans =  tp-> total_retrans;      
    event.sndbuf =  sk-> sk_sndbuf;
	event.rcvbuf=sk->sk_rcvbuf;                       
    event.sk_wmem_queued =  sk-> sk_wmem_queued;          
    event.tcp_backlog =  sk-> sk_ack_backlog;             
    event.max_tcp_backlog =  sk-> sk_max_ack_backlog;

	tmp.saddr=sk->__sk_common.skc_rcv_saddr;
	tmp.daddr=sk->__sk_common.skc_daddr;
	tmp.sport=sk->__sk_common.skc_num;
	tmp.dport=sk->__sk_common.skc_dport;
	tmp.oldstate=oldstate;
	tmp.newstate=newstate;
	tmp.sk=sk;

	//寻找/新建  发/收
	if(oldstate==TCP_STATE_CLOSE||oldstate==TCP_STATE_LISTEN)
	{
		tmp.time=event.tv.tv_sec*1000000+event.tv.tv_usec;
		spin_lock_irqsave(&diag_tcpstates_tree_lock, flags);
		desc=__add_alloc_desc(&tmp);
		spin_unlock_irqrestore(&diag_tcpstates_tree_lock, flags);
		event.time=0;
	}
	else if(newstate==TCP_STATE_CLOSE)
	{
		spin_lock_irqsave(&diag_tcpstates_tree_lock, flags);
		desc=__find_alloc_desc(&tmp);
		if(!desc)
			goto out1;
		printk("delete the node");
		rb_erase(&desc->rb_node, &diag_tcpstates_tree);
out1:
		spin_unlock_irqrestore(&diag_tcpstates_tree_lock, flags);
		if(desc){
			event.time=event.tv.tv_sec*1000000+event.tv.tv_usec-desc->time;
			kfree(desc);
		}
		else
			event.time=0;
	}
	else{
		spin_lock_irqsave(&diag_tcpstates_tree_lock, flags);
		desc=__find_alloc_desc(&tmp);
		if(!desc)
			return;
		spin_unlock_irqrestore(&diag_tcpstates_tree_lock, flags);
		event.time=event.tv.tv_sec*1000000+event.tv.tv_usec-desc->time;
		desc->time=event.tv.tv_sec*1000000+event.tv.tv_usec;
		desc->saddr=sk->__sk_common.skc_rcv_saddr;
		desc->sport=sk->__sk_common.skc_num;
		desc->daddr=sk->__sk_common.skc_daddr;
		desc->dport=sk->__sk_common.skc_dport;
		desc->oldstate=oldstate;
		desc->newstate=newstate;
	}
	
	if(tcp_states_settings.verbose==0)
	{
		if(tcp_states_settings.addr&&tcp_states_settings.addr!=ntohl(event.daddr))
			return;
		diag_variant_buffer_spin_lock(&tcp_states_variant_buffer, flags);
		diag_variant_buffer_reserve(&tcp_states_variant_buffer, sizeof(struct tcp_states_detail));
		diag_variant_buffer_write_nolock(&tcp_states_variant_buffer, &event, sizeof(struct tcp_states_detail));
		diag_variant_buffer_seal(&tcp_states_variant_buffer);
		diag_variant_buffer_spin_unlock(&tcp_states_variant_buffer, flags);
	}


}
static void lookup(void)
{
	unsigned long flags;
	struct rb_node *node;
	struct list_head header;
	struct tcp_states_detail event;

	struct tcp_states_total total = {
    	.et_type = et_tcp_states_total,
    	.state_count = {0}, // 将数组 state_count 初始化为 0
	};
	INIT_LIST_HEAD(&header);
	spin_lock_irqsave(&diag_tcpstates_tree_lock, flags);
	for (node = rb_first(&diag_tcpstates_tree); node; node = rb_next(node)) {
		struct diag_tcpstates *this = container_of(node,
				struct diag_tcpstates, rb_node);
		INIT_LIST_HEAD(&this->list);
		list_add_tail(&this->list, &header);
	}
	spin_unlock_irqrestore(&diag_tcpstates_tree_lock, flags);
	while (!list_empty(&header)) {
		printk("find the node");
		struct diag_tcpstates *this = list_first_entry(&header, struct diag_tcpstates, list);
		if(!this) continue;
		event.et_type = et_tcp_states_detail_now;
		event.daddr=this->daddr;
		event.saddr=this->saddr;
		event.dport=this->dport;
		event.sport=this->sport;
		event.oldstate=this->oldstate;
		event.newstate=this->newstate;
		total.state_count[event.newstate]++;
		do_diag_gettimeofday(&event.tv);
		event.time=event.tv.tv_sec*1000000+event.tv.tv_usec-this->time;
		diag_variant_buffer_spin_lock(&tcp_states_variant_buffer, flags);
		diag_variant_buffer_reserve(&tcp_states_variant_buffer, sizeof(struct tcp_states_detail));
		diag_variant_buffer_write_nolock(&tcp_states_variant_buffer, &event, sizeof(struct tcp_states_detail));
		diag_variant_buffer_seal(&tcp_states_variant_buffer);
		diag_variant_buffer_spin_unlock(&tcp_states_variant_buffer, flags);
		list_del_init(&this->list);
	}

	diag_variant_buffer_spin_lock(&tcp_states_variant_buffer, flags);
	diag_variant_buffer_reserve(&tcp_states_variant_buffer, sizeof(struct tcp_states_total));
	diag_variant_buffer_write_nolock(&tcp_states_variant_buffer, &total, sizeof(struct tcp_states_total));
	diag_variant_buffer_seal(&tcp_states_variant_buffer);
	diag_variant_buffer_spin_unlock(&tcp_states_variant_buffer, flags);
	return;
}
static int __activate_tcp_states(void)
{
    
	int ret = 0;

	ret = alloc_diag_variant_buffer(&tcp_states_variant_buffer);
	if (ret)
		goto out_variant_buffer;

	tcp_states_alloced = 1;

	hook_tracepoint("inet_sock_set_state", inet_sock_set_state_hit, NULL);

	clean_data();
	return 1;

out_variant_buffer:
	return 0;
}

static void __deactivate_tcp_states(void)
{
	unhook_tracepoint("inet_sock_set_state", inet_sock_set_state_hit, NULL);

	synchronize_sched();
	msleep(20);
	while (atomic64_read(&diag_nr_running) > 0)
		msleep(20);
	
	clean_data();
}
int activate_tcp_states(void)
{
	if (!tcp_states_settings.activated)
		tcp_states_settings.activated = __activate_tcp_states();

	return tcp_states_settings.activated;
}

int deactivate_tcp_states(void)
{
	if (tcp_states_settings.activated)
		__deactivate_tcp_states();

	tcp_states_settings.activated = 0;
	return 0;
}

int tcp_states_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_tcp_states_settings settings;

	switch (id) {
	case DIAG_TCP_STATES_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_tcp_states_settings)) {
			ret = -EINVAL;
		} else if (tcp_states_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				tcp_states_settings = settings;
			}
		}
		break;
	case DIAG_TCP_STATES_SETTINGS:
		// user_buf = (void __user *)SYSCALL_PARAM1(regs);
		// user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		// if (user_buf_len != sizeof(struct diag_tcp_states_settings)) {
		// 	ret = -EINVAL;
		// } else {
		// 	settings.activated = tcp_states_settings.activated;
		// 	settings.verbose = tcp_states_settings.verbose;
		// 	ret = copy_to_user(user_buf, &settings, user_buf_len);
		// }
		// break;
	case DIAG_TCP_STATES_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		if (!tcp_states_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&tcp_states_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("tcp-states");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

long diag_ioctl_tcp_states(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_tcp_states_settings settings;
	struct diag_ioctl_dump_param dump_param;

	switch (cmd) {
	case CMD_TCP_STATES_SET:
		if (tcp_states_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_tcp_states_settings));
			if (!ret) {
				tcp_states_settings = settings;
			}
		}
		break;
	case CMD_TCP_STATES_SETTINGS:
		settings = tcp_states_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_tcp_states_settings));
		break;
	case CMD_TCP_STATES_DUMP:
		printk("DUMP");
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!tcp_states_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			if(tcp_states_settings.verbose==1)
			{
				lookup();
			}
			ret = copy_to_user_variant_buffer(&tcp_states_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("tcp-states");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

int diag_net_tcp_states_init(void)
{
	init_diag_variant_buffer(&tcp_states_variant_buffer, 2 * 1024 * 1024);

	if (tcp_states_settings.activated)
		activate_tcp_states();

	return 0;
}


void diag_net_tcp_states_exit(void)
{
	if (tcp_states_settings.activated)
		deactivate_tcp_states();
	tcp_states_settings.activated = 0;

	destroy_diag_variant_buffer(&tcp_states_variant_buffer);
	return;
}


