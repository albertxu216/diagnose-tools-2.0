### tcpstates 正确性效验

在测试中，我们同时使用了我们的TCP状态跟踪工具和Wireshark/tcpdump来捕获和分析网络流量。通过对比两个工具的输出结果，我们验证了TCP连接状态转换的正确性。我们的工具精确记录了每个TCP状态的变化，包括源IP、目的IP、源端口、目的端口、状态持续时间等详细信息。这些数据与Wireshark/tcpdump捕获的数据完全一致，表明我们的工具在记录和跟踪TCP状态转换方面具有高精度和可靠性。通过这种对比验证，我们确保了我们的工具在实际网络监控和分析应用中的准确性和有效性。

**测试步骤**：

1. 使用我们的工具捕获并记录TCP连接的状态转换，包括源IP、目的IP、源端口、目的端口、原先状态、新状态、状态持续时间、窗口大小、发送和接收字节数等详细信息。
2. 使用tcpdump捕获所有TCP流量并生成pcap文件/通过Wireshark捕获报文，检查TCP连接状态和数据包的详细信息。
3. 比较两个工具的输出，验证我们的工具在捕获和记录TCP连接状态方面的准确性。

**wireshark**

我们将工具同时开启，之后使用curl 1.1.1.1 模拟本机客户端的访问，得到以下结果

![image-20240723220318735](./images/\tcpstates-image5.png)

![697f02318247affac8f8012187cbfe8](./images/\tcpstates-image6.png)

使用curl 192.168.239.136 模拟服务器的访问，得到以下结果

![image-20240728151919466](./images/\tcpstates-image7.png)

![image-20240728152011383](./images/\tcpstates-image8.png)

通过从C/S架构的两端：客户端和服务器端的监测，可以看到我们得到的相应数据与实际数据相符，并且能在状态转化的层面上做到更加细粒度的监测。

**tcpdump**

编写相关脚本，同时启用本功能以及使用tcpdump来监听tcp报文

```shell
#!/bin/bash

# 定义变量
OUR_TOOL_INSTALL="sudo diagnose-tools install"
OUR_TOOL_START="sudo diagnose-tools tcpstates --activate"
OUR_TOOL_REPORT="sudo diagnose-tools tcpstates --report > our_tool_output.txt"
OUR_TOOL_UNINSTALL="sudo diagnose-tools uninstall"
TCPDUMP_TOOL="sudo tcpdump -i any tcp -w tcpdump_output.pcap"  # tcpdump命令，捕获所有接口的流量
TCPDUMP_TXT_OUTPUT="tcpdump_output.txt"  # tcpdump解析后的输出文件
OUR_TOOL_OUTPUT="our_tool_output.txt"
TCPDUMP_OUTPUT="tcpdump_output.pcap"

# 确保之前的输出文件被删除
rm -f $OUR_TOOL_OUTPUT $TCPDUMP_OUTPUT $TCPDUMP_TXT_OUTPUT

# 加载我们的工具
echo "Loading our tool..."
$OUR_TOOL_INSTALL

# 启动我们的工具
echo "Starting our tool..."
$OUR_TOOL_START &
OUR_TOOL_PID=$!

# 启动tcpdump
echo "Starting tcpdump..."
$TCPDUMP_TOOL &
TCPDUMP_PID=$!

# 等待一些时间以确保监听工具启动
sleep 5

# 运行测试用例
echo "Running test cases..."

# 示例测试用例：curl一个外部网站
curl http://baidu.com 

# 生成我们的工具的报告
echo "Generating our tool's report..."
$OUR_TOOL_REPORT

# 停止tcpdump
echo "Stopping tcpdump..."
sudo kill $TCPDUMP_PID

# 等待tcpdump完全停止
wait $TCPDUMP_PID

# 解析tcpdump生成的pcap文件
echo "Converting pcap to txt..."
sudo tcpdump -r $TCPDUMP_OUTPUT -nn -tttt > $TCPDUMP_TXT_OUTPUT

# 卸载我们的工具
echo "Uninstalling our tool..."
$OUR_TOOL_UNINSTALL

echo "监听工具和测试用例已完成。输出文件为 $OUR_TOOL_OUTPUT 和 $TCPDUMP_TXT_OUTPUT"

```

输出文件如下

![image-20240728155241145](./images/\tcpstates-image9.png)



经过几种不同工具的验证比对，表明我们的工具在记录和跟踪TCP状态转换方面具有高精度和可靠性。

我们的工具在TCP连接状态跟踪和分析方面具有以下显著优势：

1. **详细的状态信息**：我们的工具不仅捕获TCP包，还详细记录了每个连接的状态转换，包括发送窗口大小、接收窗口大小、缓冲区大小、发送和接收字节数等信息。这些信息对于深入分析TCP连接性能和状态至关重要。
2. **实时监控和低开销**：我们的工具可以实时监控TCP连接状态，具有较低的系统开销。
3. **专注于TCP状态转换**：我们的工具专门设计用于跟踪和分析TCP状态转换，提供了丰富的状态信息，而现有工具如Wireshark和tcpdump虽然功能强大，但主要侧重于数据包捕获和协议分析，并不专注于TCP状态转换的细节。

综上所述，我们的工具在TCP连接状态跟踪和分析方面具有明显优势，提供了更详细、更专业的信息，具有更低的系统开销和更高的易用性。

