import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

# 读取 summary.csv 文件
summary_df = pd.read_csv('summary.csv')

# 对表格进行排序
summary_df = summary_df.sort_values(by=['num', 'map', 'operation'])

# 定义要关注的数据列
data_columns = ['cpu_user', 'cpu_system', 'mem_percent', 'mem_used', 'mem_cached', 'mem_available', 'load_min1', 'load_min5', 'cpu_total']

# 提取 hash_map 和 rbtree 的数据
hash_map_df = summary_df[summary_df['map'] == 'hash_map']
rbtree_df = summary_df[summary_df['map'] == 'rbtree']

# 按 num 和 operation 进行分组
grouped_hash_map = hash_map_df.groupby(['num', 'operation'])
grouped_rbtree = rbtree_df.groupby(['num', 'operation'])

# 初始化图表
fig, axs = plt.subplots(len(data_columns), figsize=(15, 25))
fig.suptitle('Hash Map vs RBTree Comparison')

# 对每个数据列进行对比并绘制图表
for i, col in enumerate(data_columns):
    hash_map_values = []
    rbtree_values = []
    labels = []

    for (num, operation), hash_group in grouped_hash_map:
        if (num, operation) in grouped_rbtree.groups:
            rbtree_group = grouped_rbtree.get_group((num, operation))

            # 计算均值
            hash_map_mean = hash_group[col].mean()
            rbtree_mean = rbtree_group[col].mean()

            hash_map_values.append(hash_map_mean)
            rbtree_values.append(rbtree_mean)
            labels.append(f'{num}_{operation}')

    # 使用 seaborn 绘制对比图
    ax = axs[i]
    sns.lineplot(x=labels, y=hash_map_values, label='Hash Map', marker='o', linestyle='-', ax=ax)
    sns.lineplot(x=labels, y=rbtree_values, label='RBTree', marker='x', linestyle='--', ax=ax)
    ax.set_title(col)
    ax.set_ylabel(col)
    ax.legend()
    ax.grid(True)

# 设置 x 轴标签和图例
for ax in axs:
    ax.set_xlabel('Num_Operation')
    ax.set_xticks(range(len(labels)))
    ax.set_xticklabels(labels, rotation=45, ha='right')

# 调整布局和显示图表
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()
