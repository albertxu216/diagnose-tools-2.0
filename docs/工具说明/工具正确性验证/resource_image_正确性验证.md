# resource_image 正确性验证
## 0.准备工作：

学习使用LTTng 来监测数据;

学习将LTTng生成的文件在compass上跑出来,并分析;

### 0.1.LTTng学习使用:

[linux下 LTTng使用详细说明_no package 'liburcu' found-CSDN博客](https://blog.csdn.net/mao_hui_fei/article/details/120654095)

```c
lttng create my-session
lttng enable-event --kernel sched_switch
lttng add-context --kernel --type pid --type tid
lttng track --kernel --pid <pid>
lttng track --kernel --tid <tid>
lttng start
# 运行你想监控的进程或线程
lttng stop
lttng view
lttng destroy
```

### 0.2.trace compass
`
[Trace Compass User Guide - LTTng Kernel Analysis (eclipse.org)](https://archive.eclipse.org/tracecompass/doc/stable/org.eclipse.tracecompass.doc.user/LTTng-Kernel-Analysis.html#Control_Flow_View)

![1720660193068](./images/1720660193068.png)


## 1.resource_image

resource_image:是一款致力于研究进程对试实时系统资源利用情况的监测工具，涉及到当前进程的cpu利用率、mem使用率、读写字节数等。该功能通过使用kprobe钩子函数挂载到`finish_task_switch`内核函数上，旨在追踪任务切换时的资源使用情况；

本次测试以及数据关联工作分为三部分：

- resource_image工具逻辑正确性的测试；
- resource_image工具数据正确性的测试；
- resource_image工具数据的关联关系，以及原理梳理；

### 1.1 测试用例test_resource

正在完善中...