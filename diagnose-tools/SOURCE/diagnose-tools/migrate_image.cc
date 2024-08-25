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

#include <set>

#include "internal.h"
#include "symbol.h"
#include "json/json.h"
#include <iostream>
#include <fstream>

#include "uapi/migrate_image.h"
#include "params_parse.h"
#include <syslog.h>

using namespace std;
long long  interval;
//用于记录监测的总时间
void usage_migrate_image(void)
{
    printf("----------------------------------\n");
	printf("    migrate_image usage:\n");
	printf("        --help migrate_image help info\n");
	printf("        --activate\n");
	printf("          verbose VERBOSE\n");
	// printf("          threshold THRESHOLD(MS)\n");
	printf("          tgid process group monitored\n");
	printf("          pid thread id that monitored\n");
	printf("          comm comm that monitored\n");
	printf("        --deactivate\n");
	printf("        --report dump log with text.\n");
}

static void do_activate(const char *arg)
{
	int ret ;
	struct params_parser parse(arg);
	struct diag_migrate_image_settings settings;//控制结构体
	string str;
    /*初始化setting*/
	memset(&settings, 0, sizeof(struct diag_migrate_image_settings));
    /*根据参数赋值*/
	settings.verbose = parse.int_value("verbose");
	settings.tgid = parse.int_value("tgid");
	settings.pid = parse.int_value("pid");
	if(!settings.tgid&&!settings.pid){
		printf("activate 失败，请指定需要跟踪的进程或线程\n");
		printf("diagnose-tools migrate-image --activate='pid=<pid>\ndiagnose-tools migrate-image --activate='tgid=<tgid>'\n");
		return;
	}	
    /*获取进程名*/
	str = parse.string_value("comm");
	if (str.length() > 0) {
		strncpy(settings.comm, str.c_str(), TASK_COMM_LEN);
		settings.comm[TASK_COMM_LEN - 1] = 0;
	}

    /*传递信号,调用系统调用去设置settings*/
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_MIGRATE_IMAGE_SET, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_MIGRATE_IMAGE_SET, &ret, &settings, sizeof(struct diag_migrate_image_settings));
	}
	printf("功能设置%s，返回值：%d\n", ret ? "失败" : "成功", ret);
	printf("    进程ID：\t%d\n", settings.tgid);
	printf("    线程ID：\t%d\n", settings.pid);
	printf("    进程名称：\t%s\n", settings.comm);
	// printf("    监控阈值(ms)：\t%d\n", settings.threshold_ms);
	printf("    输出级别：\t%d\n", settings.verbose);
	if (ret)
		return;


    /*在内核中真正地激活
     *  调用misc.cc中的diag_activate函数，
     *  通过标准输入流，会调用向/proc/ali-linux/diagnose/controller
     *  写入migrate-image ，写成功会返回1，将1赋给ret
     */
	ret = diag_activate("migrate-image");
	if (ret == 1) {
		printf("migrate-image activated\n");
	} else {
		printf("migrate-image is not activated, ret %d\n", ret);
	}
}

static void do_deactivate(void)
{
	int ret = 0;

	//向controller写入deactivate 
	ret = diag_deactivate("migrate-image");//通过/proc/controller失活函数
	if (ret == 0) {
		printf("migrate-image is not activated\n");
	} else {
		printf("deactivate migrate-image fail, ret is %d\n", ret);
	}
}

static void print_settings_in_json(struct diag_migrate_image_settings *settings, int ret)
{
	Json::Value root;
	std::string str_log;

	if (ret == 0) {
		root["activated"] = Json::Value(settings->activated);
		root["tgid"] = Json::Value(settings->tgid);
		root["pid"] = Json::Value(settings->pid);
		root["comm"] = Json::Value(settings->comm);
		root["verbose"] = Json::Value(settings->verbose);
	} else {
		root["err"] = Json::Value("found migrate-image settings failed, please check if diagnose-tools is installed correctly or not.");
	}

	str_log.append(root.toStyledString());
	printf("%s", str_log.c_str());

	return;
}


