# 基于 ESP32 的音频信号发生器与示波器 (ESP32 Audio Generator & Scope)

---

## 一、 项目简介与整体架构

本项目利用 **ESP32 DevKit V1** 开发板作为主控，构建了一个兼具“模拟信号采集显示”与“高品质音频输出”的综合硬件平台。
* **示波器端 (Scope Input)**：交流信号经硬件隔直与偏置网络抬升至 $1.65\text{V}$，由 ESP32 ADC1 连续采样并计算波形参数（频率、$V_{pp}$ 等）。
* **显示端 (Display)**：使用 2.4 寸 ST7789 IPS 屏（SPI 接口，240x320 分辨率）实现波形的实时双缓冲平滑刷新。
* **发生器端 (Generator Output)**：利用 ESP32 硬件 I2S 控制器外接 PCM5102A 独立 DAC，输出 Hi-Fi 级低底噪模拟音频信号。
* **后级输出 (Audio Output & Monitor)**：双声道均配备 RC 耦合与限流保护网络。左声道连接无源蜂鸣器模组实现声音监听，右声道引出至探针接口向外部电路注入信号。

### 1.1 系统架构框图

```
+-----------------------------------------------------------------------------------+
|                                  ESP32 DevKit V1                                  |
|                                                                                   |
|  [ ADC1_CH6 (GPIO34) ] <--- 1.65V 直流偏置电路 <--- 外部模拟输入 (ADC_IN 探针)            |
|  [ I2S0 (GPIO22/25/26) ] --> PCM5102A DAC --> RC 滤波/耦合 --> L: 蜂鸣器 / R: DAC_OUT  |
|  [ SPI2 (GPIO2/4/5/18/23)] -> ST7789 2.4寸 IPS 彩屏 (240x320)                        |
+-----------------------------------------------------------------------------------+
```

---

## 二、 硬件材料清单 (BOM)

所有分立器件与模组均采用**标准通孔直插 (THT/DIP)** 封装，方便面包板搭线与手动焊接：

| 位号 | 元器件 / 模组描述 | 推荐规格 / 型号 | 数量 | 作用说明 |
| :--- | :--- | :--- | :--- | :--- |
| `U3` | 主控开发板 | ESP32 DevKitC / DOIT V1 (DIP-30) | 1 | 系统主控核心 |
| `U2` | 显示模组 | ST7789 2.4寸 IPS 8-Pin 屏 (GMT024-08P) | 1 | 实时 UI 与波形显示 |
| `U1` | I2S DAC 模组 | PCM5102A 紫色模组 (15-Pin) | 1 | 高品质音频 DAC 解码 |
| `BUZ`| 蜂鸣器模组 | KY-006 / HW-508 3-Pin 蜂鸣器模组 | 1 | 左声道实时声音监听 |
| `C1` | 直插铝电解电容 | 10µF / 25V ($P=2.5\text{mm}$) | 1 | ADC 输入隔直耦合（注意极性） |
| `C2, C3`| 直插铝电解电容 | 10µF / 25V ($P=2.5\text{mm}$) | 2 | 双声道 DAC 输出隔直耦合 |
| `R1, R2`| 直插五色环电阻 | 10kΩ, ±1% 1/4W | 2 | ADC 1.65V 直流分压偏置 |
| `R3, R4`| 直插五色环电阻 | 1kΩ, ±1% 1/4W | 2 | 输出短路保护与阻抗匹配 |
| - | 测试端子 / 排针 | 2.54mm 单排针 | 1 套 | `ADC_IN` 输入与 `DAC_OUT` 输出探针 |

---

## 三、 详细物理接线矩阵 (Pin-by-Pin Matrix)

### 3.1 主控 MCU：ESP32 DevKit V1 (DIP-30)
* **`Pin 1 (3V3)`** $\to$ 挂载至系统 **`3V3` 电源主总线**
* **`Pin 2 (GND)`** $\to$ 挂载至系统 **`GND` 地线主总线**
* **`Pin 4 (D2)`** $\to$ 板载蓝色 **音频活动 LED**（有 I2S 波形输出时常亮，软件控制）
* **`Pin 5 (D4)`** $\to$ `U2 (ST7789)` Pin 5 (`RST`)
* **`Pin 8 (D5)`** $\to$ `U2 (ST7789)` Pin 7 (`CS`)
* **`Pin 3 (D15)`** $\to$ `U2 (ST7789)` Pin 8 (`BL`)
* **`Pin 9 (D18)`** $\to$ `U2 (ST7789)` Pin 3 (`SCL`)
* **`Pin 15 (D23)`** $\to$ `U2 (ST7789)` Pin 4 (`SDA`)
* **`Pin 38 (D19)`** $\to$ `U2 (ST7789)` Pin 6 (`DC`)
* **`Pin 27 (TX2/GPIO17)`** $\to$ `U1 (PCM5102A)` Pin 4 (`DIN`)
* **`Pin 23 (D25)`** $\to$ `U1 (PCM5102A)` Pin 3 (`LRCK`)
* **`Pin 22 (D26)`** $\to$ `U1 (PCM5102A)` Pin 5 (`BCK`)
* **`Pin 27 (D34)`** $\to$ 偏置网络汇合节点 **`MID`**（ADC1_CH6 模拟采集）

