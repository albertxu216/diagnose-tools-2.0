/*
 * Linux内核诊断工具--内核态udp功能
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
#include "uapi/udp.h"
#include "pub/variant_buffer.h"
#include "pub/trace_file.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"
#include "internal.h"

#define ADDR_PAIR_HASH_BITS 20
static atomic64_t diag_nr_running = ATOMIC64_INIT(0);
static struct diag_variant_buffer udp_variant_buffer;

static struct kprobe kprobe_udp_rcv;
// static struct kprobe kprobe_ip_rcv;
static struct kprobe kprobe_udp_send_skb;
// static struct kprobe kprobe_ip_send_skb;

#define DNS_PROT 1
#define QUIC_PROT 2
#define SSDP_PROT 3
#define NTP_PROT 4
#define MDNS_PROT 5
#define NFS_PROT 6
#define SNMP_PROT 7
#define TFTP_PROT 8
#define DHCP_PROT 9
#define RIP_PROT 10
static struct diag_udp_settings udp_settings;
static unsigned int udp_alloced;
static int searchprot(struct udp_detail *detail,unsigned int sport,unsigned int dport)
{
	if(sport==443||dport==443)
	{
		detail->prot = QUIC_PROT;
	}
	else if(sport==1900||dport==1900)
	{
		detail->prot = SSDP_PROT;
	}
	else if(sport==123||dport==123)
	{
		detail->prot = NTP_PROT;
	}
	else if(sport==5353||dport==5353)
	{
		detail->prot = MDNS_PROT;
	}
	else if(sport==2049||dport==2049)
	{
		detail->prot = NFS_PROT;
	}
	else if(sport==161||dport==161)
	{
		detail->prot = SNMP_PROT;
	}
	else if(sport==69||dport==69)
	{
		detail->prot = TFTP_PROT;
	}
	else if(sport==68||dport==68)
	{
		detail->prot = DHCP_PROT;
	}
	else if(sport==520||dport==520)
	{
		detail->prot = RIP_PROT;
	}
	return 0;
}
static void parse_domain(const unsigned char *data, char *output,__u32 *addr) {
	__u8 len=*data++;
	int i =0;
    // 循环到尾部，标志0
	while (len)
	{
		output[i++]=*data++;
		if(--len==0)
		{
			len=*data++;
			if(len==0)
				break;
			else
				output[i++]='.';
		}
	}
	
    output[i++] = '\0'; // 确保字符串正确结束
	//printk("%s",output);
	data+=16;
	*addr =(__u32)((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
	//printk("%x",addr);
	return;
	
}
static void parse_domain_send(const unsigned char *data, char *output) {
	__u8 len=*data++;
	int i =0;
    // 循环到尾部，标志0
	while (len)
	{
		output[i++]=*data++;
		if(--len==0)
		{
			len=*data++;
			if(len==0)
				break;
			else
				output[i++]='.';
		}
	}
	
    output[i++] = '\0'; // 确保字符串正确结束
	//printk("%s",output);
	return;
	
}
static int dns_rcv_proc(struct udp_detail detail,struct dns_header* data)
{
	struct udp_dns_rcv detail_rcv ={};
	char domain_name[256];
	unsigned long flags;
	__u32 addr;
	void *dns_ques_data;
	detail_rcv.comm=detail;
	detail_rcv.comm.et_type = et_udp_dns_rcv;

	detail_rcv.qdcount=ntohs(data->qdcount);
	if(detail_rcv.qdcount != 1 ) return 0;
	detail_rcv.ancount=ntohs(data->ancount);
	if(!detail_rcv.ancount ) return 0;

	dns_ques_data=(void *)((void*)data+sizeof(struct dns_header));
	parse_domain((const unsigned char *)dns_ques_data,domain_name,&addr);
	//printk("%s",domain_name);
	strncpy(detail_rcv.question, domain_name, 255);
	detail_rcv.answer_addr=addr;

	diag_variant_buffer_spin_lock(&udp_variant_buffer, flags);
	diag_variant_buffer_reserve(&udp_variant_buffer, sizeof(struct udp_dns_rcv));
	diag_variant_buffer_write_nolock(&udp_variant_buffer, &detail_rcv, sizeof(struct udp_dns_rcv));
	diag_variant_buffer_seal(&udp_variant_buffer);
	diag_variant_buffer_spin_unlock(&udp_variant_buffer, flags);
	return 0;
}
static int dns_send_proc(struct udp_detail detail,struct dns_header* data)
{
	struct udp_dns_send detail_send ={};
	char domain_name[256];
	unsigned long flags;
	__u32 addr;
	void *dns_ques_data;
	detail_send.comm=detail;
	detail_send.comm.et_type = et_udp_dns_send;

	detail_send.qdcount=ntohs(data->qdcount);
	if(detail_send.qdcount != 1 ) return 0;
	dns_ques_data=(void *)((void*)data+sizeof(struct dns_header));
	parse_domain_send((const unsigned char *)dns_ques_data,domain_name);
	
	strncpy(detail_send.question,domain_name, 255);
	printk("%s",detail_send.question);
	diag_variant_buffer_spin_lock(&udp_variant_buffer, flags);
	diag_variant_buffer_reserve(&udp_variant_buffer, sizeof(struct udp_dns_send));
	diag_variant_buffer_write_nolock(&udp_variant_buffer, &detail_send, sizeof(struct udp_dns_send));
	diag_variant_buffer_seal(&udp_variant_buffer);
	diag_variant_buffer_spin_unlock(&udp_variant_buffer, flags);
	return 0;
}
static int kprobe_udp_rcv_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct sk_buff *skb = (void *)ORIG_PARAM1(regs);
	struct iphdr *iphdr;
    struct udphdr *udphdr;
    iphdr = ip_hdr(skb);
    udphdr = udp_hdr(skb);
    int saddr = 0;
	int daddr = 0;
    int sport = 0;
    int dport = 0;
	int len=0;
	int prot_num=0;
	struct dns_header *data;
	if (iphdr->protocol != IPPROTO_UDP)
		return 0;

	saddr = iphdr->saddr;
	daddr = iphdr->daddr;
    sport = htons(udphdr->source);
    dport = udphdr->dest;
	len = ntohs(udphdr->len);

	if (udp_settings.addr) {
	 	if (be32_to_cpu(saddr) != udp_settings.addr && be32_to_cpu(daddr) != udp_settings.addr)
			return 0;
	}
    unsigned long flags;
	struct udp_detail detail ={};
	detail.et_type = et_udp_detail;

	do_diag_gettimeofday(&detail.tv);
	detail.saddr = saddr;
	detail.daddr = daddr;
    detail.sport = sport;
	detail.dport = dport;
	detail.len = len;
	detail.step = PD_UDP_RCV;
	detail.prot = 0;
	searchprot(&detail,sport,dport);
	if(sport==53||dport==53)
	{
		detail.prot=DNS_PROT;
		data=(struct dns_header*)((void*)udphdr+sizeof(struct udphdr));
		//printk("%x %x\n",ntohs(data->id),ntohs(data->flags));
		if(ntohs(data->flags)&0x8000)
		 	dns_rcv_proc(detail,data);
		return 0;
	}
	diag_variant_buffer_spin_lock(&udp_variant_buffer, flags);
	diag_variant_buffer_reserve(&udp_variant_buffer, sizeof(struct udp_detail));
	diag_variant_buffer_write_nolock(&udp_variant_buffer, &detail, sizeof(struct udp_detail));
	diag_variant_buffer_seal(&udp_variant_buffer);
	diag_variant_buffer_spin_unlock(&udp_variant_buffer, flags);
	return 0;
}
static int kprobe_udp_send_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct sk_buff *skb = (void *)ORIG_PARAM1(regs);
	struct iphdr *iphdr;
    struct udphdr *udphdr;
	int saddr = 0;
	int daddr = 0;
    int sport = 0;
    int dport = 0;
	int len=0;
	int prot_num=0;
	struct dns_header *data;
	unsigned long flags;
	struct udp_detail detail;

    iphdr = ip_hdr(skb);
    udphdr = udp_hdr(skb);

	if (iphdr->protocol != IPPROTO_UDP)
		return 0;

	saddr = skb->sk->__sk_common.skc_rcv_saddr;
	daddr = skb->sk->__sk_common.skc_daddr;
	sport = skb->sk->__sk_common.skc_num;
	dport = ntohs(skb->sk->__sk_common.skc_dport);
	if (udp_settings.addr) {
	 	if (be32_to_cpu(saddr) != udp_settings.addr && be32_to_cpu(daddr) != udp_settings.addr)
			return 0;
	}
	detail.et_type = et_udp_detail;
	do_diag_gettimeofday(&detail.tv);
	detail.saddr = saddr;
	detail.daddr = daddr;
    detail.sport = sport;
	detail.dport = dport;
	detail.len = udphdr->len;

	detail.step = PD_UDP_SEND;
	detail.prot = 0;
	searchprot(&detail,sport,dport);
	if(sport==53||dport==53)
	{
		detail.prot=DNS_PROT;
		data=(struct dns_header*)((void*)udphdr+sizeof(struct udphdr));
		//printk("%x %x\n",ntohs(data->id),ntohs(data->flags));
		if(!(ntohs(data->flags)&0x8000))
			dns_send_proc(detail,data);
		return 0;
	}
	diag_variant_buffer_spin_lock(&udp_variant_buffer, flags);
	diag_variant_buffer_reserve(&udp_variant_buffer, sizeof(struct udp_detail));
	diag_variant_buffer_write_nolock(&udp_variant_buffer, &detail, sizeof(struct udp_detail));
	diag_variant_buffer_seal(&udp_variant_buffer);
	diag_variant_buffer_spin_unlock(&udp_variant_buffer, flags);
	return 0;
}
static int __activate_udp(void)
{
    
	int ret = 0;

	ret = alloc_diag_variant_buffer(&udp_variant_buffer);
	if (ret)
		goto out_variant_buffer;

	udp_alloced = 1;
    hook_kprobe(&kprobe_udp_rcv, "udp_rcv",
				kprobe_udp_rcv_pre, NULL);
	hook_kprobe(&kprobe_udp_send_skb, "udp_send_skb",
				kprobe_udp_send_pre, NULL);
    
	return 1;

out_variant_buffer:
	return 0;
}

static void __deactivate_udp(void)
{
	// unhook_kprobe(&kprobe_ip_rcv);
    unhook_kprobe(&kprobe_udp_rcv);
    unhook_kprobe(&kprobe_udp_send_skb);
    // unhook_kprobe(&kprobe_ip_send_skb);

	synchronize_sched();
	msleep(20);
	while (atomic64_read(&diag_nr_running) > 0)
		msleep(20);
}

int activate_udp(void)
{
	if (!udp_settings.activated)
		udp_settings.activated = __activate_udp();

	return udp_settings.activated;
}

int deactivate_udp(void)
{
	if (udp_settings.activated)
		__deactivate_udp();

	udp_settings.activated = 0;
	return 0;
}

int udp_syscall(struct pt_regs *regs, long id)
{
	int __user *user_ptr_len;
	size_t __user user_buf_len;
	void __user *user_buf;
	int ret = 0;
	struct diag_udp_settings settings;

	switch (id) {
	case DIAG_UDP_SET:
		user_buf = (void __user *)SYSCALL_PARAM1(regs);
		user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		if (user_buf_len != sizeof(struct diag_udp_settings)) {
			ret = -EINVAL;
		} else if (udp_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, user_buf, user_buf_len);
			if (!ret) {
				udp_settings = settings;
			}
		}
		break;
	case DIAG_UDP_SETTINGS:
		// user_buf = (void __user *)SYSCALL_PARAM1(regs);
		// user_buf_len = (size_t)SYSCALL_PARAM2(regs);

		// if (user_buf_len != sizeof(struct diag_udp_settings)) {
		// 	ret = -EINVAL;
		// } else {
		// 	settings.activated = udp_settings.activated;
		// 	settings.verbose = udp_settings.verbose;
		// 	ret = copy_to_user(user_buf, &settings, user_buf_len);
		// }
		// break;
	case DIAG_UDP_DUMP:
		user_ptr_len = (void __user *)SYSCALL_PARAM1(regs);
		user_buf = (void __user *)SYSCALL_PARAM2(regs);
		user_buf_len = (size_t)SYSCALL_PARAM3(regs);

		if (!udp_alloced) {
			ret = -EINVAL;
		} else {
			ret = copy_to_user_variant_buffer(&udp_variant_buffer,
					user_ptr_len, user_buf, user_buf_len);
			record_dump_cmd("udp");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

long diag_ioctl_udp(unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct diag_udp_settings settings;
	struct diag_ioctl_dump_param dump_param;

	switch (cmd) {
	case CMD_UDP_SET:
		if (udp_settings.activated) {
			ret = -EBUSY;
		} else {
			ret = copy_from_user(&settings, (void *)arg, sizeof(struct diag_udp_settings));
			if (!ret) {
				udp_settings = settings;
			}
		}
		break;
	case CMD_UDP_SETTINGS:
		settings = udp_settings;
		ret = copy_to_user((void *)arg, &settings, sizeof(struct diag_udp_settings));
		break;
	case CMD_UDP_DUMP:
		printk("DUMP");
		ret = copy_from_user(&dump_param, (void *)arg, sizeof(struct diag_ioctl_dump_param));
		if (!udp_alloced) {
			ret = -EINVAL;
		} else if (!ret){
			ret = copy_to_user_variant_buffer(&udp_variant_buffer,
					dump_param.user_ptr_len, dump_param.user_buf, dump_param.user_buf_len);
			record_dump_cmd("udp");
		}
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	return ret;
}

int diag_net_udp_init(void)
{
	init_diag_variant_buffer(&udp_variant_buffer, 2 * 1024 * 1024);

	if (udp_settings.activated)
		activate_udp();

	return 0;
}


void diag_net_udp_exit(void)
{
	if (udp_settings.activated)
		deactivate_udp();
	udp_settings.activated = 0;

	destroy_diag_variant_buffer(&udp_variant_buffer);
	return;
}


