## migrate-image
本功能用于对不同类型的进程进行进程画像，聚焦于进程核间迁移情况，可以查看单个线程或进程发生核间迁移时的情况,分析源cpu、目的cpu以及当前调度实体的量化负载、所需算力情况。

![migrate_image](../../../images/migrate_image.gif)

###  查看帮助信息
通过如下命令查看本功能的帮助信息：
```
diagnose-tools migrate-image --help
```
结果如下：
```
----------------------------------
    migrate_image usage:
        --help migrate_image help info
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
diagnose-tools migrate-image --activate='pid=<pid>'
diagnose-tools migrate-image --activate='tgid=<tgid>'
```
在激活本功能时，可用参数为：

- pid 跟踪线程
- tgid 跟踪进程

例如，下面的命令可以将跟踪进程3907的核间迁移情况：
`diagnose-tools migrate-image --activate="tgid=3907"`
```
功能设置：
    是否激活：√
    进程ID：3907
    线程ID：0
    进程名称：
    输出级别：0
```
如果失败，将输出：
```
call cmd c0302b00 fail, ret is -1
功能设置失败，返回值：-1
    进程ID：    3907
    线程ID：    0
    进程名称：
    输出级别：  0
```

###  查看设置参数
使用如下命令查看本功能的设置参数：

`diagnose-tools migrate-image --settings`
```
功能设置：
    是否激活：√
    进程ID：3907
    线程ID：0
    进程名称：
    输出级别：0
```
### 查看结果
执行如下命令查看本功能的输出结果：
```
diagnose-tools migrate-image --report
```
结果示例如下：
```
-----------------MIGRATE_IMAGE---------------------
PID: 3921        Time :[1721714252:387264]
PRIO: 120        COUNT: 4        REAR : 0
Time_Stamp       ocpu       dcpu       PLoad_AVG    Putil_AVG    MEM_USAGE    READ         WRITE          
5103508803552    2          15         0            0            2002304      0            0              
5103604242764    15         14         0            0            2002308      0            0              
5103732312105    9          8          13           13           2002412      0            0  
```

这些数据代表指定的进程或线程对在核间迁移时的信息:
- ocpu:源cpu;
- dcpu:目的cpu;
- PLoad_AVG:线程在源cpu上的负载;
- Putil_AVG:当前调度实体的算力需求;
- MEM_USAGE :消耗内存资源情况;
- READ:read操作的字节数;
- WRITE:写操作的字节数;

### 关闭功能
通过如下命令关闭本功能：
```
diagnose-tools migrate-image --deactivate 
```
如果成功，将输出：
```
migrate-image is not activated
```
如果失败，将输出：
```
deactivate migrate-image fail, ret is -1
```
关闭功能后，本功能将不会对系统带来任何影响。
