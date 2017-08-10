车载产品的软硬件设计
=====================

## 整体结构和设计思路


信号定义
1. +B： 电池电源， 这个电压的参考值是12V，连接到汽车的蓄电池上，一般工作电压为10V~14V。在发动起启动的瞬间，电压一般会掉落到8V左右1～2秒，在某些车型上甚至可能掉落到6V左右。
关于车辆电源部分的规范，一般的规则是
- 汽油车的电源系统为12V
- 柴油车的电源系统是24V
近年来的部分产品，包括新能源车，因为车载电器的功率越来越大，所以出现了48V供电系统。本文中以下部分都基于12V系统的前提。

下面图片显示了一个实际的发动机启动的波形图（某个项目的测试Case)
<img src="./picture/IG_ON_Signal.png" >

2. ACC：<BR> 辅助电源。 传统汽车的钥匙孔上有 LOCK, ACC和Start三个位置，LOCK位置，发动机不发动，ACC不供给。 ACC位置，发动机可能发动也可能不发动。在Start位置，如果发动机未发动，则会开始启动发动机。（注意Start位置时ACC是OFF状态）
3. IG:<BR>发动机信号，这个信号表示发动机是否已经启动。一般在AVN产品中并无作用
4. ILL信号
ILL信号包括了两个部分，一个叫做ILL ON/OFF信号。这个信号的来源可以是后线束的电平信号，也可能是CAN总线。
举例：某个车型的灯光开关如下图所示
<img src="./picture/light.jpg" >
当开关放在“灯光关闭”位置时，ILL ON/OFF信号会获取到 OFF状态 （白天）
当开关放在“示廓灯”或者“近光灯”位置时，ILL ON/OFF信号会获取到ON状态 (夜晚模式)
ILL Step信号： 这个信号的来源可能是：
- 后线束的ILL Step PWM信号，这个信号的占空比表示ILL Step的设置
- CAN总线的广播信息，具体的ID与车辆型号有关，一般是周期性发送（比如H车型是300ms发送一次）
绝大部分车上都有一个“仪表背景灯亮度调节”的旋钮，这个旋钮调节后，ILL Step的值会改变。DA（AVN）会从PWM信号或者CAN总线获取到信号，根据ILL Step的值可以调整LCD背光和按键背光的亮度。

5. VSP信号
VSP信号的来源是与车辆的速度相关的，VSP信号的来源可能包括：
- 后线束的脉冲信号， 这个信号一般是50%占空比的脉冲信号，与车轮的转速成正比。
  举例：某个车型的VSP Plus的规格是：车轮每旋转一圈，产生4个Plus
- CAN总线<BR> 上文提到的脉冲信号，通过一个ECU的转换，发送到CAN总线上。

6. CAN信号
  CAN信号上有很多信号通过广播的方式传播，与AVN相关的信号包括： ACC信号，ILL信号，车速信号，方向盘转角，锁车信号，车门车窗信号，空调状态信号等。CAN相关的入门文档可以参考 http://192.168.5.2/wiki/

7 其他
  在某些混合动力车上会有IG1和IG2信号，用于标示两个发动机是否启动。


## 电源状态转移及电源变动测试标准

针对电源状态变化，一般分为有钥匙启动和无钥匙启动两种情况。
对于有钥匙启动的车型（传统型号）

一般汽车的钥匙孔是这样的：

<img src="./picture/key_hole.png" width = "120" height = "120">

可以看到钥匙孔有三个位置： LOCK， ACC和START。

汽车未使用时，钥匙孔位置在"LOCK", 此时ACC信号为低电平。插入钥匙并且旋转到“ACC”位置后，ACC电源有效。
当钥匙旋转到“START”位置时， 并不能保持，松手后会回到“ACC”位置。 在“START”位置，ACC信号是低电平。启动电机工作，因为启动电机电流很大，此时蓄电池的输出电压会降低（在某些车型上，电压可能会低至6V）

无钥匙启动的设计如下图

<img src="./picture/engine_start_key.jpg" >

如果不踩刹车按启动按钮，会进入ACC ON状态，ACC


## 接口设计


### MCU通信协议的设计
  MCU与SoC间的通信接口五花八门，针对不同的结构我们设计了通用的通信协议。note
（对于MCU集成到SOC的情况，也认为MCU是单独的系统）

MCU到SOC的结构包括以下几种可能性

- MCU作为一个单片机，通过外部通信接口与SoC相连
  这个方案实际上是目前大部分车载产品的结构， MCU与SoC的连接方式，可以是串口，I2C， SPI等外部通信线路，也可能是特定的高速总线。还有可能使用速度更快的双口RAM之类的通信方式。

- MCU作为SoC上的一个模块，通过内部总线与SoC通信

- MCU是SoC系统上的一个虚拟设备，比如在Hypervisor上运行的一个实时操作系统


首先我们抽象出一个模型，假设所有的通信都由SoC发起（这样假设的原因是在某些通信方式比如I2C下，SOC可能不支持作为Slave模式）, 对于MCU需要发起的通信，MCU通过额外的一个中断信号来通知SOC。
UART通信方式下可能没有中断信号，这时在通信协议设计时可以通过MCU向SOC发送特定的数据来实现。
（这部分以后作为


