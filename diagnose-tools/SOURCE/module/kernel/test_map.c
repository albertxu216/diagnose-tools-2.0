#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/mm_types.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
// #include <linux/slab_def.h>
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
#include <linux/timekeeping.h>  

#include <asm/irq_regs.h>
#include <asm/unistd.h>

#if !defined(DIAG_ARM64)
#include <asm/asm-offsets.h>
#endif

#include "internal.h"
#include "pub/trace_file.h"
#include "pub/trace_point.h"

#include "uapi/test_map.h"

struct diag_test_map_settings test_map_settings;

static unsigned int test_map_alloced;
static struct diag_variant_buffer test_map_variant_buffer;//缓冲区

static atomic64_t diag_nr_running = ATOMIC64_INIT(0);

// 全局变量
static struct kmem_cache *hash_map_cache;
static struct kmem_cache *rbtree_cache;
static unsigned int hash_map_memory_usage = 0;
static unsigned int rbtree_memory_usage = 0;
static DEFINE_SPINLOCK(memory_lock);

// 获取kmem_cache对象大小的函数
// static inline unsigned int kmem_cache_size(struct kmem_cache *cachep) {
// 	unsigned int size = cachep->size;
//     return size;
// }
// 自定义缓存分配函数
void *cache_alloc(struct kmem_cache *cachep, unsigned int *memory_usage) {
    void *ptr = kmem_cache_alloc(cachep, GFP_KERNEL);
    if (ptr) {
        spin_lock(&memory_lock);
        *memory_usage += kmem_cache_size(cachep);
        spin_unlock(&memory_lock);
    }
    return ptr;
}

// 自定义缓存释放函数
void cache_free(struct kmem_cache *cachep, void *ptr, unsigned int *memory_usage) {
    if (ptr) {
        kmem_cache_free(cachep, ptr);
        spin_lock(&memory_lock);
        *memory_usage -= kmem_cache_size(cachep);
        spin_unlock(&memory_lock);
    }
}

// 宏定义
#define alloc_hash_map() cache_alloc(hash_map_cache, &hash_map_memory_usage)
#define free_hash_map(ptr) cache_free(hash_map_cache, ptr, &hash_map_memory_usage)
#define alloc_rbtree() cache_alloc(rbtree_cache, &rbtree_memory_usage)
#define free_rbtree(ptr) cache_free(rbtree_cache, ptr, &rbtree_memory_usage)


/*hash表的自定义*/
#define STAMP_BITS 16
#define STAMP_SIZE (1 << STAMP_BITS)
static struct hlist_head ht_hash[STAMP_SIZE];
struct hash_node {
	int key; 
	struct value value; 
	struct hlist_node node;
};

/*查找哈希*/
// 查找 start_rsc 的哈希表
static struct hash_node *find_hash(int key) {
    struct hash_node *t;
    hash_for_each_possible(ht_hash, t, node, hash_long(key, STAMP_BITS)) {
        if (key == t->key)
            return t;
    }
	// printk("find err start\n");
    return NULL;
}

/*更新哈希表*/
static bool update_hash(int key, struct value *value) {
    // struct hash_node *t = find_hash(key);
	// if(t){
	// 	t->value = *value;
	// 	// printk("pid : %d \t in update_hash find success!\n",key);
	// 	return;
	// }
	// printk("in update_hash find fail!  start to creat a new hashnode!\n");
    struct hash_node *t = alloc_hash_map();
	if(!t) return 0;
    t->value = *value;
    t->key = key;
    hash_add(ht_hash, &t->node,hash_long(key, STAMP_BITS));
	return 1;
}

/*删除哈希表*/
static void clean_hash(void) {
	struct hash_node *t;
	struct hlist_node *tmp;
	int i;
	hash_for_each_safe(ht_hash, i, tmp, t, node) {
		hash_del(&t->node);
		free_hash_map(t);
    }
}

/*rbtree*/
__maybe_unused static atomic64_t diag_alloc_count = ATOMIC64_INIT(0);

__maybe_unused static struct rb_root diag_test_map_tree = RB_ROOT;
__maybe_unused static DEFINE_SPINLOCK(diag_test_map_tree_lock);
struct diag_test_map {
	struct rb_node rb_node;
	struct list_head list;
    int key;
    struct value value;
};

