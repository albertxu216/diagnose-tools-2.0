#
# ../../vender/module.mk 用于支持厂商自有ＯＳ版本
#

DIAG_EXTRA_CFLAGS := -DMODULE

ARCH := $(shell uname -i)
UNAME := $(shell uname -r)
ifeq ($(KERNEL_BUILD_PATH),)
	KERNEL_BUILD_PATH := /lib/modules/$(UNAME)/build
endif

ifeq ($(CFLAGS_MODULE),)
	DIAG_EXTRA_CFLAGS += $(CFLAGS_MODULE)
endif

ifeq ($(EXPERIENTIAL),1)
	DIAG_EXTRA_CFLAGS += -DEXPERIENTIAL
endif

ifneq ($(findstring el8,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_8U

	ifneq ($(findstring 4.18.0-80,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_4_18_80
	endif

	ifneq ($(findstring 4.18.0-147,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_4_18_147
	endif

	ifneq ($(findstring 4.18.0-193,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_4_18_193
	endif
#	ifneq ($(findstring 4.18.0-193.19.1.el8_2.x86_64,$(KERNEL_BUILD_PATH)),)
#		DIAG_EXTRA_CFLAGS += -DCENTOS_4_18_193_19_1
#	endif
endif

ifneq ($(findstring el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_7U

	ifneq ($(findstring 3.10.0-293,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_293
	endif

	ifneq ($(findstring 3.10.0-514,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_514
	endif

	ifneq ($(findstring 3.10.0-693,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_693
	endif

	ifneq ($(findstring 3.10.0-862,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_862
	endif

	ifneq ($(findstring 3.10.0-957,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_957
	endif

	ifneq ($(findstring 3.10.0-1062,$(KERNEL_BUILD_PATH)),)
                DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1062
        endif

	ifneq ($(findstring 3.10.0-1127,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1127
	endif
endif

ifneq ($(findstring alios7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DALIOS_7U

	ifneq ($(findstring ali2016,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DALIOS_2016
	endif

	ifneq ($(findstring ali3000,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DALIOS_3000

		ifneq ($(findstring -009,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_009
		endif

		ifneq ($(findstring -010,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_010
		endif

		ifneq ($(findstring -011,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_011
		endif

		ifneq ($(findstring -012,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_012
		endif

		ifneq ($(findstring -013,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_013
		endif

		ifneq ($(findstring -014,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_014
		endif

		ifneq ($(findstring -015,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_015
		endif

		ifneq ($(findstring -016,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_016
		endif

		ifneq ($(findstring -017,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_017
		endif

		ifneq ($(findstring -018,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_018
		endif

		ifneq ($(findstring -019,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_3000_019
		endif
	endif

	ifneq ($(findstring ali4000,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DALIOS_4000

		ifneq ($(findstring -007,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_4000_007
		endif
		ifneq ($(findstring -009,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_4000_009
		endif
		ifneq ($(findstring -011,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_4000_011
		endif
	endif

	ifneq ($(findstring ali5000,$(KERNEL_BUILD_PATH)),)
		DIAG_EXTRA_CFLAGS += -DALIOS_5000
		ifneq ($(findstring -003,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_5000_003
		endif
		ifneq ($(findstring -004,$(KERNEL_BUILD_PATH)),)
			DIAG_EXTRA_CFLAGS += -DALIOS_5000_004
		endif
	endif
endif

ifneq ($(findstring alios7.aarch64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DDIAG_ARM64
endif
ifneq ($(findstring aarch64,$(ARCH)),)
	DIAG_EXTRA_CFLAGS += -DDIAG_ARM64
endif

#ifneq ($(findstring 3.10.0-957.el7.x86_64,$(KERNEL_BUILD_PATH)),)
#	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_957 -DCENTOS_7U
#endif

ifneq ($(findstring 3.10.0-957.21.3.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_957_21_3
endif

ifneq ($(findstring 3.10.0-957.27.2.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_957_27_2
endif

#ifneq ($(findstring 3.10.0-1062.el7.x86_64,$(KERNEL_BUILD_PATH)),)
#	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1062 -DCENTOS_7U
#endif

ifneq ($(findstring 3.10.0-1062.1.2.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1062_1_2
endif

ifneq ($(findstring 3.10.0-1062.9.1.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1062_9_1
endif

#ifneq ($(findstring 3.10.0-1127.el7.x86_64,$(KERNEL_BUILD_PATH)),)
#	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1127
#endif

ifneq ($(findstring 3.10.0-1127.8.2.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1127_8_2
endif

ifneq ($(findstring 3.10.0-1127.10.1.el7.x86_64,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DCENTOS_3_10_1127_10_1
endif

ifneq ($(findstring 4.15.0-112-generic,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DUBUNTU_1604
endif

ifneq ($(findstring 4.4.0-93-generic,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DUBUNTU_1604
endif

ifneq ($(findstring 4.4.38+,$(KERNEL_BUILD_PATH)),)
	DIAG_EXTRA_CFLAGS += -DUBUNTU_1604
endif

ifneq ($(KERNELRELEASE),)
	TARGET := diagnose
	obj-m = $(TARGET).o

	ccflags-y += -I$(PWD) -I$(PWD)/../ -Wuninitialized

	$(TARGET)-objs = pub/mem_pool.o pub/trace_buffer.o pub/trace_file.o pub/variant_buffer.o pub/trace_point.o \
			pub/kprobe.o pub/stack.o pub/cgroup.o pub/fs_utils.o pub/uprobe.o pub/symbol.o
	ifeq ($(EXPERIENTIAL),1)
		$(TARGET)-objs += pub/remote_stack.o
	endif

	$(TARGET)-objs += entry.o chr_dev.o misc.o symbol.o mm_tree.o stack.o stub.o pupil.o
	$(TARGET)-objs += kernel/kern_entry.o kernel/mutex.o kernel/load.o kernel/exit.o \
			kernel/irq_stats.o kernel/irq_delay.o kernel/sys_delay.o \
			kernel/exec.o kernel/perf.o kernel/run_trace.o kernel/irq_trace.o \
			kernel/kprobe.o kernel/utilization.o kernel/sched_delay.o kernel/reboot.o \
			kernel/uprobe.o kernel/sys_cost.o kernel/sig_info.o kernel/task_monitor.o \
			kernel/rw_sem.o kernel/throttle_delay.o kernel/bad_time.o \
			kernel/resource_image.o kernel/migrate_image.o kernel/sched_image.o kernel/keytime_image.o kernel/test_map.o 


	$(TARGET)-objs += mm/mm_entry.o mm/alloc_page.o mm/alloc_top.o mm/high_order.o mm/rss_monitor.o mm/memcg_stats.o
	$(TARGET)-objs += io/io_entry.o
	$(TARGET)-objs += fs/fs_entry.o fs/orphan.o fs/shm.o fs/rw_top.o
	$(TARGET)-objs += net/net_entry.o net/tcp_retrans.o net/drop_packet.o net/ping_delay.o net/ping_delay6.o \
			net/net_bandwidth.o net/tcp_connect.o net/tcpstates.o net/udp.o
	$(TARGET)-objs += pmu/entry.o pmu/pmu.o pmu/debug.o

	ifeq ($(EXPERIENTIAL),1)
		$(TARGET)-objs += test/test.o
	endif

	$(TARGET)-objs += mm/memory_leak.o

	ifeq ($(EXPERIENTIAL),1)
		$(TARGET)-objs += fs/cache.o
	endif
else
    DIAG_STRIP := 1
    include ${shell pwd}/../../vender/module.mk
    ifeq ($(KERNEL_BUILD_PATH),)
        KERNEL_BUILD_PATH=/lib/modules/$(shell uname -r)/build
    endif
    MOD_PATH := $(shell pwd)
    default:
	$(MAKE) CFLAGS_MODULE="$(DIAG_EXTRA_CFLAGS)" -C $(KERNEL_BUILD_PATH) M=$(MOD_PATH) modules
        ifeq ($(DIAG_STRIP),1)
	    strip --strip-debug diagnose.ko
        endif

    clean:
	    rm -rf *.o *.mod.c *.mod.o *.ko* Module.symvers modules.order .*.cmd
	    rm -rf net/*.o net/*.mod.c net/*.mod.o net/*.ko* net/Module.symvers net/modules.order net/.*.cmd
	    rm -rf io/*.o io/*.mod.c io/*.mod.o io/*.ko* io/Module.symvers io/modules.order io/.*.cmd
	    rm -rf kernel/*.o kernel/*.mod.c kernel/*.mod.o kernel/*.ko* kernel/Module.symvers kernel/modules.order kernel/.*.cmd
	    rm -rf hooks/*.o hooks/*.mod.c hooks/*.mod.o hooks/*.ko* hooks/Module.symvers hooks/modules.order hooks/.*.cmd
	    rm -rf fs/*.o fs/*.mod.c fs/*.mod.o fs/*.ko* fs/Module.symvers fs/modules.order fs/.*.cmd
	    rm -rf mm/*.o mm/*.mod.c mm/*.mod.o mm/*.ko* mm/Module.symvers mm/modules.order mm/.*.cmd
	    rm -rf pub/*.o pub/*.mod.c pub/*.mod.o pub/*.ko* pub/Module.symvers pub/modules.order pub/.*.cmd
	    rm -rf test/*.o test/*.mod.c test/*.mod.o test/*.ko* test/Module.symvers test/modules.order test/.*.cmd
	    rm -rf .tmp_versions
endif
