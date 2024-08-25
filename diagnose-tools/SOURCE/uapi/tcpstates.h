#ifndef UAPI_TCP_STATES_H
#define UAPI_TCP_STATES_H

#include <linux/ioctl.h>

#define DIAG_TCP_STATES_SET (DIAG_BASE_SYSCALL_TCP_STATES)
#define DIAG_TCP_STATES_SETTINGS (DIAG_TCP_STATES_SET + 1)
#define DIAG_TCP_STATES_DUMP (DIAG_TCP_STATES_SETTINGS + 1)

#ifndef __maybe_unused
# define __maybe_unused		__attribute__((unused))
#endif
int tcp_states_syscall(struct pt_regs *regs, long id);
__maybe_unused static const char *tcp_states[] = {
	"NULL",
	"ESTABLISHED",
	"SYN_SENT",
	"SYN_RECV",
	"FIN_WAIT1",
	"FIN_WAIT2",
	"TIME_WAIT",
	"CLOSE",
	"CLOSE_WAIT",
	"LAST_ACK",
	"LISTEN",
	"CLOSING",
	"NEW_SYN_RECV",
	"UNKNOWN",
	"",
};
enum {
    TCP_STATE_NULL,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECV,
    TCP_STATE_FIN_WAIT1,
    TCP_STATE_FIN_WAIT2,
    TCP_STATE_TIME_WAIT,
    TCP_STATE_CLOSE,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_LISTEN,
    TCP_STATE_CLOSING,
    TCP_STATE_NEW_SYN_RECV,
    TCP_STATE_UNKNOWN,
    TCP_STATE_EMPTY, // 对应最后的空字符串
} ;
struct diag_tcp_states_settings {
	unsigned int activated;
	unsigned int verbose;
	unsigned int addr;

};

struct tcp_states_detail {
	int et_type;
	struct diag_timespec tv;
	unsigned int saddr;
	unsigned int daddr;
    __u16 sport;
    __u16 dport;
    int oldstate;
    int newstate;
	__u64 time;                          
    __u32 bytes_acked;                
    __u32 bytes_sent;                
    __u32 bytes_received;     
    __u32 snd_cwnd;                      
    __u32 rcv_wnd;                                  
    __u32 total_retrans;      
    __u32 sndbuf;
	__u32 rcvbuf;                       
    __u32 sk_wmem_queued;          
    __u32 tcp_backlog;             
    __u32 max_tcp_backlog;
};
struct tcp_states_total {
	int et_type;
	int state_count[TCP_STATE_EMPTY];
};
#define CMD_TCP_STATES_SET (0)
#define CMD_TCP_STATES_SETTINGS (CMD_TCP_STATES_SET + 1)
#define CMD_TCP_STATES_DUMP (CMD_TCP_STATES_SETTINGS + 1)
#define DIAG_IOCTL_TCP_STATES_SET _IOWR(DIAG_IOCTL_TYPE_TCP_STATES, CMD_TCP_STATES_SET, struct diag_tcp_states_settings)
#define DIAG_IOCTL_TCP_STATES_SETTINGS _IOWR(DIAG_IOCTL_TYPE_TCP_STATES, CMD_TCP_STATES_SETTINGS, struct diag_tcp_states_settings)
#define DIAG_IOCTL_TCP_STATES_DUMP _IOWR(DIAG_IOCTL_TYPE_TCP_STATES, CMD_TCP_STATES_DUMP, struct diag_ioctl_dump_param)

#endif /* UAPI_TCP_STATES_H */
