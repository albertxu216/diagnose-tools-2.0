#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# 指定输出目录
OUTPUT_DIR="$SCRIPT_DIR/sys_test_lttng_$(date +'%Y%m%d_%H_%M_%S')"
LTTNG_CSV_FILE="$OUTPUT_DIR/lttng_sys_test_data.csv"
EBPF_CSV_FILE="$OUTPUT_DIR/ebpf_sys_test_data.csv"
mapping_file="$SCRIPT_DIR/syscall_mapping.txt"
# # mkdir -p "$SCRIPT_DIR/sys_test_ebpf_$(date +'%Y%m%d_%H_%M_%S')"
# EBPF_OUTPUT_DIR="$SCRIPT_DIR/sys_test_ebpf_$(date +'%Y%m%d_%H_%M_%S')"
# 定义表头
header="TIME,TGID,PID,1st/num,2nd/num,3nd/num,P_AVG_DELAY(ns),S_AVG_DELAY(ns),P_MAX_DELAY(ns),S_MAX_DELAY(ns),P_MIN_DELAY(ns),S_MIN_DELAY(ns),SYSCALLS"

# 获取目标进程的 PID
TARGET_PID=$(pidof test_proc_image)

if [ -z "$TARGET_PID" ]; then
    echo "目标进程未运行，请先启动目标进程。"
    exit 1
fi
echo "测试程序 PID: $TARGET_PID"

# 创建会话并指定输出目录
sudo lttng create test_sys --output=$OUTPUT_DIR

# 启用内核事件，仅针对特定 PID
lttng enable-event -k --syscall --all

# 添加上下文信息
sudo lttng add-context --kernel --type pid

sudo lttng track --kernel --pid $TARGET_PID

# 运行proc_image监测工具
cd ../CPU_Subsystem/eBPF_proc_image/
sudo ./proc_image -s > $SCRIPT_DIR/ebpf_sys.log &
PROC_IMAGE_PID=$!
sleep 1
sudo ./controller -s 5  -P $TARGET_PID -a
# 启动会话
sudo lttng start
read

sudo lttng stop 
sudo lttng destroy
echo "追踪数据已保存到 $OUTPUT_DIR 目录中"
# 将 LTTng 跟踪数据转换为文本格式
babeltrace2 $OUTPUT_DIR > $OUTPUT_DIR/trace_data.txt
sudo ./controller -s 20  -P $TARGET_PID -d
sudo ./controller -s 20  -P $TARGET_PID -f

sudo chown -R xhb:xhb $SCRIPT_DIR
# 解析文本格式并导出为 CSV 文件
echo "timestamp, event, pid, cpu, details" > $LTTNG_CSV_FILE
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
    printf "%s, %s, %s, %s, \"%s\"\n", timestamp, event, pid, cpu, details >> "'"$LTTNG_CSV_FILE"'"
}
' $OUTPUT_DIR/trace_data.txt

# 读取系统调用映射到数组
declare -A syscall_map
while IFS=, read -r number name; do
    syscall_map[$number]=$name
    echo "Loaded mapping: $number -> $name"
done < "$mapping_file"

# 检查是否正确读取到数组中
echo "System call mapping loaded:"
for number in "${!syscall_map[@]}"; do
    echo "$number: ${syscall_map[$number]}"
done

# 创建输出文件并写入表头
echo "$header" > "$EBPF_CSV_FILE"
# 处理每一行日志
tail -n +3 "$SCRIPT_DIR/ebpf_sys.log" | while IFS= read -r line; do
    # 使用正则表达式提取各个部分
    if [[ $line =~ ([0-9]{2}:[0-9]{2}:[0-9]{2})[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]*/[[:space:]]*([0-9]+)[[:space:]]+([0-9]+)[[:space:]]*/[[:space:]]*([0-9]+)[[:space:]]+([0-9]+)[[:space:]]*/[[:space:]]*([0-9]+)[[:space:]]+\|[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+\|[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+\|[[:space:]]+([0-9]+)[[:space:]]+([0-9]+)[[:space:]]+\|[[:space:]]+(.+) ]]; then
        time="${BASH_REMATCH[1]}"
        tgid="${BASH_REMATCH[2]}"
        pid="${BASH_REMATCH[3]}"
        first_num="${BASH_REMATCH[4]}"
        first_count="${BASH_REMATCH[5]}"
        second_num="${BASH_REMATCH[6]}"
        second_count="${BASH_REMATCH[7]}"
        third_num="${BASH_REMATCH[8]}"
        third_count="${BASH_REMATCH[9]}"
        p_avg_delay="${BASH_REMATCH[10]}"
        s_avg_delay="${BASH_REMATCH[11]}"
        p_max_delay="${BASH_REMATCH[12]}"
        s_max_delay="${BASH_REMATCH[13]}"
        p_min_delay="${BASH_REMATCH[14]}"
        s_min_delay="${BASH_REMATCH[15]}"
        syscalls="${BASH_REMATCH[16]}"
        # 替换系统调用号为系统调用名
        # converted_syscalls=$(echo "$syscalls" | sed -r "s/([0-9]+)([[:space:]]*,|[[:space:]]*|$)/${syscall_map[\1]}\2/g")
        # 使用awk替换系统调用号为系统调用名
        # , arr[6], arr[7], arr[8], arr[9], arr[10], arr[11], arr[12], arr[13], arr[14], arr[15], arr[16], arr[17], arr[18], arr[19], arr[20];
        converted_syscalls=$(echo "$syscalls" | awk -v mapfile="${mapping_file}" '
        BEGIN {
            while ((getline < mapfile) > 0) {
                split($0, a, ",");
                syscall_map[a[1]] = a[2];
            }
        }
        {
            split($0, arr, ",");
            for (i in arr) {
                if (arr[i] in syscall_map) {
                    arr[i] = syscall_map[arr[i]];
                }
            }
            OFS = ",";
            print arr[1], arr[2], arr[3], arr[4], arr[5]
        }')
        # 添加调试输出
        echo "Original syscalls: $syscalls"
        echo "Converted syscalls: $converted_syscalls"

        # 将提取的数据写入到 CSV 文件中
        echo "$time,$tgid,$pid,$first_num/$first_count,$second_num/$second_count,$third_num/$third_count,$p_avg_delay,$s_avg_delay,$p_max_delay,$s_max_delay,$p_min_delay,$s_min_delay,$converted_syscalls" >> "$EBPF_CSV_FILE"
    else
        echo "Line not matched: $line"
    fi
done 
echo "lttng、eBPF 程序数据收集完毕，请 Ctrl+C 结束"
read

echo "数据已导出到 $LTTNG_CSV_FILE"