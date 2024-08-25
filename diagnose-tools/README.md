# diagnose-tools

1､快速上手

  建议在 Linux 5.19 内核版本中进行实验。

  第一步、使用如下命令clone代码：

    git clone https://gitlab.eduxiji.net/T202411664992682/project2210132-233932
    
  第二步、在diagnose-tools目录中运行如下命令初始化编译环境：
  
    make devel        # 安装编译过程中需要的包
    
    make deps         # 编译依赖库，目前主要是编译java agent，以支持用户态java符号表解析
    
  第三步、编译工具：
  
    make
    
    这一步实际上会完成rpm的安装，你也可以用如下命令分别完成相应的工作：
    
    make module       # 编译内核模块
    
    make tools        # 编译用户态命令行工具
    
    make java_agent   # 编译java agent
    
    make pkg          # 制作rpm安装包
    
    