__maybe_unused static void clean_rbtree(void)
{
	unsigned long flags;
	struct list_head header;
	struct rb_node *node;

	INIT_LIST_HEAD(&header);
	spin_lock_irqsave(&diag_test_map_tree_lock, flags);

	for (node = rb_first(&diag_test_map_tree); node; node = rb_next(node)) {
		struct diag_test_map *this = container_of(node,
				struct diag_test_map, rb_node);

		rb_erase(&this->rb_node, &diag_test_map_tree);
		INIT_LIST_HEAD(&this->list);
		list_add_tail(&this->list, &header);
	}
	diag_test_map_tree = RB_ROOT;

	spin_unlock_irqrestore(&diag_test_map_tree_lock, flags);

	while (!list_empty(&header)) {
		struct diag_test_map *this = list_first_entry(&header, struct diag_test_map, list);

		list_del_init(&this->list);
		free_rbtree(this);
	}
}

__maybe_unused static int compare_desc(struct diag_test_map *desc, struct diag_test_map *this)
{
	if (desc->key < this->key)
		return -1;
	if (desc->key > this->key)
		return 1;
	return 0;
}

__maybe_unused static struct diag_test_map *__find_alloc_desc(struct diag_test_map *desc){                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            
	struct diag_test_map *this;
	struct rb_node **node, *parent;
	int compare_ret;

	node = &diag_test_map_tree.rb_node;
	parent = NULL;

	while (*node != NULL)
	{
		parent = *node;
		this = container_of(parent, struct diag_test_map, rb_node);
		compare_ret = compare_desc(desc, this);

		if (compare_ret < 0)
			node = &parent->rb_left;
		else if (compare_ret > 0)
			node = &parent->rb_right;
		else
		{
			// printk("find this node %d!!\n",this->key);
			return this;//找到该点
		}
	}
    return 0;
}
__maybe_unused static struct diag_test_map *__add_alloc_desc(struct diag_test_map *desc)
{
	struct diag_test_map *this;
	struct rb_node **node, *parent;
	int compare_ret;

	node = &diag_test_map_tree.rb_node;
	parent = NULL;

	while (*node != NULL)
	{
		parent = *node;
		this = container_of(parent, struct diag_test_map, rb_node);
		compare_ret = compare_desc(desc, this);

		if (compare_ret < 0)
			node = &parent->rb_left;
		else if (compare_ret > 0)
			node = &parent->rb_right;
		else
		{
			printk("find this node %d!!\n",this->key);
			return this;//找到该点
		}
	}
	/*未找到，则重新申请空间*/

	this = alloc_rbtree();
	if (!this) {
		atomic64_inc_return(&diag_alloc_count);
		return this;
	}
	memset(this, 0, sizeof(struct diag_test_map));
	// printk("add this node %d!!\n",this->key);
	this->key = desc->key;
	this->value = desc->value;
	spin_lock_irq(&diag_test_map_tree_lock);
	rb_link_node(&this->rb_node, parent, node);
	rb_insert_color(&this->rb_node, &diag_test_map_tree);
	spin_unlock_irq(&diag_test_map_tree_lock);
	return this;
}

/*获取hashmap 内存使用情况*/
static unsigned int get_memory_usage_hash_map(void) {
    struct hash_node *t;
    struct hlist_node *tmp;
    unsigned int total_size = 0;
    int i;

    hash_for_each_safe(ht_hash, i, tmp, t, node) {
        total_size += sizeof(struct hash_node);
    }
    return total_size;
}

