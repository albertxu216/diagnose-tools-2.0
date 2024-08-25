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
#include <unordered_map>
#include "uapi/keytime_image.h"
#include "params_parse.h"
#include <syslog.h>

using namespace std;
long long  interval;
//用于存储按时间排序的事件信息
using TimeSortedEvents = std::map<int, event_info>;
// 存储每个进程的事件信息
std::unordered_map<int, TimeSortedEvents> processEvents;
//用于记录监测的总时间
void usage_keytime_image(void)
{
    printf("----------------------------------\n");
	printf("    keytime_image usage:\n");
	printf("        --help keytime_image help info\n");
	printf("        --activate\n");
	printf("          verbose VERBOSE\n");
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
	struct diag_keytime_image_settings settings;//控制结构体
	string str;
    /*初始化setting*/
	memset(&settings, 0, sizeof(struct diag_keytime_image_settings));
    /*根据参数赋值*/
	settings.verbose = parse.int_value("verbose");
	settings.tgid = parse.int_value("tgid");
	settings.pid = parse.int_value("pid");
	if(!settings.tgid&&!settings.pid){
		printf("activate 失败，请指定需要跟踪的进程或线程\n");
		printf("diagnose-tools keytime-image --activate='pid=<pid>\ndiagnose-tools keytime-image --activate='tgid=<tgid>'\n");
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
		ret = diag_call_ioctl(DIAG_IOCTL_KEYTIME_IMAGE_SET, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_KEYTIME_IMAGE_SET, &ret, &settings, sizeof(struct diag_keytime_image_settings));
	}
	printf("功能设置%s，返回值：%d\n", ret ? "失败" : "成功", ret);
	printf("    进程ID：\t%d\n", settings.tgid);
	printf("    线程ID：\t%d\n", settings.pid);
	// printf("    进程名称：\t%s\n", settings.comm);
	printf("    输出级别：\t%d\n", settings.verbose);
	if (ret)
		return;


    /*在内核中真正地激活
     *  调用misc.cc中的diag_activate函数，
     *  通过标准输入流，会调用向/proc/ali-linux/diagnose/controller
     *  写入keytime-image ，写成功会返回1，将1赋给ret
     */
	ret = diag_activate("keytime-image");
	if (ret == 1) {
		printf("keytime-image activated\n");
	} else {
		printf("keytime-image is not activated, ret %d\n", ret);
	}
}

static void do_deactivate(void)
{
	int ret = 0;

	//向controller写入deactivate 
	ret = diag_deactivate("keytime-image");//通过/proc/controller失活函数
	if (ret == 0) {
		printf("keytime-image is not activated\n");
	} else {
		printf("deactivate keytime-image fail, ret is %d\n", ret);
	}
}

static void print_settings_in_json(struct diag_keytime_image_settings *settings, int ret)
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
		root["err"] = Json::Value("found keytime-image settings failed, please check if diagnose-tools is installed correctly or not.");
	}

	str_log.append(root.toStyledString());
	printf("%s", str_log.c_str());

	return;
}


