/*
 * Linux内核诊断工具--用户接口API
 * 
 * Copyright (C) 2022 Alibaba Ltd.
 *
 * 作者: Yang Wei <albin.yangwei@linux.alibaba.com>
 *
 * License terms: GNU General Public License (GPL) version 3
 *
 */
#ifndef UAPI_TEST_MAP_H
#define UAPI_TEST_MAP_H

#include <linux/ioctl.h>
int test_map_syscall(struct pt_regs *regs, long id);

#define DIAG_TEST_MAP_SET (DIAG_BASE_SYSCALL_TEST_MAP)
#define DIAG_TEST_MAP_SETTINGS (DIAG_TEST_MAP_SET + 1)
#define DIAG_TEST_MAP_DUMP (DIAG_TEST_MAP_SETTINGS + 1)

#define BITS_PER_LEN	64

typedef unsigned long long u64;
typedef unsigned int u32;

struct diag_test_map_settings {
	unsigned int activated;
    /*0:rbtree hash map全测
     *1:hash map
     *-1:rbmap
     */
	unsigned int which_map;
    /*1:插入
     *2:查找
     *-1:删除
     *-2:不执行任何操作
     *0:全部
     */
    unsigned int which_index;
    int num_elements;
};
struct value{
    u64 test;//何时开始
};

struct test_map_detail {
    int et_type;
    int which_map;
    int which_index;
	struct diag_timespec tv;
    int num_elements;
    u64 hash_insert_delay,rbtree_insert_delay;
    u64 hash_find_delay,rbtree_find_delay;
    u64 hash_delet_delay,rbtree_delet_delay;
    unsigned int hash_map_insert_memory_usage,hash_map_delet_memory_usage;
    unsigned int rbtree_insert_memory_usage,rbtree_delet_memory_usage;
};



#define CMD_TEST_MAP_SET (0)
#define CMD_TEST_MAP_SETTINGS (CMD_TEST_MAP_SET + 1)
#define CMD_TEST_MAP_DUMP (CMD_TEST_MAP_SETTINGS + 1)
#define DIAG_IOCTL_TEST_MAP_SET _IOWR(DIAG_IOCTL_TYPE_TEST_MAP, CMD_TEST_MAP_SET, struct diag_test_map_settings)
#define DIAG_IOCTL_TEST_MAP_SETTINGS _IOWR(DIAG_IOCTL_TYPE_TEST_MAP, CMD_TEST_MAP_SETTINGS, struct diag_test_map_settings)
#define DIAG_IOCTL_TEST_MAP_DUMP _IOWR(DIAG_IOCTL_TYPE_TEST_MAP, CMD_TEST_MAP_DUMP, struct diag_ioctl_dump_param)

#endif /*UAPI_TEST_MAP_H*/

