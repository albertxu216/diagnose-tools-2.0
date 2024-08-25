/*
 * Linux内核诊断工具--用户态ping-delay功能实现
 *
 * Copyright (C) 2020 Alibaba Ltd.
 *
 * 作者: Baoyou Xie <baoyou.xie@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include <sys/time.h>
#include <string.h>
#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */

#include <iostream>
#include <unordered_set>

#include "internal.h"
#include "symbol.h"
#include "uapi/tcpstates.h"
#include "unwind.h"
#include "params_parse.h"
#include <arpa/inet.h>
using namespace std;

void usage_tcp_states(void)
{
    printf("----------------------------------\n");
	printf("    tcp_states usage:\n");
	printf("        --help tcp_states help info\n");
	printf("        --activate\n");
	printf("          verbose VERBOSE\n");
	printf("          addr filtered ipv4 address.\n");
	printf("        --deactivate\n");
	printf("        --settings dump settings\n");
	printf("        --report dump log with text.\n");
	//printf("        --log\n");
	//printf("          sls=/tmp/1.log store in file\n");
	//printf("          syslog=1 store in syslog\n");
}

static void do_activate(const char *arg)
{
	int ret = 0;
	//解析参数
	struct params_parser parse(arg);
	struct diag_tcp_states_settings settings;
	string str;
	char ipstr[255];

	memset(&settings, 0, sizeof(struct diag_tcp_states_settings));
	//根据参数给verbose赋值
	settings.verbose = parse.int_value("verbose");

	//根据参数给addr赋值
	str = parse.string_value("addr");
	if (str.length() > 0) {
		settings.addr = ipstr2int(str.c_str());
	}
	//传递信号
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TCP_STATES_SET, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TCP_STATES_SET, &ret, &settings, sizeof(struct diag_tcp_states_settings));
	}

	//输出信息
	printf("功能设置%s，返回值：%d\n", ret ? "失败" : "成功", ret);
	printf("    输出级别：%d\n", settings.verbose);
	printf("    过滤地址：%s\n", int2ipstr(settings.addr, ipstr, 255));

	if (ret)
		return;

	ret = diag_activate("tcpstates");
	if (ret == 1) {
		printf("tcp-states activated\n");
	} else {
		printf("tcp-states is not activated, ret %d\n", ret);
	}
}

static void do_deactivate(void)
{
	int ret = 0;

	//向controller写入deactivate 
	ret = diag_deactivate("tcpstates");
	if (ret == 0) {
		printf("tcpstates is not activated\n");
	} else {
		printf("deactivate tcpstates fail, ret is %d\n", ret);
	}
}

static void print_settings_in_json(struct diag_tcp_states_settings *settings, int ret)
{
	Json::Value root;
	std::string str_log;

	if (ret == 0) {
		root["activated"] = Json::Value(settings->activated);
		root["verbose"] = Json::Value(settings->verbose);
	} else {
		root["err"] = Json::Value("found ping-delay settings failed, please check if diagnose-tools is installed correctly or not.");
	}

	str_log.append(root.toStyledString());
	printf("%s", str_log.c_str());

	return;
}

