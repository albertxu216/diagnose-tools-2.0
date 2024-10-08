/*
 * Linux内核诊断工具--内核态网络功能入口
 *
 * Copyright (C) 2020 Alibaba Ltd.
 *
 * 作者: Baoyou Xie <baoyou.xie@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */

#include <linux/module.h>
#include <linux/stacktrace.h>
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
#include <linux/version.h>
#include <linux/net.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/icmp.h>
#include <linux/netfilter.h>

#include "internal.h"
#include "net_internal.h"

int diag_net_init(void)
{
	int ret;
	struct proc_dir_entry *pe;

	pe = diag_proc_mkdir("ali-linux/diagnose/net", NULL);

	ret = diag_tcp_retrans_init();
	if (ret)
		goto out_tcp_retrans;

	ret = diag_net_drop_packet_init();
	if (ret)
		goto out_drop;

	ret = diag_net_reqsk_init();
	if (ret)
		goto out_reqsk;

	ret = diag_net_packet_corruption_init();
	if (ret)
		goto out_corruption;

	ret = diag_net_redis_ixgbe_init();
	if (ret)
		goto out_redis_ixgbe;

	ret = diag_net_ping_delay_init();
	if (ret)
		goto out_ping_delay;

	ret = diag_net_ping_delay6_init();
	if (ret)
		goto out_ping_delay6;

	ret = diag_net_net_bandwidth_init();
	if (ret)
		goto out_net_bandwidth;

	ret = diag_net_tcp_connect_init();
	if (ret)
		goto out_tcp_connect;

	ret = diag_net_tcp_states_init();
	if (ret)
		goto out_tcp_states;

	ret = diag_net_udp_init();
	if (ret)
		goto out_udp;
	return 0;

out_tcp_connect:
	diag_net_net_bandwidth_exit();
out_net_bandwidth:
	diag_net_ping_delay6_exit();
out_ping_delay6:
	diag_net_ping_delay_exit();
out_ping_delay:
	diag_net_redis_ixgbe_exit();
out_redis_ixgbe:
	diag_net_packet_corruption_exit();
out_corruption:
	diag_net_reqsk_exit();
out_reqsk:
	diag_net_drop_packet_exit();
out_drop:
	diag_tcp_retrans_exit();
out_tcp_states:
	diag_net_tcp_states_exit();
out_udp:
	diag_net_udp_exit();
out_tcp_retrans:
	return ret;
}

void diag_net_exit(void)
{
	diag_net_ping_delay6_exit();
	diag_net_ping_delay_exit();
	diag_tcp_retrans_exit();
	diag_net_drop_packet_exit();
	diag_net_reqsk_exit();
	diag_net_packet_corruption_exit();
	diag_net_redis_ixgbe_exit();
	diag_net_net_bandwidth_exit();
	diag_net_tcp_connect_exit();
	diag_net_tcp_states_exit();
	diag_net_udp_exit();
	//remove_proc_entry("ali-linux/diagnose/net", NULL);
}
