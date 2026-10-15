# 腕部驱动设备 — 上位机

腕关节康复设备（腕部驱动设备）的 PC 端串口控制台，基于 **Qt 6 / C++** 开发。
用于通过串口连接下位机 MCU，控制 M1/M2 两个电机与康复流程，接收并显示
电机、IMU、六维力传感器遥测数据，并把遥测数据记录为 CSV 文件。

---

## 目录结构

```text
腕部驱动设备/
├─ README.md                  # 本文件：项目总览与快速上手
├─ 00_项目说明/                      # 全部说明文档
│  ├─ 项目说明.md              # 项目详解：功能、架构、源码逐文件说明
│  ├─ 使用手册_连接与采集.md    # 硬件连接、串口配置、数据采集流程
│  ├─ 串口协议说明.md          # 帧格式、命令码、IO ID、浮点参数 ID
│  ├─ 串口连接排查记录.md      # 设备识别、驱动状态、端口/波特率实测与误码定位
│  └─ 目录调整记录.md          # 目录方案迁移记录
├─ 01_上位机程序/                       # 自有上位机源码
│  ├─ CMakeLists.txt
│  ├─ main.cpp
│  ├─ mainwindow.h / .cpp / .ui
│  ├─ protocolworker.h / .cpp
│  ├─ floatiddefs.h
│  └─ .gitignore
├─ 02_采集数据/
│  └─ telemetry/              # 采集到的遥测 CSV 数据
├─ 03_构建与辅助工具/
│  └─ build.ps1               # 一键构建脚本
└─ build/                     # 构建产物（不纳入版本控制）
   ├─ Desktop_Qt_6_9_1_MinGW_64_bit_Debug/    # 旧版构建（Qt 6.9.1，历史留存）
   ├─ Desktop_Qt_6_8_2_MinGW_64_bit_Release/  # 当前可用构建（Qt 6.8.2）
   └─ build-Qt612-MinGW-Debug/                # 早期失败配置残留
```

---

## 快速开始

### 直接运行（无需编译）

双击：

```text
build\Desktop_Qt_6_8_2_MinGW_64_bit_Release\Uart_controller.exe
```

窗口标题为 **“腕关节康复设备串口控制台”**。
Qt 与 MinGW 运行库已部署到该 exe 同目录，可直接运行，无需额外配置环境变量。

### 重新编译

在 PowerShell 中执行：

```powershell
.\03_构建与辅助工具\build.ps1
```

脚本会自动配置 CMake、编译并把运行库部署到 exe 同目录。

### 用 Qt Creator 打开

打开 `01_上位机程序\CMakeLists.txt`，选择 **Desktop Qt 6.8.2 MinGW 64-bit** 套件。

> **重要**：必须使用 **Release** 配置。本机 Qt 只安装了 Release 运行库，
> 没有 Debug 版 DLL（`Qt6Cored.dll` 等），Debug 配置编译出的程序无法运行。

---

## 环境要求

| 组件 | 版本 / 路径 |
|---|---|
| Qt | 6.8.2（`C:\Qt\6.8.2\mingw_64`） |
| Qt 模块 | Core、Widgets、**SerialPort**、**Charts** |
| 编译器 | MinGW 13.1.0（`C:\Qt\Tools\mingw1310_64`） |
| CMake | 3.19+（`C:\Qt\Tools\CMake_64`） |
| 构建器 | Ninja（`C:\Qt\Tools\Ninja`） |

---

## 已知限制与注意事项

### 1. 项目路径含中文，必须通过 ASCII 联接构建

Qt 的 `moc.exe` 无法处理含中文的路径，会报：

```text
moc: Cannot create C:/Users/Lenovo/Desktop/鑵曢儴椹卞姩璁惧/... Error: No such file or directory
```

因此 `03_构建与辅助工具\build.ps1` 会自动创建一个 ASCII 路径的目录联接（junction）：

```text
C:\Users\Lenovo\uart_wrist  →  C:\Users\Lenovo\Desktop\腕部驱动设备
```

通过该联接路径构建即可正常工作。**该联接不复制、不移动任何源码。**
直接双击运行 exe 不受此限制影响。

### 2. 只能使用 Release 构建

本机 Qt 未安装 Debug 运行库，Debug 构建产物无法启动。

### 3. 数据文件写入位置

CSV 记录与 `error_frames.txt` 写入 **exe 所在目录**（即程序运行目录），
而不是 `02_采集数据\telemetry\`。采集完数据后需要手动归档到 `02_采集数据\telemetry\`。

### 4. 记录功能只在停止时落盘

代码中遥测数据先缓存到内存（`m_recordBuffer`），**点击“停止记录”时才写入 CSV**。
请勿在记录过程中直接结束进程，否则数据会丢失。

---

## 安全提示

本程序可向设备下发电机启停、模式切换、康复动作、Flash 保存等命令。
连接真实硬件前请确保：

- 设备处于安全状态，急停/断电方式可用；
- 空载测试通过后再接触人体；
- 谨慎使用机械零点、清除故障、力传感器清零、Flash 保存等持久化操作。

## 中文目录整理（2026-10-15）

外层目录已按 B 方案迁移为编号加中文职责名。技术文件名、构建输出和虚拟环境保留。目录映射和验证情况见 `00_项目说明/目录调整记录.md`。

根目录 `启动腕部上位机.cmd` 用于启动；`启动腕部上位机.cmd --check-paths` 检查部署和采集归档目录，不连接设备。Qt 构建使用 build/source 源码联接绕过中文路径限制，该联接指向唯一源码目录，不是副本。CSV 原有写入位置仍为 exe 所在目录，02_采集数据/telemetry 用于手工归档。
