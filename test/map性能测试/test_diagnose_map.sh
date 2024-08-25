#!/bin/bash

# 定义参数列表
NUM_VALUES=(10000000 20000000 50000000 80000000) #10万 100万 1000万 1亿
MAP_VALUES=(1 -1)
INDEX_VALUES=(-2 1)

# 获取脚本目录路径
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
CURRENT_TIME=$(date +'%Y%m%d_%H:%M:%S')

# 循环测试所有参数组合
for NUM in "${NUM_VALUES[@]}"; do
    for MAP in "${MAP_VALUES[@]}"; do
        # 判断 map 值并设置输出目录
        if [ $MAP -eq 1 ]; then
            OUTPUT_DIR="$SCRIPT_DIR/hash_map"
        elif [ $MAP -eq -1 ]; then
            OUTPUT_DIR="$SCRIPT_DIR/rbtree"
        fi
        
        # 创建输出目录（如果不存在）
        mkdir -p $OUTPUT_DIR
        
        for INDEX in "${INDEX_VALUES[@]}"; do
            # 根据 INDEX 值设置操作名称
            case $INDEX in
                1)
                    OPERATION="insertV2"
                    ;;
                2)
                    OPERATION="find"
                    ;;
                -1)
                    OPERATION="delete"
                    ;;
                -2)
                    OPERATION="baseV2"
                    ;;
                0)
                    OPERATION="all"
                    ;;
                *)
                    OPERATION="unknown"
                    ;;
            esac
            
            # 测试每种情况5次
            for i in {1..5}; do
                # 定义输出文件路径
                OUTPUT_CSV="${OUTPUT_DIR}/test_${OPERATION}_num${NUM}_map${MAP}_index${INDEX}_run${i}_${CURRENT_TIME}.csv"
                REPORT_FILE="${OUTPUT_DIR}/report_${OPERATION}_num${NUM}_map${MAP}_index${INDEX}_run${i}_${CURRENT_TIME}.txt"
                echo "开始运行diagnose测试用例时间: $(date '+%Y-%m-%d %H:%M:%S')"
                diagnose-tools install
                # 启动测试程序并后台运行
                diagnose-tools test-map --activate="num=${NUM} map=${MAP} index=${INDEX}" &
                # 获取测试程序的 PID
                TEST_PID=$!
                TEST_NAME=$(ps -p $TEST_PID -o comm=)
                echo "Test Program PID: $TEST_PID $TEST_NAME"
                
                # 启动 Glances 并导出系统资源数据到 CSV 文件
                glances --disable-plugin all --enable-plugin cpu,mem,load -t 1 -f diagnose-tools --export csv --export-csv-file $OUTPUT_CSV &
                
                # 获取 Glances 的 PID 以便稍后停止
                GLANCES_PID=$!
                
                # 等待测试用例完成
                wait $TEST_PID
                
                # 停止 Glances
                kill $GLANCES_PID
                
                # 生成报告并重定向输出到报告文件
                diagnose-tools test-map --report > $REPORT_FILE
                diagnose-tools test-map --deactivate
                diagnose-tools uninstall
                # 提示完成
                echo "Test program finished for num=${NUM}, map=${MAP}, index=${INDEX}, run ${i}. Resource usage has been logged to $OUTPUT_CSV. Report has been saved to $REPORT_FILE."
                
                # 清空系统缓存
                echo "同步并清空缓存..."
                sudo sync
                echo 3 | sudo tee /proc/sys/vm/drop_caches
                
                # 等待一段时间后再进行下一次测试
                echo "等待 5 秒后进行下一次测试..."
                sleep 5            
            done
        done
    done
done