static void do_settings(const char *arg)
{
	struct diag_tcp_states_settings settings;
	int ret;
	char ipstr[255];
	int enable_json = 0;
	struct params_parser parse(arg);
	enable_json = parse.int_value("json");
	memset(&settings, 0, sizeof(struct diag_tcp_states_settings));
	//传递信号
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TCP_STATES_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TCP_STATES_SETTINGS, &ret, &settings, sizeof(struct diag_tcp_states_settings));
	}

	if (1 == enable_json) {
		return print_settings_in_json(&settings, ret);
	}

	if (ret == 0) {
		printf("功能设置：\n");
		printf("    是否激活：%s\n", settings.activated ? "√" : "×");
		printf("    输出级别：%d\n", settings.verbose);
		printf("    过滤地址：%s\n", int2ipstr(settings.addr, ipstr, 255));
		
	} else {
		printf("获取tcpstates设置失败，请确保正确安装了diagnose-tools工具\n");
	}
}
static int tcp_states_extract(void *buf, unsigned int len, void *)
{
	int *et_type;
	struct tcp_states_detail *detail;
	struct tcp_states_total *total;
	unsigned char *saddr;
	unsigned char *daddr;

	if (len == 0)
		return 0;

	et_type = (int *)buf;
	switch (*et_type) {
	case et_tcp_states_detail:
		if (len < sizeof(struct tcp_states_detail))
			break;
		detail = (struct tcp_states_detail *)buf;

		saddr = (unsigned char *)&detail->saddr;
		daddr = (unsigned char *)&detail->daddr;
		printf("[%lu:%lu],tcp-states跟踪,源IP: %lu.%lu.%lu.%lu,目的IP: %lu.%lu.%lu.%lu,源端口: %d,目的端口: %d,原先状态: %s, 新状态: %s, 原状态持续时间: %llu\n\
	|发送窗口大小: %d,接收窗口大小: %d,发送缓冲区大小: %d,已使用发送缓冲区大小: %d,接收缓冲区大小: %d\n\
	|发送字节数: %d,接收字节数: %d,确认字节数: %d,backlog: %d,max_backlog: %d,重传数: %d\n\n",
			detail->tv.tv_sec, detail->tv.tv_usec,
			(unsigned long)saddr[0], (unsigned long)saddr[1], (unsigned long)saddr[2], (unsigned long)saddr[3],
			(unsigned long)daddr[0], (unsigned long)daddr[1], (unsigned long)daddr[2], (unsigned long)daddr[3],
			detail->sport, ntohs(detail->dport),
			tcp_states[detail->oldstate],tcp_states[detail->newstate],detail->time,
			detail->snd_cwnd,detail->rcv_wnd,detail->sndbuf,detail->sk_wmem_queued,detail->rcvbuf,
			detail->bytes_sent,detail->bytes_received,detail->bytes_acked,
			detail->tcp_backlog,detail->max_tcp_backlog,detail->total_retrans);
		break;
	case et_tcp_states_detail_now:
		if (len < sizeof(struct tcp_states_detail))
			break;
		detail = (struct tcp_states_detail *)buf;

		saddr = (unsigned char *)&detail->saddr;
		daddr = (unsigned char *)&detail->daddr;
		printf("[%lu:%lu],tcp-states连接,源IP：%lu.%lu.%lu.%lu, 目的IP：%lu.%lu.%lu.%lu, 源端口：%d, 目的端口: %d, 状态: %s, 状态持续时间：%llu\n",
			detail->tv.tv_sec, detail->tv.tv_usec,
			(unsigned long)saddr[0], (unsigned long)saddr[1], (unsigned long)saddr[2], (unsigned long)saddr[3],
			(unsigned long)daddr[0], (unsigned long)daddr[1], (unsigned long)daddr[2], (unsigned long)daddr[3],
			detail->sport, ntohs(detail->dport),
			tcp_states[detail->newstate],
			detail->time);
		break;
	case et_tcp_states_total:
		if (len < sizeof(struct tcp_states_total))
			break;
		total = (struct tcp_states_total *)buf;
		printf("------------------------------------------states_num------------------------------------------\n");
		printf("---SYN_SENT:%d   SYN_RECV:%d   ESTABLISHED:%d   FIN_WAIT1:%d   FIN_WAIT2:%d   CLOSE_WAIT:%d---\n",
		total->state_count[TCP_STATE_SYN_SENT],total->state_count[TCP_STATE_SYN_RECV],
		total->state_count[TCP_STATE_ESTABLISHED],total->state_count[TCP_STATE_FIN_WAIT1],
		total->state_count[TCP_STATE_FIN_WAIT2],total->state_count[TCP_STATE_CLOSE_WAIT]);
		printf("----------------------------------------------------------------------------------------------\n");

		break;
	default:
		break;
	}
	return 0;
}

// static int sls_extract(void *buf, unsigned int len, void *)
// {
// 	int *et_type;
// 	struct tcp_states_detail *detail;
// 	unsigned char *saddr;
// 	unsigned char *daddr;
// 	int i;
// 	symbol sym;

// 	const char *func;
// 	Json::Value root;
// 	Json::Value raw;
// 	Json::Value msg;
// 	stringstream ss;

