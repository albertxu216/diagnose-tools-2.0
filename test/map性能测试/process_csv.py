import pandas as pd
import os
import re

# 定义操作类型和文件夹
operation_types = ['insertV2']
folders = ['hash_map', 'rbtree']

# 获取当前工作目录路径
current_dir = os.getcwd()

# 创建一个字典来保存每类操作的 DataFrame
dataframes = {folder: {op: {} for op in operation_types} for folder in folders}

# 遍历文件夹并按类型分类CSV文件
for folder in folders:
    folder_path = os.path.join(current_dir, folder)
    if os.path.exists(folder_path):
        for file in os.listdir(folder_path):
            if file.endswith('.csv'):
                file_path = os.path.join(folder_path, file)
                
                # 根据文件名识别操作类型、样本大小
                match = re.search(r'test_(insertV2)_num(\d+)_map(-?\d+)_index(-?\d+)_run(\d+)', file)
                if match:
                    operation = match.group(1)
                    num = int(match.group(2))
                    map_value = int(match.group(3))
                    index = int(match.group(4))
                    run = int(match.group(5))
                    map_type = 'hash_map' if map_value == 1 else 'rbtree'
                    
                    df = pd.read_csv(file_path)
                    # 添加标记信息
                    df['num'] = num
                    df['map'] = map_type
                    df['index'] = index
                    df['run'] = run
                    
                    # 创建唯一键（map_type, operation, num, index）
                    key = (map_type, operation, num, index)
                    
                    if key not in dataframes[map_type][operation]:
                        dataframes[map_type][operation][key] = []
                    
                    dataframes[map_type][operation][key].append(df)

# 合并并保存每类操作的CSV文件
for map_type in folders:
    for operation in operation_types:
        for key, df_list in dataframes[map_type][operation].items():
            if df_list:
                combined_df = pd.concat(df_list, ignore_index=True)
                map_type, operation, num, index = key
                combined_file_path = os.path.join(current_dir, f'combined_{map_type}_{operation}_num{num}_index{index}.csv')
                combined_df.to_csv(combined_file_path, index=False)
                print(f"合并后的 {map_type} {operation} num={num} index={index} 文件已保存到 {combined_file_path}")