/*获取rbtree 内存使用情况*/
static unsigned int get_memory_usage_rbtree(void) {
    struct diag_test_map *this;
    struct rb_node *node;
    unsigned int total_size = 0;
    for (node = rb_first(&diag_test_map_tree); node; node = rb_next(node)) {
        this = container_of(node, struct diag_test_map, rb_node);
        total_size += sizeof(struct diag_test_map);
    }
    return total_size;
}
static void test_hash_map(struct test_map_detail *detail){
	u64 start, end;
    int num_elements = test_map_settings.num_elements;  // 测试插入 100 万个元素
    int i;

	/*插入测试*/
	if(test_map_settings.which_index!=-2){
    	printk("hash map 插入性能测试...\n");
    	start = ktime_to_ns(ktime_get());
    	for (i = 0; i < num_elements; ++i) {
    	    struct value val = { .test = i };  // 假设结构体 value 中有数据域 data
    	    if(!update_hash(i, &val)) {
				printk("hashmap %d 个节点申请空间失败\n",i);
				continue;
			}
    	}
    	end = ktime_to_ns(ktime_get());
    	printk("Hash map 插入%d个元素 消耗%llu ns 消耗内存: %zu bytes\n", num_elements,
    	       end-start,hash_map_memory_usage);
		detail->hash_insert_delay = end-start;
		detail->hash_map_insert_memory_usage = hash_map_memory_usage;
	}



    // 查找性能测试
	if(test_map_settings.which_index==0 || test_map_settings.which_index==2){
    	printk("hash map 查找性能测试...\n");
    	start = ktime_to_ns(ktime_get());
    	for (i = 0; i < num_elements; ++i) {
    	    struct hash_node *node = find_hash(i);
    	    if (!node) {
    	        printk("Element %d not found!\n", i);
    	        break;
    	    }
    	}
    	end = ktime_to_ns(ktime_get());
    	printk("Hash map 查找%d个元素 消耗%llu ns\n", num_elements,
    	       end-start);
		detail->hash_find_delay = end-start;
	}

    // 内存使用情况测试
    // unsigned int hash_map_memory_usage = get_memory_usage_hash_map();
    // printk("Hash map共消耗内存: %zu bytes\n", hash_map_memory_usage);
	// detail->hash_map_memory_usage = hash_map_memory_usage;

    // 删除性能测试
	if(test_map_settings.which_index==0 || test_map_settings.which_index==-1){
    	printk("hash map 删除性能测试...\n");
    	start = ktime_to_ns(ktime_get());
    	clean_hash();
    	end = ktime_to_ns(ktime_get());
    	printk("Hash map 删除%d个元素 消耗%llu ns 目前内存为：%zu bytes\n", num_elements,
    	       end-start,hash_map_memory_usage);
		detail->hash_delet_delay = end-start;
		detail->hash_map_delet_memory_usage = hash_map_memory_usage;
	}

}

static void test_rbtree(struct test_map_detail *detail) {
    u64 start, end;
    int num_elements = test_map_settings.num_elements;  // 测试插入 100 万个元素
    int i;

	/*插入测试*/
	if(test_map_settings.which_index!=-2){
    	printk("rbtree 插入性能测试...\n...\n");
    	start = ktime_to_ns(ktime_get());
    	for (i = 0; i < num_elements; ++i) {
    	    struct diag_test_map this = {.key = i,.value=i,};
			struct diag_test_map *desc = __add_alloc_desc(&this);
    	    if (!desc) {
    	        printk("rbtree 第%d个节点插入时 内存申请失败!\n",i);
    	        break;
    	    }
    	}
    	end = ktime_to_ns(ktime_get());
    	printk("RBtree 插入%d个元素 消耗%llu ns 消耗内存: %zu bytes\n", num_elements,
    	       end-start,rbtree_memory_usage);
		detail->rbtree_insert_delay = end-start;
		detail->rbtree_insert_memory_usage = rbtree_memory_usage;
	}


	// 查找性能测试
	if(test_map_settings.which_index==0 || test_map_settings.which_index==2){
    	printk("RBtree 查找性能测试...\n");
    	start = ktime_to_ns(ktime_get());
    	for (i = 0; i < num_elements; ++i) {
    	    struct diag_test_map desc = { .key = i };
    	    struct diag_test_map *node = __find_alloc_desc(&desc);
    	    if (!node) {
    	        printk("rbtree 元素 %d 未找到!\n", i);
    	        break;
    	    }
    	}
    	end = ktime_to_ns(ktime_get());
    	printk("RBTree find time for %d elements: %lld ns\n", num_elements,
    	       end-start);
		detail->rbtree_find_delay = end-start;
	}


    // 内存使用情况测试
    // unsigned int rbtree_memory_usage = get_memory_usage_rbtree();
    // printk("RBtree 共消耗内存: %zu bytes\n", rbtree_memory_usage);
	// detail->rbtree_memory_usage = rbtree_memory_usage;

    // 删除性能测试
	if(test_map_settings.which_index==0 || test_map_settings.which_index==-1){
    	printk("RBtree 删除性能测试...\n");
    	start = ktime_to_ns(ktime_get());
    	clean_rbtree();
    	end = ktime_to_ns(ktime_get());
    	printk("RBtree 删除%d个元素 消耗%llu ns 目前内存为：%zu bytes\n", num_elements,
    	       end-start,rbtree_memory_usage);
		detail->rbtree_delet_delay = end-start;
		detail->rbtree_delet_memory_usage = rbtree_memory_usage;	
	}
}


