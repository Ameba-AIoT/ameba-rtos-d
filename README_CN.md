
![Realtek-logo](https://logos-download.com/wp-content/uploads/2016/05/Realtek_logo_logotype.png)

* [English Version](./README.md)

如果您有任何问题，请登录瑞昱官方网站[RealMCU](https://www.realmcu.com/) 获取更多信息。

目录
=================

* [1 开始](#1-开始)
  * [了解AmebaD](#了解AmebaD)
* [2 文档](#2-文档)
* [3 配置环境](#3-配置环境)
  * [Windows](#windows)
  * [Linux](#linux)
  * [连接串口](#连接串口)
* [4 编译项目](#4-编译项目)
   * [Windows GCC](#windows-gcc)
   * [Linux终端](#Linux终端)
* [5 固件烧录](#5-固件烧录)
* [6 发布说明](#6-发布说明)


# 1 开始

## 了解AmebaD

> AmebaD系列有多款型号的芯片可供选择，包括RTL8720CS， RTL8721CSM， RTL8722CSM， RTL8720DN， RTL8720DM， RTL8721DM和RTL8722DM。
> 此外，芯片组内所有型号的芯片都可以集成到AmebaD开发板中，开发板集成了多种多样的I/O接口。我们还提供了对应的HDK(硬件开发包)相关文档，请联系我们获取更多的详细信息。

AmebaD开发板的硬件框图如下所示:

   - USB TO UART: 供电和log输出
   - LOG UART: 波特率为115200bps 
   - SWD: SWD接口，用于IAR中烧写固件和代码调试
   - Reset button: 重启按钮，用于重启AmebaD

<p align="center">
	<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_3.png" width="600" />
</p>

# 2 文档

最新文档: [RTL872xCS/RTL872xD Series](https://www.realmcu.com/en/Home/DownloadList/a6248c82-7c9a-4f79-bebf-0df240726c4d)

# 3 配置环境

## 工具链
	
工具链同时支持Windows和Linux平台。

## Windows

> 注意: Windows平台可以使用``Cygwin``作为GCC开发环境。``Cygwin``是一个大型的GNU和开源工具集合，支持在Windows上提供类似于Linux distribution版本的功能。

> ``Cygwin``下载网址: [http://cygwin.com](http://cygwin.com/)， 下载安装包[setup-x86.exe](http://cygwin.com/setup-x86_64.exe)。 

1. 32位``Cygwin``同时支持32位和64位Windows系统。

> 注意: 如果不能安装32位Cygwin，请参考[Q: How can I install the last Cygwin version for an old, unsupported Windows?](https://cygwin.com/install.html)。
使用命令行进行安装:
```
setup-2.912.x86.exe --allow-unsupported-windows --site http://ctm.crouchingtigerhiddenfruitbat.org/pub/cygwin/circa/2022/11/23/063457
```
2. 安装``Cygwin``安装包时，在Select Packages页面选择`Devel -> make`和`Math -> bc`，如下所示。

	<p align="center">
	  <img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_1.png" width="400" />
	  <img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_2.png" width="400" />
	</p>

## Linux

> 注意: Linux平台请参考[Ameba-D Application Note(PDF)](https://www.realmcu.com/en/Home/DownloadList/a6248c82-7c9a-4f79-bebf-0df240726c4d) 来配置GCC开发环境。 

## 连接串口

> 在AmebaD开发板上，使用FTDI芯片和FT232芯片作为日志输出和调试器。为了观察日志输出，可以使用例如``SecureCRT``/``TeraTerm``/``Putty``等其他的终端工具。以下我们将使用内部开发的工具作为例子来展示。 

1) 选择实际使用的串口名称，配置串口参数并且打开串口。
2) 按下AmebaD开发板上的重启按钮，终端将会打印以下信息。

	<p align = "center">    
		<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_4.png" width="500" />
	</p>

# 4 编译项目

## Windows GCC

以下步骤适用于开发者首次构建GCC项目:

> SDK编译的详细步骤请参考上面链接中应用指南的**Building Code**小节。
> 首先，你需要将路径切换到GCC项目下。对于Windows平台，打开``Cygwin``终端使用命令``cd``切换路径进入KM0或者KM4项目。

> 注意: 需要将``{path}``替换为你自己的SDK路径，在SDK路径前添加``cygdrive``前缀，以便Cygwin访问你的文件系统。

```bash
cd /cygdrive/{path}/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp
cd /cygdrive/{path}/project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp
```
编译默认的固件，直接执行命令``make all``。

对于KM0项目，如果终端输出日志中出现``km0_image2_all.bin``和``Image manipulating end``，则表示KM0的固件已经成功编译，如下所示。

<p align = "center">    
	<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_5.png" width="500" />
</p>

> 如果出现编译失败，请执行命令``make clean``清理工程之后重新编译。

> 编译成功之后，生成的固件所在位置``project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp/asdk/image``，如下所示。

<p align = "center">    
	<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_6.png" width="500" />
</p>

对于KM4项目，如果终端输出日志中出现``km4_image2_all.bin``和``Image manipulating end``，则表示KM4的固件已经成功编译，如下所示。

<p align = "center">    
	<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_7.png" width="500" />
</p>

> 如果出现编译失败，请执行命令``make clean``清理工程之后重新编译。

> 编译成功之后，生成的固件所在位置``project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp/asdk/image``，如下所示。

<p align = "center">    
	<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_8.png" width="500" />
</p>

## Linux终端

在Linux终端编译默认固件，

1. 使用以下命令切换路径进入到KM0或者KM4项目

```bash
cd /{path}/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp
cd /{path}/project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp
```

2. 在相应的路径下，执行命令``make all``编译固件。你可以先编译``project_lp``，然后再编译``project_hp``。
   - **KM0项目(project_lp)**
     - 对于KM0项目，如果终端输出日志中出现``km0_image2_all.bin``和``Image manipulating end``，则表示KM0的固件已经成功编译。
	 > 如果出现编译失败，请执行命令``make clean``清理工程之后重新编译。
	 
	 > 编译成功之后，生成的固件所在位置``project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp/asdk/image``。

   - **KM4 project (project_hp)**
     - 对于KM0项目，如果终端输出日志中出现``km4_image2_all.bin``和``Image manipulating end``，则表示KM4的固件已经成功编译。
	 > 如果出现编译失败，请执行命令``make clean``清理工程之后重新编译。
	 
	 > 编译成功之后，生成的固件所在位置``project/realtek_amebaD_va0_example/GCC-RELEASE/project_hp/asdk/image``，如下所示。

	<p align = "center">    
		<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_8.png" width="500" />
	</p>

> 注意:
> 如果出现如下的错误信息: 
  
```bash
make[1]: execvp: /{path}/ambd_sdk/project/realtek_amebaD_va0_example/GCC-RELEASE/project_lp/asdk/gnu_utility/prepend_header.sh: **Permission denied**
Makefile:198: recipe for target 'linker_loader' failed
```
 
> 请执行以下命令以授予所有相应文件的执行权限:

```bash 
chmod -R 777 {path}/project/realtek_amebaD_va0_example/GCC-RELEASE/
``` 


# 5 固件烧录

当编译结束之后，使用工具[ImageTool](tools/AmebaD/Image_Tool/ImageTool.exe)将固件烧写到芯片中:

* 仅支持Windows系统，包括WinXP，Win7或以上版本，需要安装微软插件.NET Framework 3.5。

假设运行在PC上的ImageTool是服务器，而AmebaD是客户端，服务器端通过UART将固件发送给客户端时，客户端必须先进入烧录模式。

1) **进入UART_DOWNLOAD模式。**

   * 按住UART DOWNLOAD按钮，先不要松开。
   * 然后按下重启按钮给板子重新上电。
   * 最后松开UART DOWNLOAD按钮。
   
   现在AmebaD进入了UART_DOWNLOAD模式，准备开始接收数据。
   
2) **在ImageTool UI界面点击Chip Select按钮，选择芯片AmebaD。**

3) **选择实际使用的串口和传输波特率。默认的波特率是1.5Mbps(推荐)。**

4) **点击Browse按钮选择固件(km0_boot_all.bin/km4_boot_all.bin/km0_km4_image2.bin)，设置烧写的起始地址。**

   * 固件所在路径: 
	 ```
     {path}\project\realtek_amebaD_va0_example\GCC-RELEASE\project_lp\asdk\image
     {path}\project\realtek_amebaD_va0_example\GCC-RELEASE\project_hp\asdk\image
	 ```
     其中{path}是你的实际SDK的路径。
   * ImageTool界面显示的默认的起始地址即为SDK中默认的固件起始地址，可以直接使用。

5) **点击Download按钮开始烧写，每个固件的下载进度都会显示在进度条上，日志窗口提供当前操作状态信息。**

   <p align = "center">    
		<img src="https://www.amebaiot.com/wp-content/uploads/2020/02/guide_d_10.png" width="400" />
   </p>

# 6 发布说明

> 注意: 对于蓝牙例程，目前仅支持`BT_peripheral`，`BT_central`，`BT_scatternet`和`BT_config`这四个例子。

## 版本同步

GitHub仓库`ameba-rtos-d`目前同步到6.2 Git分支的版本为73b6c5ec。
