## resource-image
本功能用于对不同类型的进程进行进程画像，聚焦于进程对系统资源的使用情况，用于查看单个线程或进程对于系统资源的实时使用情况,包括cpu使用率、内存使用率、进程的实时io情况。

![resource_image](../../../images/resource_image.gif)

###  查看帮助信息
通过如下命令查看本功能的帮助信息：
```
diagnose-tools resource-image --help
```
结果如下：
```
----------------------------------
    resource_image usage:
        --help resource_image help info
        --activate
          verbose VERBOSE
          tgid process group monitored
          pid thread id that monitored
          comm comm that monitored
        --deactivate
        --report dump log with text.
```
### 激活功能
激活本功能的命令是：
```
diagnose-tools resource-image --activate='pid=<pid>'
diagnose-tools resource-image --activate='tgid=<tgid>'
```
在激活本功能时，可用参数为：
- `verbose` 输出级别
- `threshold` 阈值大小。

例如，下面的命令可以对进程：

`diagnose-tools resource-image --activate="tgid=5593"`
```
功能设置成功，返回值：0
    进程ID：    5593
    线程ID：    0
    进程名称：
    输出级别：  0
resource-image activated
```

如果失败，将输出：
```
call cmd c0302b00 fail, ret is -1
功能设置失败，返回值：-1
    进程ID：    5593
    线程ID：    0
    进程名称：
    输出级别：  0
```

###  查看设置参数
使用如下命令查看本功能的设置参数：
`diagnose-tools resource-image --settings`
```
功能设置：
    是否激活：√
    进程ID：5593
    线程ID：0
    进程名称：
    输出级别：0
```
### 查看结果
执行如下命令查看本功能的输出结果：
```
diagnose-tools resource-image --report
```
结果示例如下：
```
pid :5593   cpu_id: 4       CPU(%): 0.000874  MEM(%): 0.947531  READ()kb/s: 0.00          WRITE(kb/s): 0.00        
pid :5585   cpu_id: 12      CPU(%): 0.012830  MEM(%): 0.947531  READ()kb/s: 18434.88      WRITE(kb/s): 0.00        
pid :5580   cpu_id: 2       CPU(%): 0.007427  MEM(%): 0.947531  READ()kb/s: 1698.07       WRITE(kb/s): 0.00        
pid :5592   cpu_id: 1       CPU(%): 0.010407  MEM(%): 0.947531  READ()kb/s: 22726.65      WRITE(kb/s): 0.00        
pid :5596   cpu_id: 3       CPU(%): 0.000337  MEM(%): 0.947531  READ()kb/s: 0.00          WRITE(kb/s): 0.00        
pid :5583   cpu_id: 5       CPU(%): 0.007104  MEM(%): 0.947531  READ()kb/s: 0.00          WRITE(kb/s): 0.00        
pid :5600   cpu_id: 6       CPU(%): 0.012288  MEM(%): 0.947531  READ()kb/s: 0.00          WRITE(kb/s): 0.00 
```

这些数据代表指定的进程或线程对系统资源使用情况的实时调查

### 关闭功能
通过如下命令关闭本功能：
```
diagnose-tools resource-image --deactivate 
```
如果成功，将输出：
```
resource-image is not activated
```
如果失败，将输出：
```
deactivate resource-image fail, ret is -1
```
关闭功能后，本功能将不会对系统带来任何影响。
