#ifndef UAPI_UDP_H
#define UAPI_UDP_H

#include <linux/ioctl.h>

#define DIAG_UDP_SET (DIAG_BASE_SYSCALL_UDP)
#define DIAG_UDP_SETTINGS (DIAG_UDP_SET + 1)
#define DIAG_UDP_DUMP (DIAG_UDP_SETTINGS + 1)

#ifndef __maybe_unused
# define __maybe_unused		__attribute__((unused))
#endif
int udp_syscall(struct pt_regs *regs, long id);

enum udp_protocol
{
	PD_NONE,
    PD_DNS,
	PD_QUIC,
	PD_SSDP,
	PD_NTP,
	PD_MDNS,
	PD_NFS,
	PD_SNMP,
	PD_TFTP,
	PD_DHCP,
	PD_RIP,
	PD_PROT_COUNT,
};
__maybe_unused static const char *udp_prot_str[PD_PROT_COUNT] = {
	"UNKNOWN",
    "DNS",
	"QUIC",
	"SSDP",
	"NTP",
	"MDNS",
	"NFS",
	"SNMP",
	"TFTP",
	"DHCP",
	"RIP",
};
enum udp_packet_step
{
    PD_UDP_RCV,
    PD_UDP_SEND,
	PD_TRACK_COUNT,
};
__maybe_unused static const char *udp_packet_steps_str[PD_TRACK_COUNT] = {
	"UDP_RCV",
    "UDP_SEND"
};
struct diag_udp_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int addr;

};

struct udp_detail {
	int et_type;
	struct diag_timespec tv;
	unsigned int saddr;
	unsigned int daddr;
    __u16 sport;
    __u16 dport;
	int len;
    enum udp_packet_step step;
	enum udp_protocol prot;
};

struct udp_dns_rcv {
	struct udp_detail comm;
    __u16 qdcount; // 问题部分计数
    __u16 ancount; // 应答记录计数
	char question[256];
	unsigned int answer_addr;
};

struct udp_dns_send {
	struct udp_detail comm;
    __u16 qdcount; // 问题部分计数
	char question[256];
};
struct dns_header {
    __u16 id;      // 事务ID
    __u16 flags;   // 标志字段
    __u16 qdcount; // 问题部分计数
    __u16 ancount; // 应答记录计数
    __u16 nscount; // 授权记录计数
    __u16 arcount; // 附加记录计数
};

// struct dns_question_data {
//     char* data; //4字节
// 	__u16 type;
// 	__u16 que_class;
// };

// struct dns_answer_data {
// 	__u16 name;
// 	__u16 type;
// 	__u16 ans_class;
// 	__u32 timetolive;
// 	__u16 datalen;
// 	__u32 addr;
// };
#define CMD_UDP_SET (0)
#define CMD_UDP_SETTINGS (CMD_UDP_SET + 1)
#define CMD_UDP_DUMP (CMD_UDP_SETTINGS + 1)
#define DIAG_IOCTL_UDP_SET _IOWR(DIAG_IOCTL_TYPE_UDP, CMD_UDP_SET, struct diag_udp_settings)
#define DIAG_IOCTL_UDP_SETTINGS _IOWR(DIAG_IOCTL_TYPE_UDP, CMD_UDP_SETTINGS, struct diag_udp_settings)
#define DIAG_IOCTL_UDP_DUMP _IOWR(DIAG_IOCTL_TYPE_UDP, CMD_UDP_DUMP, struct diag_ioctl_dump_param)

#endif /* UAPI_UDP_H */
