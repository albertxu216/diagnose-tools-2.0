# import pandas as pd
# import os
# import re

# # 获取当前工作目录路径
# current_dir = os.getcwd()

# # 创建一个列表来保存所有 base 文件的均值数据
# base_means = []

# # 遍历当前目录并处理 base 数据
# for file in os.listdir(current_dir):
#     if file.endswith('.csv') and 'base' in file:
#         file_path = os.path.join(current_dir, file)
        
#         # 读取 CSV 文件
#         df = pd.read_csv(file_path)
        
#         # 计算每列数据的均值
#         base_mean = df.mean(numeric_only=True)  # 计算每列的均值，只包括数值列
#         base_mean['file'] = file  # 记录文件名
        
#         # 将均值和文件名保存到列表中
#         base_means.append(base_mean)

# # 合并所有 base 均值数据
# if base_means:
#     base_means_df = pd.DataFrame(base_means)
    
#     # 保存到 base.csv 文件
#     base_file_path = os.path.join(current_dir, 'base.csv')
#     base_means_df.to_csv(base_file_path, index=False)
#     print(f"基准数据均值文件 base.csv 已保存到 {base_file_path}")
# else:
#     print("没有找到 base 数据文件。")

# import pandas as pd
# import os

# # 获取当前工作目录路径
# current_dir = os.getcwd()

# # 遍历当前目录下的所有 CSV 文件
# for file_name in os.listdir(current_dir):
#     if file_name.endswith('.csv'):  # 处理所有 CSV 文件
#         file_path = os.path.join(current_dir, file_name)
        
#         # 读取 CSV 文件
#         df = pd.read_csv(file_path)
        
#         # 检查是否有 'run' 列
#         if 'run' in df.columns:
#             run_groups = df.groupby('run')
#             diffs = []

#             # 计算每个 run 分组的每列数据的最大值和最小值之差
#             for run, group in run_groups:
#                 # 选择数值列
#                 numeric_cols = group.select_dtypes(include='number')
                
#                 # 计算最大值和最小值之差
#                 max_values = numeric_cols.max()
#                 min_values = numeric_cols.min()
#                 diff = max_values - min_values
#                 diffs.append(diff)
            
#             # 将差值列表转换为 DataFrame
#             diffs_df = pd.DataFrame(diffs)

#             # 计算每列数据的均值
#             mean_diff = diffs_df.mean()
            
#             # 创建包含差值和均值的 DataFrame
#             diffs_df = pd.concat([diffs_df, pd.DataFrame([mean_diff], columns=diffs_df.columns)], ignore_index=True)

#             # 将差值和均值 DataFrame 添加到原始 DataFrame
#             df = pd.concat([df, diffs_df], ignore_index=True)

#             # 将修改后的 DataFrame 保存回 CSV 文件
#             df.to_csv(file_path, index=False)
#             print(f"处理后的文件已保存：{file_path}")



import pandas as pd
import os
import re

# 获取当前工作目录路径
current_dir = os.getcwd()

# 定义输出文件路径
output_file_path = os.path.join(current_dir, 'summary2.csv')

# 初始化一个空的 DataFrame 来存储结果
summary2_df = pd.DataFrame()

# 遍历当前目录下的所有 CSV 文件
for file_name in os.listdir(current_dir):
    if file_name.endswith('.csv'):  # 处理所有 CSV 文件
        file_path = os.path.join(current_dir, file_name)
        
        # 读取 CSV 文件
        df = pd.read_csv(file_path)
        
        # 提取文件名中的信息
        match = re.search(r'combined_(hash_map|rbtree)_(insertV2)_num(\d+)_index(-?\d+)', file_name)
        if match:
            map_type = match.group(1)
            operation = match.group(2)
            num = match.group(3)
            
            # 获取最后一行数据
            last_row = df.iloc[-1]
            
            # 创建一个新的 DataFrame 来存储当前文件的信息
            row_df = pd.DataFrame([last_row])
            
            # 添加文件名中的信息到最后几列
            row_df['map'] = map_type
            row_df['num'] = num
            row_df['operation'] = operation
            
            # 将当前文件的信息添加到 summary2_df
            summary2_df = pd.concat([summary2_df, row_df], ignore_index=True)

# 将结果保存到新的 CSV 文件
summary2_df.to_csv(output_file_path, index=False)
print(f"汇总文件已保存：{output_file_path}")