### 3.2 模拟前端 ADC 隔直与 1.65V 偏置网络
```
[ ADC_IN 探针 ] ---> C1 (10uF) 负极 
                       |
             (正极) 节点 MID ---------> ESP32 D34 (Pin 27)
                       |
               +-------+-------+
               |               |
          R1 (10kΩ)       R2 (10kΩ)
               |               |
             [5V]            [GND]
```
* **`ADC_IN` 探针** $\to$ `C1` (10µF) 负极
* **`MID` 节点** $\to$ `C1` 正极、`R1` 底部、`R2` 顶部、ESP32 `D34` 四者汇合
* **`R1` 顶端** $\to$ 接 `5V` 电源（或 `Vin` 引脚）
* **`R2` 底端** $\to$ 接 `GND` 地

### 3.3 显示屏：ST7789 2.4寸 IPS 模组 (8-Pin)
* **Pin 1 (`GND`)** $\to$ 接 `GND`
* **Pin 2 (`VCC`)** $\to$ 接 `5V`
* **Pin 3 (`SCL`)** $\to$ 接 ESP32 `D18` (VSPI_CLK)
* **Pin 4 (`SDA`)** $\to$ 接 ESP32 `D23` (VSPI_MOSI)
* **Pin 5 (`RST`)** $\to$ 接 ESP32 `D4`
* **Pin 6 (`DC`)**  $\to$ 接 ESP32 `D19` (VSPI_MISO)
* **Pin 7 (`CS`)**  $\to$ 接 ESP32 `D5`
* **Pin 8 (`BL`)**  $\to$ 接 ESP32 `D15`

### 3.4 音频解码：PCM5102A DAC 紫色模组 (15-Pin)
* **Pin 1 (`VIN`)**  $\to$ 接 `5V`
* **Pin 2 (`GND`)**  $\to$ 接 `GND`
* **Pin 3 (`LRCK`)** $\to$ 接 ESP32 `D25` (I2S WS/LCK)
* **Pin 4 (`DIN`)**  $\to$ 接 ESP32 `TX2` / `GPIO17` (I2S DATA)
* **Pin 5 (`BCK`)**  $\to$ 接 ESP32 `D26` (I2S BCLK)
* **Pin 6 (`SCK`)**  $\to$ **直连 `GND`**（使能内部 PLL 自动生成主时钟）
* **Pin 12 (`AGND`)** $\to$ 接 `GND`
* **Pin 14 (`AGND`)** $\to$ 接 `GND`
* **Pin 15 (`LOUT`)** $\to$ 接 `C2` (10µF) 正极
* **Pin 13 (`ROUT`)** $\to$ 接 `C3` (10µF) 正极
* *Pin 7-11 (`FLT, DEMP, XSMT, FMT, A3V3`)* $\to$ 悬空（板载已有硬件拉低/上拉配置）

### 3.5 后级输出与蜂鸣器网络
* **左声道 (监听)**：`LOUT` (Pin 15) $\to$ `C2` (10µF) 正极；`C2` 负极 $\to$ `R3` (1kΩ) $\to$ `BUZ` Pin 2 (`Signal`)
* **右声道 (信号输出)**：`ROUT` (Pin 13) $\to$ `C3` (10µF) 正极；`C3` 负极 $\to$ `R4` (1kΩ) $\to$ **`DAC_OUT` 信号输出探针**
* **`BUZ` 蜂鸣器模组**：Pin 1 (`VCC`) 接 `3V3`，Pin 3 (`GND`) 接 `GND`

---

## 四、 软件架构设计

代码建议基于 VS Code + PlatformIO (Arduino-ESP32 框架) 开发，采用 FreeRTOS 双核任务并行机制：

### 4.1 目录结构
````text
ESP32_Audio_Scope/
├── platformio.ini         # 环境配置文件
├── include/
│   └── config.h           # GPIO 管脚定义与全局配置
└── src/
    ├── main.cpp           # 双核调度管理
    ├── dac_generator.cpp  # DDS 信号发生器 (I2S DMA)
    ├── scope_adc.cpp      # ADC Continuous DMA 采样 & DSP
    └── ui_display.cpp     # ST7789 UI 屏显与双缓冲波形绘制