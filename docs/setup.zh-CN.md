# Mid 部署与操作

**简体中文** · [English](setup.en.md) · [日本語](setup.ja.md) · [项目首页](../README.md)

[工程结构](#structure) · [硬件](#hardware) · [WROOM](#wroom) · [S3](#s3) · [Jetson](#jetson) · [操作检查](#operation) · [排查](#troubleshooting)

本轮通过14组协议/游戏离线用例及WROOM完整构建。S3只完成源码检查，未做硬件、训练或端到端测试；历史演示不能替代修订版验收。

<a id="structure"></a>

## 工程结构

```text
.
├── esp32-s3r16n8-idf/
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── partitions/
│   └── main/
│       ├── main.cc
│       ├── application.cc
│       ├── mcp_server.cc
│       ├── idf_component.yml
│       ├── audio/
│       └── boards/
├── esp32-wroom-32-arduino/
│   ├── esp32-wroom-32-arduino.ino
│   ├── Config.h
│   ├── Config.example.h
│   ├── MatrixLayout.h
│   ├── DisplayManager.h
│   ├── SpectrumManager.h
│   ├── GameEngine.h
│   └── BlinkerManager.h
├── jetson-nano-py36/
│   ├── run_gesture.py
│   ├── run_gesture2.py
│   ├── face_system_cn.py
│   ├── face_system.py
│   ├── face_pyserial.py
│   └── faces/
└── docs/
    ├── images/
    ├── technical-notes.{zh-CN,en,ja}.md
    └── setup.{zh-CN,en,ja}.md
```

花括号表示三种语言文件。`Config.private.h`、`sdkconfig`、`dependencies.lock`、`managed_components` 和构建目录属于本地／忽略文件，不能假定干净克隆自带。人脸目录仅含占位文件。

<a id="hardware"></a>

## 硬件与连接

| 路径 | 源码引脚与设置 | 核对事项 |
| :--- | :--- | :--- |
| S3 → WROOM | S3 GPIO17 TX → WROOM GPIO16 RX；115200 8N1。 | `main.cc` 与 `Config.h`。 |
| UART | S3 GPIO17 TX→WROOM GPIO16 RX并共地；反向UART不使用，WROOM TX配置为-1，S3不分配GPIO18给RX。GPIO18留给灯控制器，不应再连接旧的WROOM→S3反向线。接线仍需按实际开发板确认。 | 115200 8N1 |
| Jetson → WROOM | WROOM GPIO25 RX；配置注释标 Jetson J41 pin8 TX，反向 pin10 RX 接 WROOM GPIO26。 | 核实 Nano 载板版本、设备节点及引脚电平；共享信号地。 |
| 灯板 | WROOM GPIO4 → DIN；WS2812B，GRB，256 灯，初始亮度30。 | 当前索引为单块16×16逐行蛇形，DIN按正视左上；历史4块版不能直接套用。 |
| 音频／显示 | 按选定 S3 `boards/.../config.h`。 | 目录名不证明具体模组型号；核对麦克风、功放／扬声器、可选OLED及引脚。 |
| 电源 | 本地课程接线指南记载WROOM从USB供电、VIN连接灯板V+。 | 历史说明不是当前容量测量；FastLED 5 V/1200 mA只作软件预算。 |

各设备按实际电路供电，共享信号参考地；没有确认电源方案前不要把两个独立5 V输出并接。仓库未给出可核验的完整原理图、PCB或额定3 A电源证据，不据此声称电气隔离设计。按实际板卡资料核实电源和逻辑电平。

<a id="wroom"></a>

## WROOM 构建配置

WROOM完整构建通过：Arduino ESP32 3.3.8、FastLED 3.9.15、Blinker 0.3.10230510、ArduinoJson 7.4.3；应用1348101字节，静态RAM60588字节。测试使用4 MiB Flash配置及官方 `min_spiffs` 分区（应用上限1966080字节），不代表已核验实际板载Flash。默认应用分区不足；烧录前必须确认硬件容量与分区。当前FastLED 3.10.5在本机触发编译器内部错误，3.9.15在隔离目录验证成功，全局库未改动。

保留 [Config.h](../esp32-wroom-32-arduino/Config.h)，在同目录创建被 `.gitignore` 排除的 `Config.private.h`：

```cpp
#pragma once
const char auth[] = "YOUR_BLINKER_AUTH";
const char ssid[] = "YOUR_WIFI_SSID";
const char pswd[] = "YOUR_WIFI_PASSWORD";
```

不要覆盖 `Config.h`；旧 `Config.example.h` 缺少当前映射和功率宏。打开 [主 `.ino`](../esp32-wroom-32-arduino/esp32-wroom-32-arduino.ino)，确认同目录头文件完整后编译。先低亮度验证四角和奇偶行，再确认游戏及频谱方向。初始联网尝试最多15 s，失败会继续离线执行，后来连接成功时再初始化 Blinker。

在 Blinker 中配置键名精确为 `W`、`S`、`A`、`D`、`E`、`F` 的按钮。界面导出和具体App版本未归档；以回调触发和串口日志验证，不假定任意模板都匹配。

<a id="s3"></a>

## S3 工具链与板型

当前 [依赖清单](../esp32-s3r16n8-idf/main/idf_component.yml) 要求 ESP-IDF **>=5.5.2**；根 CMake 的2.2.4是小智项目版本，不是IDF版本。使用满足要求的工具链，并记录实际版本。构建还需 Component Manager 获取依赖、ESP-DSP、语音资源及所选板型所需组件。

在已激活 ESP-IDF 的终端，进入仓库根目录：

```powershell
cd esp32-s3r16n8-idf
idf.py --version
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

`COMx`替换为实际串口；`set-target`会重新生成本地配置，已有配置须先备份。用menuconfig选择实际板型、Flash/PSRAM、分区、音频、语言与唤醒资源。当前本地快照是`BREAD_COMPACT_WIFI`、16MB Flash、Octal PSRAM、SSD1306 128×32，不保证这些选择对应所有样机，更不证明芯片商品型号。

该板型配置为麦克风WS/SCK/DIN=4/5/6，扬声器DOUT/BCLK/LRCK=7/15/16，OLED SDA/SCL=41/42；数字属于S3，与WROOM的同号GPIO是不同节点。先解决GPIO18灯输出和UART RX复用。小智配网、激活和服务端工具配置见[S3上游文档](../esp32-s3r16n8-idf/README_zh.md)与[协议资料](../esp32-s3r16n8-idf/docs/mcp-protocol.md)。服务器凭证、模型／媒体服务不随本仓库提供。

<a id="jetson"></a>

## Jetson 环境与启动

脚本目录名`py36`记录历史环境，不代表当前所有依赖可在Python3.6上直接从PyPI安装。桌面原始资料有cp36/aarch64的MediaPipe 0.8/0.8.4、NumPy1.19.4与OpenCV contrib4.5.2.52安装包，但未随仓库发布，也没有完整环境锁。应使用与JetPack、系统Python和架构匹配的环境。

需要 `cv2.face`、Haar级联数据、`mp.solutions.hands`、NumPy、pyserial。先在目标环境检查，缺失时按该环境可用的兼容包安装，避免同时混装多个OpenCV wheel：

```bash
python3 -c "import cv2, mediapipe as mp, numpy, serial; print(cv2.__version__); print(cv2.face.LBPHFaceRecognizer_create); print(mp.solutions.hands)"
ls -l /dev/ttyTHS1
systemctl status nvgetty
```

确认该节点确实连接J41、被`nvgetty`占用时，才停止／禁用此服务。按设备所属组配置当前用户权限并重新登录，避免把世界可写`chmod 666`当作默认步骤。不同载板不一定使用同一节点。串口独占释放后，可以按实际节点测试：

```bash
stty -F /dev/ttyTHS1 115200 raw -echo
printf '1\n' > /dev/ttyTHS1
cd jetson-nano-py36
python3 run_gesture2.py
```

上例预期请求红色，不代表已收到硬件ACK；以WROOM日志和灯板观察确认。停止手势程序后再运行 `python3 face_system_cn.py`。两者使用图形窗口，手势按Esc退出，人脸按S录入、T重训、Q退出。人脸样本目录名必须为`ylk`／`zzc`才能触发1／2，其他登记名只用于识别。按S后需要在终端输入名字；每次保存后按T才更新当前模型。人脸图保持本地，不提交。

<a id="operation"></a>

## 逐路径操作与验证

1. **灯板与USB：** WROOM监视器115200，发送`1`、`2`、`3`、`0`加换行；观察红、蓝、绿、关闭，并核对坐标方向。
2. **手机游戏：** E进入贪吃蛇，W/S/A/D方向；F进入俄罗斯方块，W旋转、S下移、A/D横移。游戏结束观察两位得分。同一运行模式重复发送不会重新初始化。
3. **S3语音：** 先验证在线会话，再确认日志中实际调用`self.device.set_light_mode`并发送模式；工具返回成功不能替代灯板观察。
4. **音频频谱：** 让服务端返回实际可播放音频并开启音乐标记，确认发送`15`及带17整数的`f,`帧。单独串口切15不会生成FFT数据。静音帧存在字段不足问题，先修复后再验收静音与退出。
5. **视觉：** 手掌5、胜利6、握拳7；登记`ylk`红、`zzc`蓝。分别测试失去目标、换人、串口失败与多输入切模。

记录板型、固件提交、灯板版、工具链、输入、日志与观察结果。上述是待执行检查，不是本轮测试通过清单。

<a id="troubleshooting"></a>

## 常见问题

| 现象 | 检查依据 |
| :--- | :--- |
| 覆盖配置后缺宏／无法编译 | 恢复正确的`Config.h`内容，在私有文件只写凭证；不要套用旧example。 |
| 灯图分块／方向错 | 区分历史四块和当前单块；核对DIN和`MatrixLayout.h`，不改文案掩盖走线差异。 |
| 音乐卡在最后画面／不能退出 | 检查有效频谱帧、静音字段数、1.2 s锁与S3音乐标记；没有无帧定时衰减。 |
| 人脸提示无数据／缺`cv2.face` | 提供本地样本并用contrib构建；核对100×100灰度裁剪及训练操作。 |
| 相机／UART忙 | 不并行启动手与脸；检查摄像头0、服务占用、节点权限和公共地。 |
| 语音能答但灯不变 | 区分服务端返回、MCP参数、UART实际输出、WROOM接收与保护逻辑，逐段检查。 |

2026-10 修复版：静音帧包含节拍与16频带，频谱严格校验字段数和范围，无效帧不刷新；超过500 ms没有有效帧后每30 ms继续衰减，零输入不再强制画底行。串口使用96字节固定缓存，超长帧丢弃至分隔符，模式只允许0–16、19–22，所有发送端必须以CR/LF结束。合法显式模式（包括0）可以立即退出音乐，不再由旧保护门丢弃。

S3 GPIO17 TX→WROOM GPIO16 RX并共地；反向UART不使用，WROOM TX配置为-1，S3不分配GPIO18给RX。GPIO18留给灯控制器，不应再连接旧的WROOM→S3反向线。接线仍需按实际开发板确认。

WROOM完整构建通过：Arduino ESP32 3.3.8、FastLED 3.9.15、Blinker 0.3.10230510、ArduinoJson 7.4.3；应用1348101字节，静态RAM60588字节。测试使用4 MiB Flash配置及官方 `min_spiffs` 分区（应用上限1966080字节），不代表已核验实际板载Flash。默认应用分区不足；烧录前必须确认硬件容量与分区。当前FastLED 3.10.5在本机触发编译器内部错误，3.9.15在隔离目录验证成功，全局库未改动。

先把 `$FastLedDir` 设为独立下载的官方FastLED 3.9.15绝对路径，`$BuildDir` 设为外部构建目录；安装并匹配上述平台/其他库。以下是编译配置，不执行上传；实际Flash容量需先确认。

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs" --library "$FastLedDir" --build-path "$BuildDir" esp32-wroom-32-arduino
```
