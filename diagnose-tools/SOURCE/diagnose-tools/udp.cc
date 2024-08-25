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
#include "uapi/udp.h"
#include "unwind.h"
#include "params_parse.h"
#include <arpa/inet.h>
using namespace std;

void usage_udp(void)
{
    printf("----------------------------------\n");
	printf("    udp usage:\n");
	printf("        --help udp help info\n");
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
	struct diag_udp_settings settings;
	string str;
	char ipstr[255];

	memset(&settings, 0, sizeof(struct diag_udp_settings));
	//根据参数给verbose赋值
	settings.verbose = parse.int_value("verbose");

	//根据参数给addr赋值
	str = parse.string_value("addr");
	if (str.length() > 0) {
		settings.addr = ipstr2int(str.c_str());
	}
	//传递信号
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_UDP_SET, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_UDP_SET, &ret, &settings, sizeof(struct diag_udp_settings));
	}

	//输出信息
	printf("功能设置%s，返回值：%d\n", ret ? "失败" : "成功", ret);
	printf("    输出级别：%d\n", settings.verbose);
	printf("    过滤地址：%s\n", int2ipstr(settings.addr, ipstr, 255));

	if (ret)
		return;

	ret = diag_activate("udp");
	if (ret == 1) {
		printf("udp activated\n");
	} else {
		printf("udp is not activated, ret %d\n", ret);
	}
}

