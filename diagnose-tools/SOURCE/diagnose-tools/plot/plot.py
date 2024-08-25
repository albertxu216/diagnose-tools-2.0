import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import make_interp_spline

# 打开文件并读取数据
with open("/proc/load_trend", "r") as f:
    lines = f.read().splitlines()

# 将数据转换为四个列表，分别存储时间和3个时间段的负载值
time = []
load_1 = []
load_5 = []
load_15= []
for line in lines[1:]:
    t, l_1, l_5, l_15 = map(float, line.split("\t\t"))
    time.append(t)
    load_1.append(l_1)
    load_5.append(l_5)
    load_15.append(l_15)

# 将时间和负载值转换为 numpy 数组
time = np.array(time)
load_1 = np.array(load_1)
load_5 = np.array(load_5)
load_15 = np.array(load_15)

# 使用样条插值函数平滑曲线
x_new = np.linspace(time.min(), time.max(), 300)
spl_1 = make_interp_spline(time, load_1, k=3)
load_1_smooth = spl_1(x_new)
spl_5 = make_interp_spline(time, load_5, k=3)
load_5_smooth = spl_5(x_new)
spl_15 = make_interp_spline(time, load_15, k=3)
load_15_smooth = spl_15(x_new)

# 绘制曲线图
plt.plot(x_new, load_1_smooth, label='Load in 1 min')
plt.plot(x_new, load_5_smooth, label='Load in 5 min')
plt.plot(x_new, load_15_smooth, label='Load in 15 min')

# 添加图表标题和坐标轴标签
plt.title("The Trend of Load Changes")
plt.xlabel("Time")
plt.ylabel("Load")

# 显示图例
plt.legend()

# 显示图表
plt.savefig('load_trend.png')
plt.show()
