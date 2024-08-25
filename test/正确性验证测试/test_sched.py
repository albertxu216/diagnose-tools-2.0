import pandas as pd
import re

# 从CSV文件读取数据
df = pd.read_csv('sched_test_data.csv')

# 去除列名中的多余空格
df.columns = df.columns.str.strip()

# 提取事件类型
def extract_event_type(details):
    if 'sched_wakeup_new' in details:
        return 'sched_wakeup_new'
    elif 'sched_wakeup' in details:
        return 'sched_wakeup'
    elif 'sched_switch' in details:
        return 'sched_switch'
    elif 'sched_process_exit' in details:
        return 'sched_process_exit'
    return 'unknown'

# 从details列中提取tid
def extract_tid_from_details(details):
    match = re.search(r'tid = (\d+)', details)
    return int(match.group(1)) if match else None

# 从details列中提取prev_tid
def extract_prev_tid(details):
    match = re.search(r'prev_tid = (\d+)', details)
    return int(match.group(1)) if match else None

# 从details列中提取next_tid
def extract_next_tid(details):
    match = re.search(r'next_tid = (\d+)', details)
    return int(match.group(1)) if match else None

# 从details列中提取prev_state
def extract_prev_state(details):
    match = re.search(r'prev_state = (\d+)', details)
    return int(match.group(1)) if match else None

# # 转换时间戳
# def convert_timestamp(timestamp):
#     return float(re.sub(r'[^\d.]', '', timestamp))

def convert_timestamp(timestamp):
    # 移除时间戳中的非数字和非点字符
    timestamp = re.sub(r'[^\d:]', '', timestamp)
    
    # 查找小时、分钟、秒和纳秒部分
    match = re.match(r'(\d+):(\d+):(\d+).(\d+)', timestamp)
    if match:
        hours, minutes, seconds, nanoseconds = match.groups()
        # 转换为整数
        hours = int(hours)
        minutes = int(minutes)
        seconds = int(seconds)
        nanoseconds = int(nanoseconds.ljust(9, '0')[:9])  # 确保纳秒部分为9位数
        
        # 计算总纳秒数
        total_ns = (minutes * 60 + seconds) * 1e9 + nanoseconds
        print(total_ns)
        return total_ns
    else:
        raise ValueError(f"无法解析时间戳: {timestamp}")

# 创建一个用于存储准备调度事件的字典
current_scheduling = {}

# 列表用于存储调度延迟
delays = []

# 处理每一行数据
for index, row in df.iterrows():
    event = extract_event_type(row['details'])
    timestamp = convert_timestamp(row['timestamp'])
    tid = extract_tid_from_details(row['details'])

    if event == 'sched_wakeup_new' or event == 'sched_wakeup':
        if tid is not None:
            current_scheduling[tid] = {
                'ready_time': timestamp,
                'scheduled_time': None
            }
    
    elif event == 'sched_switch':
        prev_tid = extract_prev_tid(row['details'])
        next_tid = extract_next_tid(row['details'])
        prev_state = extract_prev_state(row['details'])
        
        # 如果 prev_state == 0，无论 prev_tid 是否在 current_scheduling 中，都记录其调度时间
        if prev_state == 0:
            current_scheduling[prev_tid] = current_scheduling.get(prev_tid, {})
            current_scheduling[prev_tid]['ready_time'] = timestamp
        
        if next_tid in current_scheduling:
                delay = timestamp - current_scheduling[next_tid]['ready_time']
                delays.append({
                    'tid': next_tid,
                    'delay': delay/10000000000000
                })
                del current_scheduling[next_tid]
    
    elif event == 'sched_process_exit':
        if tid in current_scheduling:
            del current_scheduling[tid]

# 逐条打印 delays 列表中的数据
# print("Delays 列表数据:")
# for delay in delays:
#     print(delay)

# 创建 DataFrame
delays_df = pd.DataFrame(delays)

# # 打印 DataFrame 和列名
# print("Delays DataFrame:")
# print(delays_df.head())
# print("DataFrame 列名:", delays_df.columns)

# 计算最大延迟、最小延迟和平均延迟
thread_stats = delays_df.groupby('tid').agg(
    avg_delay=('delay', 'mean'),
    max_delay=('delay', 'max'),
    min_delay=('delay', 'min')
)

print("线程统计数据:")
print(thread_stats)
