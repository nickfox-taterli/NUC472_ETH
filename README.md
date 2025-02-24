# LWIP 2.2 with FreeRTOS V11 and iperf2 测试项目 / LWIP 2.2 with FreeRTOS V11 and iperf2 Test Project

这是一个关于以下组件的测试项目 / This is a test project involving the following components:

- **LWIP 2.2**: 轻量级 TCP/IP 协议栈 / A lightweight TCP/IP stack.
- **FreeRTOS V11**: 嵌入式设备的实时操作系统 / A real-time operating system for embedded devices.
- **iperf2**: 网络性能测试工具 / A tool for network performance measurement.

---

## 主要优化 / Key Optimizations

- **RTOS 集成 / RTOS Integration**: 项目使用 FreeRTOS 进行任务调度和管理 / The project utilizes FreeRTOS for task scheduling and management.
- **发送使用零拷贝 / Zero-Copy Transmission**: 发送过程使用零拷贝技术以提高性能 / The sending process employs zero-copy techniques to enhance performance.
- **接收未使用零拷贝 / Non-Zero-Copy Reception**: 接收过程未使用零拷贝，主要是多线程操作可能会出现问题 / The receiving process does not use zero-copy due to potential issues with multi-threaded operations.

---

## 测速报告 / Speed Test Report

以下是使用 iperf2 生成的测速报告 / The following is a speed test report generated using iperf2:

```bash
# iperf -c 192.168.31.147
------------------------------------------------------------
Client connecting to 192.168.31.147, TCP port 5001
TCP window size: 64.0 KByte (default)
------------------------------------------------------------
[  1] local 192.168.31.237 port 58909 connected with 192.168.31.147 port 5001
[ ID] Interval       Transfer     Bandwidth
[  1] 0.00-20.30 sec 3.55 MBytes  1.4Mbits/sec
```

---

## 项目设置 / Project Setup

本项目使用 **ARM KEIL MDK** 打开和构建 / The project is designed to be opened and built using **ARM KEIL MDK**.

### 前提条件 / Prerequisites

- 安装 ARM KEIL MDK / ARM KEIL MDK installed on your system.
- 基本了解 LWIP、FreeRTOS 和 iperf2 / Basic understanding of LWIP, FreeRTOS, and iperf2.

### 快速开始 / Getting Started

1. 克隆仓库到本地 / Clone the repository to your local machine.
2. 使用 ARM KEIL MDK 打开项目 / Open the project in ARM KEIL MDK.
3. 构建项目 / Build the project.
4. 将二进制文件烧录到目标设备 / Flash the binary to your target device.
5. 运行 iperf2 测试以测量网络性能 / Run the iperf2 test to measure network performance.

---

## 注意事项 / Notes

- 本项目是一个快速原型，可能未针对所有用例进行优化 / This project is a quick prototype and may not be optimized for all use cases.
- 接收过程未使用零拷贝，未来可以改进 / The receiving process does not use zero-copy, which is an area for future improvement.

---

## 版权声明 / Copyright Notice

本项目没有版权，您可以随意复制、粘贴和修改 / This project has no copyright. You are free to copy, paste, and modify it as you wish.