static void do_deactivate(void)
{
	int ret = 0;

	//向controller写入deactivate 
	ret = diag_deactivate("udp");//通过/proc/controller 失活函数
	if (ret == 0) {
		printf("udp is not activated\n");
	} else {
		printf("deactivate udp fail, ret is %d\n", ret);
	}
}
static void print_settings_in_json(struct diag_udp_settings *settings, int ret)
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
	struct diag_udp_settings settings;
	int ret;
	int enable_json = 0;
	struct params_parser parse(arg);
	enable_json = parse.int_value("json");

	memset(&settings, 0, sizeof(struct diag_udp_settings));
	//传递信号
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_UDP_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_UDP_SETTINGS, &ret, &settings, sizeof(struct diag_udp_settings));
	}

	if (1 == enable_json) {
		return print_settings_in_json(&settings, ret);
	}

	if (ret == 0) {
		printf("功能设置：\n");
		printf("    是否激活：%s\n", settings.activated ? "√" : "×");
		printf("    输出级别：%d\n", settings.verbose);
		
	} else {
		printf("获取udp设置失败，请确保正确安装了diagnose-tools工具\n");
	}
}
static int udp_extract(void *buf, unsigned int len, void *)
{
	int *et_type;
	struct udp_detail *detail={};
	struct udp_dns_rcv *detail_rcv={};
	struct udp_dns_send *detail_send={};
	unsigned char *saddr;
	unsigned char *daddr;
	unsigned char *ans_addr;
	symbol sym;

	if (len == 0)
		return 0;

	et_type = (int *)buf;
	switch (*et_type) {
	case et_udp_detail:
		if (len < sizeof(struct udp_detail))
			break;
		detail = (struct udp_detail *)buf;
		if (detail->daddr == 0||detail->saddr == 0||(detail->daddr & 0x0000FFFF) == 0x0000007F ||(detail->saddr & 0x0000FFFF) == 0x0000007F)
        	return 0;
		saddr = (unsigned char *)&detail->saddr;
		daddr = (unsigned char *)&detail->daddr;

		printf("udp跟踪,%s:[%lu:%lu],源IP：%lu.%lu.%lu.%lu, 目的IP：%lu.%lu.%lu.%lu, 源端口：%d, 目的端口: %d,协议: %s \n",
			udp_packet_steps_str[detail->step],
            detail->tv.tv_sec, detail->tv.tv_usec,
			(unsigned long)saddr[0], (unsigned long)saddr[1], (unsigned long)saddr[2], (unsigned long)saddr[3],
			(unsigned long)daddr[0], (unsigned long)daddr[1], (unsigned long)daddr[2], (unsigned long)daddr[3],
			detail->sport, detail->dport,udp_prot_str[detail->prot]);

		break;
	case et_udp_dns_rcv:
		if (len < sizeof(struct udp_dns_rcv))
			break;
		detail_rcv = (struct udp_dns_rcv *)buf;
		if (detail_rcv->comm.daddr == 0||detail_rcv->comm.saddr == 0||(detail_rcv->comm.daddr & 0x0000FFFF) == 0x0000007F ||(detail_rcv->comm.saddr & 0x0000FFFF) == 0x0000007F)
        	return 0;
		saddr = (unsigned char *)&detail_rcv->comm.saddr;
		daddr = (unsigned char *)&detail_rcv->comm.daddr;

		ans_addr = (unsigned char *)&detail_rcv->answer_addr;
		printf("udp跟踪,%s:[%lu:%lu],源IP：%lu.%lu.%lu.%lu, 目的IP：%lu.%lu.%lu.%lu, 源端口：%d, 目的端口: %d,协议: %s ,询问数量: %d ,回答数量: %d,域名: %s,IP:%lu.%lu.%lu.%lu\n",
			udp_packet_steps_str[detail_rcv->comm.step],
            detail_rcv->comm.tv.tv_sec, detail_rcv->comm.tv.tv_usec,
			(unsigned long)saddr[0], (unsigned long)saddr[1], (unsigned long)saddr[2], (unsigned long)saddr[3],
			(unsigned long)daddr[0], (unsigned long)daddr[1], (unsigned long)daddr[2], (unsigned long)daddr[3],
			detail_rcv->comm.sport, ntohs(detail_rcv->comm.dport),udp_prot_str[detail_rcv->comm.prot],
			detail_rcv->qdcount,detail_rcv->ancount,detail_rcv->question,
			(unsigned long)ans_addr[3], (unsigned long)ans_addr[2], (unsigned long)ans_addr[1], (unsigned long)ans_addr[0]
			);
		break;
	case et_udp_dns_send:
		if (len < sizeof(struct udp_dns_send))
			break;
		detail_send = (struct udp_dns_send *)buf;
		if (detail_send->comm.daddr == 0||detail_send->comm.saddr == 0||(detail_send->comm.daddr & 0x0000FFFF) == 0x0000007F ||(detail_send->comm.saddr & 0x0000FFFF) == 0x0000007F)
        	return 0;
		saddr = (unsigned char *)&detail_send->comm.saddr;
		daddr = (unsigned char *)&detail_send->comm.daddr;

		printf("udp跟踪,%s:[%lu:%lu],源IP：%lu.%lu.%lu.%lu, 目的IP：%lu.%lu.%lu.%lu, 源端口：%d, 目的端口: %d,协议: %s ,询问数量: %d ,域名: %s\n",
			udp_packet_steps_str[detail_send->comm.step],
            detail_send->comm.tv.tv_sec, detail_send->comm.tv.tv_usec,
			(unsigned long)saddr[0], (unsigned long)saddr[1], (unsigned long)saddr[2], (unsigned long)saddr[3],
			(unsigned long)daddr[0], (unsigned long)daddr[1], (unsigned long)daddr[2], (unsigned long)daddr[3],
			detail_send->comm.sport, detail_send->comm.dport,udp_prot_str[detail_send->comm.prot],
			detail_send->qdcount,detail_send->question);

	// 	break;
		break;
	default:
		break;
	}
	return 0;
}

// static int sls_extract(void *buf, unsigned int len, void *)
// {
// 	int *et_type;
// 	struct udp_detail *detail;
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
// 	case et_udp_detail:
// 		if (len < sizeof(struct udp_detail))
// 			break;
// 		detail = (struct udp_detail *)buf;

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
	extract_variant_buffer(buf, len, udp_extract, NULL);
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
		ret = diag_call_ioctl(DIAG_IOCTL_UDP_DUMP, (long)&dump_param);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_UDP_DUMP, &ret, &len, variant_buf, 1 * 1024 * 1024);
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
// 			ret = diag_call_ioctl(DIAG_IOCTL_UDP_DUMP, (long)&dump_param);
// 		} else {
// 			syscall(DIAG_UDP_DUMP, &ret, &len, variant_buf, 1024 * 1024);
// 		}

// 		if (ret == 0 && len > 0) {
// 			extract_variant_buffer(variant_buf, len, sls_extract, NULL);
// 		}

// 		sleep(10);
// 	}
// }

int udp_main(int argc, char **argv)
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
		 usage_udp();
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
			usage_udp();
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
			usage_udp();
			break;
		}
	}

	return 0;
}
