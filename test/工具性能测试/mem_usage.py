import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# 读取CSV文件
base_df = pd.read_csv('./csv_data/base_export.csv')
sched_image_df = pd.read_csv('./csv_data/sched-image-export.csv')
tcp_states_df = pd.read_csv('./csv_data/tcp-states-export.csv')
sys_cost_df = pd.read_csv('./csv_data/sys-cost-export.csv')
migrate_image_df = pd.read_csv('./csv_data/migrate-image-export.csv')
resource_image_df = pd.read_csv('./csv_data/resource-image-export.csv')
udp_df = pd.read_csv('./csv_data/udp-export.csv')
keytime_df = pd.read_csv('./csv_data/keytime-export.csv')

# 选择感兴趣的内存指标列
mem_columns = ['mem_used', 'mem_active', 'mem_buffers']

# 计算这些指标的平均值
base_avg = base_df[mem_columns].mean().values
sched_image_avg = sched_image_df[mem_columns].mean().values
tcp_states_avg = tcp_states_df[mem_columns].mean().values
sys_cost_avg = sys_cost_df[mem_columns].mean().values
migrate_image_avg = migrate_image_df[mem_columns].mean().values
resource_image_avg = resource_image_df[mem_columns].mean().values
udp_avg = udp_df[mem_columns].mean().values
keytime_avg = keytime_df[mem_columns].mean().values

# 转换字节到GB
bytes_to_gb = 1024 ** 3
base_avg_gb = base_avg / bytes_to_gb
sched_image_avg_gb = sched_image_avg / bytes_to_gb
tcp_states_avg_gb = tcp_states_avg / bytes_to_gb
sys_cost_avg_gb = sys_cost_avg / bytes_to_gb
migrate_image_avg_gb = migrate_image_avg / bytes_to_gb
resource_image_avg_gb = resource_image_avg / bytes_to_gb
udp_avg_gb = udp_avg / bytes_to_gb
keytime_avg_gb = keytime_avg / bytes_to_gb

# 计算各个方法减去 base 的差值
sched_image_diff = sched_image_avg_gb - base_avg_gb
tcp_states_diff = tcp_states_avg_gb - base_avg_gb
sys_cost_diff = sys_cost_avg_gb - base_avg_gb
migrate_image_diff = migrate_image_avg_gb - base_avg_gb
resource_image_diff = resource_image_avg_gb - base_avg_gb
udp_diff = udp_avg_gb - base_avg_gb
keytime_diff = keytime_avg_gb - base_avg_gb

# 绘制柱状图
fig, ax = plt.subplots(figsize=(16, 8))  # 调整图表宽度

# 设置每组柱子的宽度和位置
bar_width = 0.1  # 调整柱子宽度
bar_positions_base = np.arange(len(mem_columns))
bar_positions_sched = bar_positions_base + bar_width
bar_positions_tcp = bar_positions_sched + bar_width
bar_positions_sys_cost = bar_positions_tcp + bar_width
bar_positions_migrate_image = bar_positions_sys_cost + bar_width
bar_positions_resource_image = bar_positions_migrate_image + bar_width
bar_positions_udp = bar_positions_resource_image + bar_width
bar_positions_keytime = bar_positions_udp + bar_width

# 修改柱状图颜色
base_color = '#1f77b4'  # 深蓝色
sched_image_color = '#2ca02c'  # 深绿色
tcp_states_color = '#ff7f0e'  # 深橙色
sys_cost_color = '#9467bd'  # 深紫色
migrate_image_color = '#d62728'  # 深红色
resource_image_color = '#8c564b'  # 深棕色
udp_color = '#7f7f7f'  # 深灰色
keytime_color = '#17becf'  # 深青色

# 绘制每个方法的柱子，并标出柱子上的数值和差值
base_bars = ax.bar(bar_positions_base, base_avg_gb, width=bar_width, label='base', color=base_color)
sched_image_bars = ax.bar(bar_positions_sched, sched_image_avg_gb, width=bar_width, label='sched_image', color=sched_image_color)
tcp_states_bars = ax.bar(bar_positions_tcp, tcp_states_avg_gb, width=bar_width, label='tcp_states', color=tcp_states_color)
sys_cost_bars = ax.bar(bar_positions_sys_cost, sys_cost_avg_gb, width=bar_width, label='sys_cost', color=sys_cost_color)
migrate_image_bars = ax.bar(bar_positions_migrate_image, migrate_image_avg_gb, width=bar_width, label='migrate_image', color=migrate_image_color)
resource_image_bars = ax.bar(bar_positions_resource_image, resource_image_avg_gb, width=bar_width, label='resource_image', color=resource_image_color)
udp_bars = ax.bar(bar_positions_udp, udp_avg_gb, width=bar_width, label='udp', color=udp_color)
keytime_bars = ax.bar(bar_positions_keytime, keytime_avg_gb, width=bar_width, label='keytime', color=keytime_color)

# 标出 base 柱子上的数值
for bar in base_bars:
    height = bar.get_height()
    ax.annotate(f'{height:.3f}', xy=(bar.get_x() + bar.get_width() / 2, height), xytext=(0, 3), textcoords="offset points", ha='center', va='bottom', color='black')

# 标出 sched_image、tcp_states、sys_cost、migrate_image、resource_image、udp 和 keytime 柱子上的数值和差值
for bars, diffs in zip([sched_image_bars, tcp_states_bars, sys_cost_bars, migrate_image_bars, resource_image_bars, udp_bars, keytime_bars], 
                       [sched_image_diff, tcp_states_diff, sys_cost_diff, migrate_image_diff, resource_image_diff, udp_diff, keytime_diff]):
    for bar, diff in zip(bars, diffs):
        height = bar.get_height()
        ax.annotate(f'{height:.3f}', xy=(bar.get_x() + bar.get_width() / 2, height), xytext=(0, 3), textcoords="offset points", ha='center', va='bottom')
        ax.text(bar.get_x() + bar.get_width() / 2, height / 2, f'{diff:.3f}', ha='center', va='bottom', color='black')

# 设置图表标题和标签
ax.set_title('Memory Usage Comparison (GB)')
ax.set_xlabel('Memory Metrics')
ax.set_ylabel('Memory Usage (GB)')
ax.set_xticks(bar_positions_base + 3.5 * bar_width)  # 设置横坐标在中间位置
ax.set_xticklabels(mem_columns)
ax.legend(title='Methods')

# 保存图表到本地文件
plt.savefig('memory_usage_comparison_gb.png')

# 显示图表
plt.show()
