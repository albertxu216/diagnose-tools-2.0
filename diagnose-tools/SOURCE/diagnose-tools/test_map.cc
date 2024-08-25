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

#include "uapi/test_map.h"
#include "params_parse.h"
#include <syslog.h>

using namespace std;
long long  interval;
//用于记录监测的总时间
void usage_test_map(void)
{
    printf("----------------------------------\n");
	printf("    test_map usage:\n");
	printf("        --help test_map help info\n");
	printf("        --activate\n");
	printf("          num num_elements\n");
	printf("          index insert/find/delet\n");
	// printf("          threshold THRESHOLD(MS)\n");
	// printf("          tgid process group monitored\n");
	// printf("          pid thread id that monitored\n");
	// printf("          comm comm that monitored\n");
	printf("        --deactivate\n");
	// printf("        --report dump log with text.\n");
}

static void do_activate(const char *arg)
{
	int ret ;
	struct params_parser parse(arg);
	struct diag_test_map_settings settings;//控制结构体
	string str;
    /*初始化setting*/
	memset(&settings, 0, sizeof(struct diag_test_map_settings));
    /*根据参数赋值*/
    settings.which_map = parse.int_value("map");
	settings.num_elements = parse.int_value("num");
	settings.which_index = parse.int_value("index");
    /*传递信号,调用系统调用去设置settings*/
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TEST_MAP_SET, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TEST_MAP_SET, &ret, &settings, sizeof(struct diag_test_map_settings));
	}
	printf("功能设置%s，返回值：%d\n", ret ? "失败" : "成功", ret);
    if(settings.which_map==1){
        printf("    测试map：\t%s \n", "hash map");
    }else if(settings.which_map == 0) {
        printf("    测试map：\t%s \n", "hash map && rbtree");
    }else 
        printf("    测试map：\t%s\n", "rbtree");
	printf("    测试功能：\t%s\n",(settings.which_index ==-1?"删除":(settings.which_index==0 ?"全部"
								:(settings.which_index==1?"插入":(settings.which_index==2?"查找"
								:(settings.which_index==-2?"不执行任何操作的base数据":"错误参数！"))))));
	printf("    测试样本数据量：\t%d\n",settings.num_elements);
	if (ret)
		return;


    /*在内核中真正地激活
     *  调用misc.cc中的diag_activate函数，
     *  通过标准输入流，会调用向/proc/ali-linux/diagnose/controller
     *  写入test-map ，写成功会返回1，将1赋给ret
     */
	ret = diag_activate("test-map");
	if (ret == 1) {
		printf("test-map activated\n");
	} else {
		printf("test-map is not activated, ret %d\n", ret);
	}
}

static void do_deactivate(void)
{
	int ret = 0;

	//向controller写入deactivate 
	ret = diag_deactivate("test-map");//通过/proc/controller失活函数
	if (ret == 0) {
		printf("test-map is not activated\n");
	} else {
		printf("deactivate test-map fail, ret is %d\n", ret);
	}
}

static void print_settings_in_json(struct diag_test_map_settings *settings, int ret)
{
	Json::Value root;
	std::string str_log;

	if (ret == 0) {
		root["activated"] = Json::Value(settings->activated);
		// root["tgid"] = Json::Value(settings->tgid);
		// root["pid"] = Json::Value(settings->pid);
		// root["comm"] = Json::Value(settings->comm);
		// root["verbose"] = Json::Value(settings->verbose);
	} else {
		root["err"] = Json::Value("found test-map settings failed, please check if diagnose-tools is installed correctly or not.");
	}

	str_log.append(root.toStyledString());
	printf("%s", str_log.c_str());

	return;
}


