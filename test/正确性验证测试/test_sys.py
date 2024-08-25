import pandas as pd

# 读取CSV文件，确保路径中没有多余空格
df = pd.read_csv('/home/xhb/proc_image2.0/test/lttng_sys_test_data.csv', delimiter=',', skipinitialspace=True)

# 初始化字典存储数据
syscall_start_times = {}
syscall_durations = {}

# 处理每一行数据以计算系统调用时延
for index, row in df.iterrows():
    details = row['details']
    timestamp = pd.to_datetime(row['timestamp'].strip('[]'), format='%H:%M:%S.%f')
    
    if 'syscall_entry_' in details:
        syscall_name = details.split('syscall_entry_')[1].split(':')[0]
        syscall_start_times[(row['pid'], syscall_name)] = timestamp
    elif 'syscall_exit_' in details:
        syscall_name = details.split('syscall_exit_')[1].split(':')[0]
        start_time = syscall_start_times.pop((row['pid'], syscall_name), None)
        if start_time:
            duration_ns = (timestamp - start_time).total_seconds() * 1e9  # 转换为纳秒
            if syscall_name not in syscall_durations:
                syscall_durations[syscall_name] = []
            syscall_durations[syscall_name].append(duration_ns)

# 计算每个系统调用的最大、最小、平均时延以及调用次数
syscall_stats = []
all_durations = []
for syscall_name, durations in syscall_durations.items():
    max_duration = max(durations)
    min_duration = min(durations)
    avg_duration = sum(durations) / len(durations)
    count = len(durations)
    syscall_stats.append([syscall_name, max_duration, min_duration, avg_duration, count])
    all_durations.extend(durations)

# 转换为DataFrame
stats_df = pd.DataFrame(syscall_stats, columns=['syscall_name', 'max_duration_ns', 'min_duration_ns', 'avg_duration_ns', 'count'])

# 找到次数最多的前三个系统调用
top_syscalls = stats_df.nlargest(3, 'count')

# 计算所有系统调用的最大、最小以及平均延迟
overall_max_duration = max(all_durations)
overall_min_duration = min(all_durations)
overall_avg_duration = sum(all_durations) / len(all_durations)

# 输出结果
print("所有系统调用统计:")
print(stats_df)
print("\n调用次数最多的前三个系统调用:")
print(top_syscalls)
print(f"\n所有系统调用的最大延迟: {overall_max_duration} ns")
print(f"所有系统调用的最小延迟: {overall_min_duration} ns")
print(f"所有系统调用的平均延迟: {overall_avg_duration} ns")

# 保存到CSV文件
stats_df.to_csv('syscall_stats.csv', index=False)
top_syscalls.to_csv('top_syscalls.csv', index=False)