static void do_settings(const char *arg)
{
	struct diag_keytime_image_settings settings;
	int ret;
	int enable_json = 0;
	struct params_parser parse(arg);
	enable_json = parse.int_value("json");

	memset(&settings, 0, sizeof(struct diag_keytime_image_settings));

	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_KEYTIME_IMAGE_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_KEYTIME_IMAGE_SETTINGS, &ret, &settings, sizeof(struct diag_keytime_image_settings));
	}

	if (1 == enable_json) {
		return print_settings_in_json(&settings, ret);
	}

	if (ret == 0) {
		printf("功能设置：\n");
		printf("    是否激活：%s\n", settings.activated ? "√" : "×");
		printf("    进程ID：\t%d\n", settings.tgid);
		printf("    线程ID：\t%d\n", settings.pid);
		printf("    输出级别：%d\n", settings.verbose);
	} else {
		printf("获取keytime-image设置失败，请确保正确安装了diagnose-tools工具\n");
	}
}
void addEvent(struct events * detail) {
	struct head *head;
	head = &detail->head;
	struct event_info e;
	e.type=head->type;
	e.tgid = head->tgid;
	strncpy(e.comm, head->comm, TASK_COMM_LEN);
	e.comm[TASK_COMM_LEN - 1] = 0;
	e.evnt=&detail->evnt;

    processEvents[head->pid][head->time] = e;
}
static void data_processing(struct events * detail){

    // static int seq;
	// diag_printf_time(&detail->tv);
	// seq++;
	// printf("##CGROUP:[%s]  %d      [%03d]  采样命中\n",
	// 		detail->task.cgroup_buf,
	// 		detail->task.pid,
	// 		seq);
    // printf("    %-18s %-5s %-8s %-10s\n","TIME_STAMP","CPU","PID","DELAY");
    // printf("    %-18lu %-5d %-8d %-10.2lf\n",
    //         detail->pre_sched_time,detail->cpu_id,detail->pid,detail->delay_ns/1000.0);
	// printf("\n");
}
static int keytime_image_extract(void *buf, unsigned int len, void *)
{
	int *et_type;
	struct events *detail;
	
	if (len == 0)
		return 0;
	et_type = (int *)buf;
	switch (*et_type) {
	case et_keytime_image_detail:
		if (len < sizeof(struct events))
			break;
		detail = (struct events *)buf;
		addEvent(detail);
		// data_processing(detail);
		break;
	default:
		break;
	}
	return 0;
}
static void print_info() {
    for (const auto& process : processEvents) {
        int pid = process.first;
        const TimeSortedEvents& events = process.second;

        std::cout << "PID:" << pid << " 进程名：" << process.second.begin()->second.comm << std::endl;

        int event_index = 1;
        for (const auto& eventPair : events) {
            uint64_t timestamp = eventPair.first;
            const event_info& event = eventPair.second;

            std::cout << "  " << event_index << ".时间：" << timestamp << std::endl;
            event_index++;

            switch (event.type) {
                case 1: // 上下 CPU
					if(event.evnt->on_off_cpu.flag){
						std::cout << "    #上CPU " << event.evnt->on_off_cpu.cpu << "；" << std::endl;
					}else
                    	std::cout << "    #下CPU " << event.evnt->on_off_cpu.cpu << "；" << std::endl;  
                    break;
                case 2: // 创建进程
                    std::cout << "    #创建子进程" << std::endl
                              << "    #child_pid：" << event.evnt->process_fork.child_pid
                              << "，child_comm：" << event.evnt->process_fork.child_comm
                              << "，child_tgid：" << event.evnt->process_fork.child_tgid << std::endl;
                    break;
                case 3: // 执行 execve
                    std::cout << "    #执行execve" << std::endl
                              << "    #详细信息：filename：" << event.evnt->execve.filename << std::endl;
                    break;
                case 4: // 进程结束
                    std::cout << "    #进程退出" << std::endl;
                    break;
                default:
                    std::cout << "    #未知事件类型。" << std::endl;
                    break;
            }
        }
        std::cout << std::endl;
    }
}
/*do_extract调用extract_variant_buffer去执行相应的打印函数*/
static void do_extract(char *buf, int len)
{
	extract_variant_buffer(buf, len, keytime_image_extract, NULL);//extract_variant_buffer被定义在/SOURCE/uapi/ali_diagnose.h中
	print_info();
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
		ret = diag_call_ioctl(DIAG_IOCTL_KEYTIME_IMAGE_DUMP, (long)&dump_param);//将内核中的数据拉到用户态
	} else {
		ret = -ENOSYS;
		syscall(DIAG_KEYTIME_IMAGE_DUMP, &ret, &len, variant_buf, 1 * 1024 * 1024);
	}
	if (ret == 0) {
		do_extract(variant_buf, len);
	}
}
int keytime_image_main(int argc, char **argv){
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
		usage_keytime_image();
		return 0;
	}
	while (1) {
		int option_index = -1;

		c = getopt_long_only(argc, argv, "", long_options, &option_index);//解析参数
		if (c == -1)
			break;
		switch (option_index) {
		case 0:
			usage_keytime_image();
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
		default:
			usage_keytime_image();
			break;
		}
	}

	return 0;    
}