static void do_settings(const char *arg)
{
	struct diag_migrate_image_settings settings;
	int ret;
	int enable_json = 0;
	struct params_parser parse(arg);
	enable_json = parse.int_value("json");

	memset(&settings, 0, sizeof(struct diag_migrate_image_settings));

	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_MIGRATE_IMAGE_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_MIGRATE_IMAGE_SETTINGS, &ret, &settings, sizeof(struct diag_migrate_image_settings));
	}

	if (1 == enable_json) {
		return print_settings_in_json(&settings, ret);
	}

	if (ret == 0) {
		printf("功能设置：\n");
		printf("    是否激活：%s\n", settings.activated ? "√" : "×");
		printf("    进程ID：%d\n", settings.tgid);
		printf("    线程ID：%d\n", settings.pid);
		printf("    进程名称：%s\n", settings.comm);	
		printf("    输出级别：%d\n", settings.verbose);
	} else {
		printf("获取migrate-image设置失败，请确保正确安装了diagnose-tools工具\n");
	}
}

static void data_processing(struct migrate_image_detail *detail){
	printf("PID: %d \t Time :[%lu:%lu]\n",
			detail->pid,
			detail->tv.tv_sec,detail->tv.tv_usec);
	printf("PRIO: %d \t COUNT: %d \t REAR : %d\n",detail->prio,detail->count,detail->rear);
	printf("%-16s %-10s %-10s %-12s %-12s %-12s %-12s %-15s\n",
			"Time_Stamp", "ocpu", "dcpu", "PLoad_AVG", "Putil_AVG", "MEM_USAGE", "READ", "WRITE", "Context_Switch");
	for(int i=detail->rear;i<=detail->count;i++){
		struct per_migrate *per_migrate = detail->migrate_info + (i%MAX_MIGRATE);
		printf("%-16lu %-10d %-10d %-12lu %-12lu %-12lu %-12lu %-15lu\n",
				per_migrate->time,
				per_migrate->orig_cpu,
				per_migrate->dest_cpu,
				per_migrate->pload_avg,
				per_migrate->putil_avg,
				per_migrate->mem_usage/1024,
				per_migrate->read_bytes/1024,
				per_migrate->write_bytes/1024,
        		per_migrate->context_switches);
	}
	printf("\n\n");
}

static int migrate_image_extract(void *buf, unsigned int len, void *)
{
	int *et_type;
	struct migrate_image_detail *detail;
	if (len == 0)
		return 0;
	et_type = (int *)buf;

	switch (*et_type) {
	case et_migrate_image_detail:
		if (len < sizeof(struct migrate_image_detail))
			break;
		detail = (struct migrate_image_detail *)buf;
		data_processing(detail);
		break;
	default:
		break;
	}
	return 0;
}
/*do_extract调用extract_variant_buffer去执行相应的打印函数*/
static void do_extract(char *buf, int len)
{
	extract_variant_buffer(buf, len, migrate_image_extract, NULL);//extract_variant_buffer被定义在/SOURCE/uapi/ali_diagnose.h中
}

/*通过ioctl获取缓冲区buf的数据,并调用do_extract输出*/
static void do_dump(void)
{
	static char variant_buf[1 * 1024 * 1024];//缓冲区
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
		ret = diag_call_ioctl(DIAG_IOCTL_MIGRATE_IMAGE_DUMP, (long)&dump_param);//将内核中的数据拉到用户态
	} else {
		ret = -ENOSYS;
		syscall(DIAG_MIGRATE_IMAGE_DUMP, &ret, &len, variant_buf, 1 * 1024 * 1024);
	}
	if (ret == 0) {
		printf("-----------------MIGRATE_IMAGE---------------------\n");
		do_extract(variant_buf, len);
	}
}

int migrate_image_main(int argc, char **argv){
	static struct option long_options[] = {
			{"help",     no_argument, 0,  0 },
			{"activate",     optional_argument, 0,  0 },
			{"deactivate", no_argument,       0,  0 },
			{"settings",     optional_argument, 0,  0 },
			{"report",     optional_argument, 0,  0 },
			{"log",     required_argument, 0,  0 },
			{0,         0,                 0,  0 }
		};
	int c;

	if (argc <= 1) {
		usage_migrate_image();
		return 0;
	}
	while (1) {
		int option_index = -1;

		c = getopt_long_only(argc, argv, "", long_options, &option_index);//解析参数
		if (c == -1)
			break;
		switch (option_index) {
		case 0:
			usage_migrate_image();
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
			usage_migrate_image();
			break;
		}
	}

	return 0;    
}





