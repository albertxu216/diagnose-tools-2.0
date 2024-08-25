#!/bin/bash

# 遍历当前目录中的所有文件
for file in *; do
    # 检查文件名是否包含时间戳模式
    if [[ $file =~ _[0-9]{8}_[0-9]{2}:[0-9]{2}:[0-9]{2}\. ]]; then
        # 删除时间戳部分
        new_name=$(echo "$file" | sed -E 's/_[0-9]{8}_[0-9]{2}:[0-9]{2}:[0-9]{2}\././')
        # 重命名文件
        mv "$file" "$new_name"
    fi
done