### 进程间通信和消息传递

进程间通信采用三种方式： DBUS， 共享内存和{PIPE or Socket}

- DBUS用于普通的通信，系统进程间的大部分交换采用DBUS完成。 DBUS上传递的消息类型为数据块结构体，接口定义通过通信模块的函数库定义并且封装实现。
- 共享内存用于传递大量数据，共享内存的handle通过dbus传递，共享内存的分配和管理通过通信模块的函数封装
- Socke（或者Pipe)用于传递低延迟的消息，Socket创建时，名称通过DBUS传递
- 其他：在系统启动早期的信息传递，需要一些特殊的手段，待定



### HAL layer
Hardware abstract lay defines a standard interface for all hardware access

- data storage and protection
- MCU communication
- USB device management
- Power and System control
- Vechial signals
- Input and Display systems
- Video input systems (RVC, recording, birdview, etc)
- ADAS systems (No detailed information)
- Audio input/output


### 数据存储和保护策略

需要存储的数据从几个不同的维度定义
1. 保存的位置
2. 保存和保留的时机
3. 初始化（回复出厂设置）后的状态

包括以下几个型
- 出厂配信息，包括MAC地址，序列号，密钥，签名，射频参数，音频参数，校准参数等。在出厂设置时针对每一个设备写入不同的值，出厂后除非返厂，否则不会修改，其他任何时刻都不允许擦除或改动。这些数据错误或消失会导致部分功能无法正常启用。这些数据在代码中有缺省值缺省值只能保证系统启动不能保证功能正确。
- 产品统一配置信息：包括很多厂商预设的参数，这些参数主要是用于支持同一个硬件适应不同的产品型号或者销售区域（也可能包括不同的档次定位等）， 在出厂设置时写入，但是同一批次的产品内容相同。这些数据除非返厂，否则不允许擦除和改动。数据缺失或错误会导致部分功能不可。这些数据在代码中有缺省值缺省值只能保证系统启动不能保证功能正确。
- 永久性的用户设置。


### RVC and surround views



- Flash中的数据存储原则

性能方面的要求
  数据分为两类，一类是日常改变的数据，一种是日常不变的数据
  产品性能的要求是：
    产品寿命周期10年，在生命周期内，假设日常改变的数据每天写入10次
    日常不变的数据每年写入10次
  对于Flash的要求：擦写次数不少于300次

  日常不改变的数据直接存储在Flash中，通过结构体的方式，便于访问。
  日常改变的数据分为两部分，一部分是需要在bootloader和kernel中访问的数据，一部分是只需要在用户态访问的数据。
    只需要在用户态访问的数据存粗在ext4文件系统中，为了保证flash性能不退化，需要多留空间
    在kernel和bootloader中都需要访问的数据，因为数据量小，通过特殊的方式直接存储在flash中。
Flash中的数据通过块来存储，一个块的大小为Sector大小的整数倍，在flash擦除后，应该是0xff，这时定义结构体的内容包括

typedef struct __stored_data
{
    unsigned long header; //=0x12345678
    data packes....
    unsigned long checksum;
}


- MCU的数据保存策略和方式



============
====

## 实际产品与仿真
 为了支持PC的调试，源代码中定义了

``` stylus
#define DA_UNDER_EMULATOR
```

 这个宏. 在代码中使用

``` stylus
 #ifdef DA_UNDER_EMULATOR
    ...Some code for emulator
 #else
    .. Code run on real device
 #endf
```

 这种方式来实现.



## MCU仿真程序
MCU仿真程序是一个模拟库，通过仿真MCU的响应来实现DA程序支持PC仿真运行

MCU仿真程序不包括操作系统，通过循环实现所有的程序。需要修改DA程序以适配。需要修改的部分包括
MCU 通信


## 需求定义

产品



### 设计思路 - HMI和功能分离


### 设计思路 - HMI与功能的异步调用

为了保证HMI的相应，不允许HMI通过函数调用的方式（阻塞）使用功能调用。到功能层的调用通过进程间通信方式异步实现。

函数调用的方式通过几种不同接口实现
- 发送消息，不等待结果
- 获取状态
- 发送命令并且等待执行结果（有时间限制）


### 设计思路 - MCU承担最少逻辑原则



# Design guide ##
## system boot time ##
After system ACC ON, the whole system shold be available to end users with in a short time. To archive this, there are two different solutions, one is to speed up boot time, the other way is to use “suspend to disk” function instead. This chapter will describe the implantation of each solution and compare them.


### KPI define ###
Usually the KPI of system boot time depends on device manufacture's spec, and usually effected by base system. For a RTOS system, it usually available within 2 or 3 seconds; For a Linux based system, usually a X11/Wayland based UI should cost quite a long time to load, and a UI based on OpenGL will need much longer load time. An android based system usually cost 20 to 40 seconds to boot in case of none optimiziation.
### Fast boot ###