// 	if (len == 0)
// 		return 0;

// 	et_type = (int *)buf;
// 	switch (*et_type) {
// 	case et_tcp_states_detail:
// 		if (len < sizeof(struct tcp_states_detail))
// 			break;
// 		detail = (struct tcp_states_detail *)buf;

// 		saddr = (unsigned char *)&detail->saddr;
// 		daddr = (unsigned char *)&detail->daddr;
// 		diag_ip_addr_to_str(saddr, "src_addr", root);
// 		diag_ip_addr_to_str(daddr, "dest_addr", root);

// 		root["echo_id"] = Json::Value(detail->echo_id);
// 		root["echo_sequence"] = Json::Value(detail->echo_sequence);
// 		diag_sls_time(&detail->tv, root);

// 		if (detail->step < PD_TRACK_COUNT) {
// 			root["STEP"] = Json::Value(ping_delay_packet_steps_str[detail->step]);
// 		} else {
// 			root["STEP"] = Json::Value("?");
// 		}

// 		write_file(sls_file, "tcpstates-detail", &detail->tv, 0, 0, root);
// 		write_syslog(syslog_enabled, "tcpstates-detail", &detail->tv, 0, 0, root);
// 		break;
// 	default:
// 		break;
// 	}
// 	return 0;
// }

static void do_extract(char *buf, int len)
{
	extract_variant_buffer(buf, len, tcp_states_extract, NULL);
}

static void do_dump(void)
{
	static char variant_buf[1 * 1024 * 1024];
	int len;
	int ret = 0;
	struct diag_ioctl_dump_param dump_param = {
		.user_ptr_len = &len,
		.user_buf_len = 1 * 1024 * 1024,
		.user_buf = variant_buf,
	};

	memset(variant_buf, 0, 1 * 1024 * 1024);
	//传递信号
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TCP_STATES_DUMP, (long)&dump_param);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TCP_STATES_DUMP, &ret, &len, variant_buf, 1 * 1024 * 1024);
	}

	if (ret == 0) {
		do_extract(variant_buf, len);
	}
}

// static void do_sls(char *arg)
// {
// 	int ret;
// 	int len;
// 	static char variant_buf[1024 * 1024];
// 	struct diag_ioctl_dump_param dump_param = {
// 		.user_ptr_len = &len,
// 		.user_buf_len = 1024 * 1024,
// 		.user_buf = variant_buf,
// 	};

// 	ret = log_config(arg, sls_file, &syslog_enabled);
// 	if (ret != 1)
// 		return;

// 	while (1) {
// 		if (run_in_host) {
// 			ret = diag_call_ioctl(DIAG_IOCTL_TCP_STATES_DUMP, (long)&dump_param);
// 		} else {
// 			syscall(DIAG_TCP_STATES_DUMP, &ret, &len, variant_buf, 1024 * 1024);
// 		}

// 		if (ret == 0 && len > 0) {
// 			extract_variant_buffer(variant_buf, len, sls_extract, NULL);
// 		}

// 		sleep(10);
// 	}
// }

int tcp_states_main(int argc, char **argv)
{
	static struct option long_options[] = {
			{"help",     no_argument, 0,  0 },
			{"activate",     optional_argument, 0,  0 },
			{"deactivate", no_argument,       0,  0 },
			{"settings",     optional_argument, 0,  0 },
			{"report",     no_argument, 0,  0 },
			{"log",     required_argument, 0,  0 },
			{0,         0,                 0,  0 }
		};
	int c;

	if (argc <= 1) {
		 usage_tcp_states();
		 return 0;
	}
	while (1) {
		int option_index = -1;
		//解析参数
		c = getopt_long_only(argc, argv, "", long_options, &option_index);
		if (c == -1)
			break;
		switch (option_index) {
		case 0:
			usage_tcp_states();
			break;
	    case 1:
			do_activate(optarg ? optarg : "");
			break;
		case 2:
			do_deactivate();
			break;
		case 3:
			do_settings(optarg ? optarg : "");
			break;
		case 4:
			do_dump();
			break;
		// case 5:
		// 	do_sls(optarg);
		// 	break;
		default:
			usage_tcp_states();
			break;
		}
	}

	return 0;
}
