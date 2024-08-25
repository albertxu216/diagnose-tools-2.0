#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# 指定输出目录
OUTPUT_DIR="$SCRIPT_DIR/lttng-traces/sched_test_$(date +'%Y%m%d_%H:%M:%S')"
CSV_FILE="$OUTPUT_DIR/sched_test_data.csv"
mkdir -p "$SCRIPT_DIR/ebpf/ebpf_output_$(date +'%Y%m%d_%H:%M:%S')"
EBPF_OUTPUT_DIR="$SCRIPT_DIR/ebpf/ebpf_output_$(date +'%Y%m%d_%H:%M:%S')"

# 获取目标进程的 PID
TARGET_PID=$(pidof test_proc_image)

if [ -z "$TARGET_PID" ]; then
    echo "目标进程未运行，请先启动目标进程。"
    exit 1
fi
echo "测试程序 PID: $TARGET_PID"

# 创建会话并指定输出目录
sudo lttng create test_sched --output=$OUTPUT_DIR

# 启用内核事件，仅针对特定 PID
sudo lttng enable-event --kernel sched_switch,sched_wakeup,sched_wakeup_new,sched_process_exit

# 添加上下文信息
sudo lttng add-context --kernel --type pid

sudo lttng track --kernel --pid $TARGET_PID

# 运行proc_image监测工具
cd ../CPU_Subsystem/eBPF_proc_image/
sudo ./proc_image -S > $EBPF_OUTPUT_DIR/ebpf_sched.log &
PROC_IMAGE_PID=$!
sleep 1
sudo ./controller -S  -P $TARGET_PID -a
# 启动会话
sudo lttng start
read

sudo lttng stop 
sudo lttng destroy
echo "追踪数据已保存到 $OUTPUT_DIR 目录中"
# 将 LTTng 跟踪数据转换为文本格式
babeltrace2 $OUTPUT_DIR > $OUTPUT_DIR/trace_data.txt

# 解析文本格式并导出为 CSV 文件
echo "timestamp, event, pid, cpu, details" > $CSV_FILE
awk '
BEGIN {
    FS=" "
}
{
    timestamp = $1
    event = $3
    pid = $6
    cpu = $8
    details = $0
    gsub(/,/, " ", details)  # 替换 details 中的逗号以避免 CSV 格式问题
    printf "%s, %s, %s, %s, \"%s\"\n", timestamp, event, pid, cpu, details >> "'"$CSV_FILE"'"
}
' $OUTPUT_DIR/trace_data.txt
echo "lttng、eBPF 程序数据收集完毕，请 Ctrl+C 结束"
read
sudo chown -R xhb:xhb ./
echo "数据已导出到 $CSV_FILE"