static void do_settings(const char *arg)
{
	struct diag_test_map_settings settings;
	int ret;
	int enable_json = 0;
	struct params_parser parse(arg);
	enable_json = parse.int_value("json");

	memset(&settings, 0, sizeof(struct diag_test_map_settings));

	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TEST_MAP_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TEST_MAP_SETTINGS, &ret, &settings, sizeof(struct diag_test_map_settings));
	}

	if (1 == enable_json) {
		return print_settings_in_json(&settings, ret);
	}

	if (ret == 0) {
		printf("功能设置：\n");
		printf("    是否激活：%s\n", settings.activated ? "√" : "×");
        if(settings.which_map>0){
            printf("    测试map：\t%s \n", "hash map");
        }else if(settings.which_map == 0) {
            printf("    测试map：\t%s \n", "hash map && rbtree");
        }else 
            printf("    测试map：\t%s\n", "rbtree");
		printf("    测试功能：\t%s\n",(settings.which_index <0?"删除":(settings.which_index==0 ?"全部"
									:(settings.which_index==1?"插入":(settings.which_index==2?"查找":(settings.which_index==-2?"不执行任何操作的base数据":"错误参数！"))))));
		printf("    测试样本数据量：\t%d\n",settings.num_elements);
	} 
	else {
	    	printf("获取test-map设置失败，请确保正确安装了diagnose-tools工具\n");
	}
}   
static void data_processing(struct test_map_detail * detail){
	// printf("time:%ld \t delay:%ld\n",detail->time,interval);
	printf("MAP性能测试情况说明:\n");
	printf(" #1.插入元素 %d个\n",detail->num_elements);
	printf(" #2.查找元素 %d个\n",detail->num_elements);
	printf(" #3.删除元素 %d个\n",detail->num_elements);
	if(detail->which_map>=0){
		printf("---------HASH MAP----------\n");
		switch (detail->which_index)
		{
		case -2:
			printf(" ##测试基准base，不做任何操作\n");
			break;
		case 1:
			printf(" #1.插入消耗时间 %llu ns\n",detail->hash_insert_delay>=0?detail->hash_insert_delay:-1);
			printf("    插入消耗内存 %zu bytes\n",detail->hash_map_insert_memory_usage>=0?detail->hash_map_insert_memory_usage:-1);			
			break;
		case 2:
			printf(" #2.查找消耗时间 %llu ns\n",detail->hash_find_delay>=0?detail->hash_find_delay:-1);
			break;
		case -1:
			printf(" #3.删除消耗时间 %llu ns\n",detail->hash_delet_delay);
			printf("    删除消耗内存 %zu bytes\n",detail->hash_map_delet_memory_usage);
			break;
		case 0:
			printf(" #1.插入消耗时间 %llu ns\n",detail->hash_insert_delay>=0?detail->hash_insert_delay:-1);
			printf("    插入消耗内存 %zu bytes\n",detail->hash_map_insert_memory_usage>=0?detail->hash_map_insert_memory_usage:-1);
			printf(" #2.查找消耗时间 %llu ns\n",detail->hash_find_delay>=0?detail->hash_find_delay:-1);
			printf(" #3.删除消耗时间 %llu ns\n",detail->hash_delet_delay);
			printf("    删除消耗内存 %zu bytes\n",detail->hash_map_delet_memory_usage);		
			break;
		default:
			break;
		}
	} 
	if(detail->which_map<=0){
		printf("---------RBtree----------\n");
		switch (detail->which_index)
		{
		case -2:
			printf(" ##测试基准base，不做任何操作\n");
			break;
		case 1:
			printf(" #1.插入消耗 %llu ns\n",detail->rbtree_insert_delay);
			printf("    插入消耗内存 %zu bytes\n",detail->rbtree_insert_memory_usage);
			break;
		case 2:
			printf(" #2.查找消耗 %llu ns\n",detail->rbtree_find_delay);
			break;
		case -1:
			printf(" #3.删除消耗 %llu ns\n",detail->rbtree_delet_delay);
			printf("    删除消耗内存 %zu bytes\n",detail->rbtree_delet_memory_usage);
			break;
		case 0:
			printf(" #1.插入消耗 %llu ns\n",detail->rbtree_insert_delay);
			printf("    插入消耗内存 %zu bytes\n",detail->rbtree_insert_memory_usage);
			printf(" #2.查找消耗 %llu ns\n",detail->rbtree_find_delay);
			printf(" #3.删除消耗 %llu ns\n",detail->rbtree_delet_delay);
			printf("    删除消耗内存 %zu bytes\n",detail->rbtree_delet_memory_usage);	
			break;
		default:
			break;
		}
	}
}
static int test_map_extract(void *buf, unsigned int len, void *)
{
	int *et_type;
	struct test_map_detail *detail;
	if (len == 0)
		return 0;
	et_type = (int *)buf;

	switch (*et_type) {
	case et_test_map_detail:
		if (len < sizeof(struct test_map_detail))
			break;
		detail = (struct test_map_detail *)buf;
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
	extract_variant_buffer(buf, len, test_map_extract, NULL);//extract_variant_buffer被定义在/SOURCE/uapi/ali_diagnose.h中
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
		ret = diag_call_ioctl(DIAG_IOCTL_TEST_MAP_DUMP, (long)&dump_param);//将内核中的数据拉到用户态
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TEST_MAP_DUMP, &ret, &len, variant_buf, 1 * 1024 * 1024);
	}
	/*获得setting中开始时间的值*/
	struct diag_test_map_settings settings;
	memset(&settings, 0, sizeof(struct diag_test_map_settings));
	if (run_in_host) {
		ret = diag_call_ioctl(DIAG_IOCTL_TEST_MAP_SETTINGS, (long)&settings);
	} else {
		ret = -ENOSYS;
		syscall(DIAG_TEST_MAP_SETTINGS, &ret, &settings, sizeof(struct diag_test_map_settings));
	}
	if (ret == 0) {
		do_extract(variant_buf, len);
	}
}
int test_map_main(int argc, char **argv){
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
		usage_test_map();
		return 0;
	}
	while (1) {
		int option_index = -1;

		c = getopt_long_only(argc, argv, "", long_options, &option_index);//解析参数
		if (c == -1)
			break;
		switch (option_index) {
		case 0:
			usage_test_map();
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
			usage_test_map();
			break;
		}
	}

	return 0;    
}