/// @brief  激活功能，统计时间单位为ns
/// @param  
/// @return 
static int __activate_test_map(void)
{
	msleep(3000);//等待2s
	int ret;
	int flags;
	ret = alloc_diag_variant_buffer(&test_map_variant_buffer);//为缓冲区申请空间
	if (ret)
		goto out_variant_buffer;
	test_map_alloced = 1;
	if(test_map_settings.which_map >=0){
    	hash_map_cache = kmem_cache_create("hash_map_cache", sizeof(struct hash_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    	if (!hash_map_cache){
			printk("创建hash_map_cache失败\n");
			return -ENOMEM;
		}
	}
	if(test_map_settings.which_map <=0){
    	rbtree_cache = kmem_cache_create("rbtree_cache", sizeof(struct diag_test_map), 0, SLAB_HWCACHE_ALIGN, NULL);
    	if (!rbtree_cache) {
			printk("创建rbtree_cache失败\n");
			if(!hash_map_cache)
    	    	kmem_cache_destroy(hash_map_cache);
    	    return -ENOMEM;
    	}
	}


	struct test_map_detail detail;
	if(test_map_settings.which_map >=0){
		hash_init(ht_hash);
		test_hash_map(&detail);
	}
	if(test_map_settings.which_map <=0){
		test_rbtree(&detail);
	}

	detail.et_type = et_test_map_detail;
	do_diag_gettimeofday(&detail.tv);
	detail.which_map = test_map_settings.which_map;
	detail.which_index = test_map_settings.which_index;
	detail.num_elements = test_map_settings.num_elements;

	diag_variant_buffer_spin_lock(&test_map_variant_buffer, flags);
	diag_variant_buffer_reserve(&test_map_variant_buffer, sizeof(struct test_map_detail));
	diag_variant_buffer_write_nolock(&test_map_variant_buffer, &detail, sizeof(struct test_map_detail));
	diag_variant_buffer_seal(&test_map_variant_buffer);
	diag_variant_buffer_spin_unlock(&test_map_variant_buffer, flags);	

	return 1;
out_variant_buffer:
	return 0;
}

static void __deactivate_test_map(void)
{

	synchronize_sched();
	msleep(20);
	while (atomic64_read(&diag_nr_running) > 0)
		msleep(20);
    clean_hash();
    clean_rbtree();
}

int activate_test_map(void)
{
	if (!test_map_settings.activated)
		test_map_settings.activated = __activate_test_map();

	return test_map_settings.activated;
}

int deactivate_test_map(void)
{
	if (test_map_settings.activated)
		__deactivate_test_map();
	test_map_settings.activated = 0;

	return 0;
}


int test_map_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	unsigned int __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_test_map_settings settings;
	int cpu;

	switch (id) {
	case DIAG_TEST_MAP_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (unsigned int)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_test_map_settings)) {
			ret = -EINVAL;
		} else if (test_map_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				test_map_settings = settings;
			}
		}
		break;
	case DIAG_TEST_MAP_SETTINGS:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (unsigned int)SYSCALL_PARAM2(regs);

		memset(&settings, 0, sizeof(settings));
		if (user_buf_len != sizeof(struct diag_test_map_settings)) {
			ret = -EINVAL;
		} else {
			settings = test_map_settings;
			ret = copy_to_user(user_buf, &settings, user_buf_len);
		}
		break;
	case DIAG_TEST_MAP_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (unsigned int)SYSCALL_PARAM3(regs);
		if (!test_map_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&test_map_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("test-map");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

long diag_ioctl_test_map(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_test_map_settings settings;
	struct diag_ioctl_dump_param dump_param;
	int cpu;

	switch (cmd) {
	case CMD_TEST_MAP_SET:
		if (test_map_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_test_map_settings));
			if (!ret) {
				test_map_settings = settings;
			}
		}
		break;
	case CMD_TEST_MAP_SETTINGS:
		settings = test_map_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_test_map_settings));
		break;
	case CMD_TEST_MAP_DUMP:
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!test_map_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			ret = copy_to_user_variant_buffer(&test_map_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("test-map");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
    return 0;
}

/*test_map入口*/
int diag_test_map_init(void)
{
	init_diag_variant_buffer(&test_map_variant_buffer, 1 * 1024 * 1024);//初始化缓冲区

	if (test_map_settings.activated)//激活该功能
		test_map_settings.activated = __activate_test_map();//调用__activate_test_map激活该功能（挂载到挂载点）

	return 0;
}

void diag_test_map_exit(void)
{
	if (test_map_settings.activated)
		deactivate_test_map();//撤销挂载点
	test_map_settings.activated = 0;
    clean_hash();
    clean_rbtree();
    kmem_cache_destroy(hash_map_cache);
    kmem_cache_destroy(rbtree_cache);
	destroy_diag_variant_buffer(&test_map_variant_buffer);//释放缓冲区
}
