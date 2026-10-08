# PCM5102 / PCM5102A I²S DAC 模块
## Raspberry Pi Zero 实用操作 Datasheet

> **用途**：将 Raspberry Pi 的 I²S 数字音频转换为模拟立体声音频输出。  
> **目标平台**：Raspberry Pi Zero / Zero W / Zero 2 W  
> **接口协议**：I²S  
> **DAC**：PCM5102 / PCM5102A 系列  
>
> ⚠️ **重要**：市售 PCM5102 模块 PCB 版本很多。本文针对常见的带 `SCK / BCK / LRCK / DIN` 接口、背面带 `H1~H4` 配置焊桥的模块。具体焊盘布局应以实物为准。

---

# 1. 快速配置

如果目标是：

> **Raspberry Pi → I²S → PCM5102 → 左右模拟音频输出**

推荐配置：

```text
H1 = LOW
H2 = LOW
H3 = HIGH
H4 = LOW
```

即：

| 配置 | 状态 | 功能 |
|---|---:|---|
| H1 / FLT | **L** | Normal Latency Filter |
| H2 / DEMP | **L** | De-emphasis Disabled |
| H3 / XSMT | **H** | Soft Mute Disabled |
| H4 / FMT | **L** | I²S Format |

---

# 2. Raspberry Pi Zero 接线

## 2.1 推荐接线

| PCM5102 模块 | Raspberry Pi Zero | Pin | 功能 |
|---|---|---:|---|
| **VIN** | 5V | 2 或 4 | 模块供电 |
| **GND** | GND | 6 | 地 |
| **BCK** | GPIO18 | 12 | I²S Bit Clock |
| **LRCK / LCK** | GPIO19 | 35 | I²S Left/Right Clock |
| **DIN** | GPIO21 | 40 | I²S Audio Data |
| **SCK** | **GND / 模块 SCK-OPEN 焊桥** | — | MCLK/SCK，不由 Pi 提供 |

核心关系：

```text
Raspberry Pi                    PCM5102
────────────────────────────────────────────

GPIO18 / PCM_CLK ──────────────> BCK
GPIO19 / PCM_FS  ──────────────> LRCK
GPIO21 / PCM_DOUT──────────────> DIN

5V ────────────────────────────> VIN
GND ───────────────────────────> GND

                               SCK ──> GND
```

---

# 3. 最重要的区别：SCK ≠ BCK

PCM5102 上有两个容易混淆的时钟：

```text
SCK  = System Clock / MCLK
BCK  = Bit Clock
```

它们不是同一个信号。

## Raspberry Pi 对应关系

```text
Pi GPIO18
   │
   └──── PCM_CLK
              │
              ▼
           PCM5102 BCK
```

所以：

> **BCK 接 GPIO18。**

绝对不要：

```text
BCK ─── GND    ❌
```

正确：

```text
GPIO18 ─── BCK    ✅
```

而可以拉低的是：

```text
SCK ─── GND       ✅
```

---

# 4. 为什么 SCK 可以接 GND？

PCM5102 可以利用内部 PLL 产生 DAC 所需要的系统时钟。

因此在这种应用中：

```text
外部 MCLK / SCK
       ↓
    不需要
```

PCM5102 可以工作在：

```text
SCK = LOW
     ↓
内部 PLL
     ↓
内部系统时钟
```

因此 Raspberry Pi 不需要额外提供 MCLK。

这对 Raspberry Pi 很方便，因为标准 Raspberry Pi I²S 音频接口主要提供：

```text
BCK
LRCK
DATA
```

而不需要额外占用一个 GPIO 输出 MCLK。

---

# 5. SCK 旁边的 OPEN 焊盘

很多 PCM5102 模块在 SCK 附近可以看到类似：

```text
SCK

[ OPEN ]
```

或者：

```text
SCK
 ├── OPEN
 └── GND
```

这个焊桥通常用于：

> **把 SCK 拉到 GND，从而启用 PCM5102 的内部 PLL 工作方式。**

如果你的 PCB 设计确实是这种结构，那么：

```text
OPEN 未短接
    ↓
SCK 保持外部输入状态

OPEN 短接
    ↓
SCK = GND
    ↓
使用内部 PLL
```

## 推荐

如果使用 Raspberry Pi Zero，并且该模块的 `OPEN` 焊桥确实对应 SCK-to-GND：

```text
OPEN → 焊锡短接
```

然后：

```text
PCM5102 SCK
    ↓
GND
```

**不要把 BCK 接到这个 OPEN 焊盘。**

## 实物板（紫色模组）的 SCK 接地方式

常见的紫色 PCM5102A 模块（淘宝款）正面右上角，**SCK 引脚接线柱旁边**有一个独立的小圆形裸铜焊盘：

```text
正面引脚排列（从右上往左看）：

  VIN  GND  LRCK  DIN  BCK  SCK
                            |
                        [小圆焊盘]  ← 此处即 SCK-to-GND 焊桥
```

该小焊盘与板上 GND 铜皮相连。有两种方式把 SCK 接地：

| 方式 | 做法 | 适用场景 |
|------|------|----------|
| **外部引线** | 用导线将 SCK 引脚接线柱连到 GND 接线柱 | 面包板搭线，灵活方便 |
| **焊盘短接** | 用焊锡把 SCK 引脚焊盘与旁边小圆焊盘桥接 | PCB 焊接，免去外部连线 |

两种方式电气效果完全相同，均可使 PCM5102A 进入内部 PLL 模式。

---

---

# 6. HxL 焊桥的读法（三焊盘跳线桥）

背面的 H1L / H2L / H3L / H4L 每一个都是一个**三焊盘跳线桥**，结构如下：

```text
       H       x       L
    [左盘] ─ [中盘] ─ [右盘]
      │                 │
    HIGH               LOW
```

- **中盘（x）** 是公共信号端，始终连接到芯片对应引脚
- **左盘（H）** 内部连接 VDD（高电平）
- **右盘（L）** 内部连接 GND（低电平）

### 操作规则

| 焊锡位置 | 短接的两盘 | 引脚电平 |
|---------|-----------|----------|
| 焊左侧  | H + x     | **HIGH** |
| 焊右侧  | x + L     | **LOW**  |

> ⚠️ 每次只能短接一侧，不可同时短接左右两侧。

### 实物确认（紫色模组出厂状态）

以实物拍摄的背面图为准，四个焊桥出厂状态：

| 焊桥 | 焊锡位置 | 实际电平 | 功能含义 |
|------|---------|---------|----------|
| **H1L** | 右侧（x+L）短接 | **LOW**  | FLT: Normal Latency Filter ✅ |
| **H2L** | 右侧（x+L）短接 | **LOW**  | DEMP: De-emphasis 关闭 ✅ |
| **H3L** | **左侧（H+x）短接** | **HIGH** | XSMT: 软件静音**关闭**，正常输出 ✅ |
| **H4L** | 右侧（x+L）短接 | **LOW**  | FMT: 标准 I²S 格式 ✅ |

这正是推荐的出厂默认配置，**无需修改**即可直接用于 ESP32 / Raspberry Pi 的 I²S 接入。

### 如何修改

若需要改变某一配置，例如将 H1 从 LOW 改为 HIGH：

1. 用吸锡线或热风枪去掉右侧焊锡
2. 在左侧两盘（H+x）加焊锡短接

```text
改前：[ H ] ─ [ x ]━━━[ L ]   → LOW
改后：[ H ]━━━[ x ] ─ [ L ]   → HIGH
```

---

# 7. H1～H4 配置

## H1 — FLT

```text
FLT = Filter Select
```

推荐：

```text
H1 = LOW
```

含义：

```text
LOW → Normal Latency Filter
HIGH → Low Latency Filter
```

普通音乐播放推荐：

```text
H1L
```

---

## H2 — DEMP

```text
DEMP = De-emphasis
```

推荐：

```text
H2 = LOW
```

即：

```text
De-emphasis = OFF
```

普通现代数字音乐播放一般使用：

```text
H2L
```

---

## H3 — XSMT

```text
XSMT = Soft Mute
```

正常播放时：

```text
H3 = HIGH
```

即：

```text
HIGH → Soft Mute Disabled
LOW  → Soft Mute Enabled
```

因此正常播放：

```text
H3H
```

⚠️ 注意：

```text
H3L ≠ 正常播放
```

`LOW` 会使 DAC 进入 mute 状态。

---

## H4 — FMT

```text
FMT = Audio Format
```

推荐：

```text
H4 = LOW
```

即：

```text
LOW → I²S
HIGH → Left Justified
```

Raspberry Pi 标准 I²S：

```text
H4L
```

---

# 7. 推荐焊桥状态

最终推荐：

```text
┌────────────────────────────┐
│ PCM5102 Configuration      │
│                            │
│ H1 = L                    │
│ H2 = L                    │
│ H3 = H                    │
│ H4 = L                    │
│                            │
│ SCK = GND                 │
└────────────────────────────┘
```

即：

```text
H1L
H2L
H3H
H4L
```

---

# 8. 完整硬件连接图

```text
                    Raspberry Pi Zero
                 ┌─────────────────────┐
                 │                     │
        5V ──────┤ Pin 2               │
                 │                     │
       GND ──────┤ Pin 6               │
                 │                     │
GPIO18/PCM_CLK ──┤ Pin 12              │
                 │                     │
 GPIO19/PCM_FS ──┤ Pin 35              │
                 │                     │
GPIO21/PCM_DOUT ─┤ Pin 40              │
                 │                     │
                 └─────────────────────┘
                      │
                      │
                      ▼

                ┌──────────────────┐
                │    PCM5102       │
                │                  │
5V ────────────>│ VIN              │
GND ───────────>│ GND              │
GPIO18 ────────>│ BCK              │
GPIO19 ────────>│ LRCK             │
GPIO21 ────────>│ DIN              │
                │                  │
GND ───────────>│ SCK              │
                │                  │
                │     L ─────────> Left
                │     R ─────────> Right
                └──────────────────┘
```

---

# 9. 不要这样接

## 错误 1：BCK 接 GND

```text
BCK ─── GND    ❌
```

结果：

```text
没有 Bit Clock
    ↓
DAC 无法正确接收 I²S 数据
```

---

## 错误 2：GPIO18 接 SCK

不要把：

```text
Pi GPIO18
```

理解成：

```text
PCM5102 SCK
```

正确关系是：

```text
GPIO18 / PCM_CLK
        ↓
PCM5102 BCK
```

---

## 错误 3：H3 设置 LOW

如果希望正常播放：

```text
H3 = HIGH
```

而不是：

```text
H3 = LOW
```

---

## 错误 4：H4 设置 HIGH

如果 Raspberry Pi 输出标准 I²S：

```text
H4 = LOW
```

---

# 10. 上电前检查表

在第一次上电之前，逐项检查：

```text
[ ] VIN 接 5V
[ ] GND 接 Raspberry Pi GND
[ ] BCK 接 GPIO18
[ ] LRCK 接 GPIO19
[ ] DIN 接 GPIO21
[ ] BCK 没有接 GND
[ ] SCK 没有误接 GPIO18
[ ] SCK 根据模块设计拉到 GND
[ ] H1 = LOW
[ ] H2 = LOW
[ ] H3 = HIGH
[ ] H4 = LOW
```

特别检查：

```text
BCK → GPIO18
LRCK → GPIO19
DIN → GPIO21
```

---

# 11. Raspberry Pi GPIO 对应表

```text
Raspberry Pi Zero 40-pin Header

Pin 12 ─ GPIO18 ─ PCM_CLK ──> PCM5102 BCK

Pin 35 ─ GPIO19 ─ PCM_FS  ──> PCM5102 LRCK

Pin 40 ─ GPIO21 ─ PCM_DOUT ─> PCM5102 DIN

Pin 2  ─ 5V ────────────────> PCM5102 VIN

Pin 6  ─ GND ───────────────> PCM5102 GND
```

---

# 12. I²S 信号概念

PCM5102 接收到：

```text
BCK
 │
 │    ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
 └────┘ └─┘ └─┘ └─┘ └─┘
      Bit Clock

LRCK
 ────────────────┐
                 │
                 └──────────────
       LEFT            RIGHT

DIN
 ── audio data ─────────────────>
```

三个主要 I²S 信号：

```text
BCK   = Bit Clock
LRCK  = Left/Right Clock
DIN   = Digital Audio Data
```

---

# 13. 模块工作模式

推荐模式：

```text
                  Raspberry Pi
                       │
              ┌────────┼────────┐
              │        │        │
             BCK      LRCK     DATA
              │        │        │
              ▼        ▼        ▼
          ┌────────────────────────┐
          │       PCM5102          │
          │                        │
SCK=LOW → │ Internal PLL           │
          │                        │
          │       DAC              │
          └───────────┬────────────┘
                      │
                 Analog Audio
                  ┌────┴────┐
                  ▼         ▼
                  L         R
```

---

# 14. 推荐配置总结

## 硬件

```text
VIN  → 5V
GND  → GND
BCK  → GPIO18
LRCK → GPIO19
DIN  → GPIO21
SCK  → GND
```

## 焊桥

```text
H1 → L
H2 → L
H3 → H
H4 → L
```

## SCK

```text
不需要 Raspberry Pi 提供 MCLK
       ↓
SCK 拉低
       ↓
使用 PCM5102 内部 PLL
```

---

# 15. 故障排查

## 有电但完全没有声音

首先检查：

```text
GPIO18 → BCK
GPIO19 → LRCK
GPIO21 → DIN
```

然后检查：

```text
H4 = LOW
```

即 I²S 模式。

---

## 有噪声 / 爆音 / 数据异常

重点检查：

```text
BCK
LRCK
DIN
GND
```

尤其是：

```text
GPIO18 ↔ BCK
GPIO19 ↔ LRCK
GPIO21 ↔ DIN
```

以及 GND 是否可靠。

---

## 完全静音

检查：

```text
H3 = HIGH
```

因为：

```text
H3 = LOW
```

可能处于 Soft Mute。

---

## Pi 没有 MCLK/SCK 怎么办？

通常不需要额外处理。

采用：

```text
PCM5102 SCK = LOW
```

让 PCM5102 内部 PLL 工作。

---

# 16. 最终“照着做”版本

如果你现在拿着模块和 Raspberry Pi Zero，可以按下面做：

### Step 1

把 PCM5102 的：

```text
H1 → L
H2 → L
H3 → H
H4 → L
```

配置好。

### Step 2

确认模块上的 `SCK / OPEN` 焊桥。

如果该 `OPEN` 焊桥明确是：

```text
SCK ↔ GND
```

则：

```text
焊锡短接 OPEN
```

使：

```text
SCK = GND
```

### Step 3

接线：

```text
Pi Pin 2  → PCM5102 VIN
Pi Pin 6  → PCM5102 GND

Pi Pin 12 → PCM5102 BCK
Pi Pin 35 → PCM5102 LRCK
Pi Pin 40 → PCM5102 DIN
```

### Step 4

确认：

```text
BCK ≠ GND
```

而：

```text
SCK = GND
```

### Step 5

最后再给 Raspberry Pi 上电。

---

# 17. 一张表记住全部内容

| 项目 | PCM5102 | Raspberry Pi Zero |
|---|---|---|
| 电源 | VIN | 5V |
| 地 | GND | GND |
| Bit Clock | **BCK** | **GPIO18 / Pin 12** |
| Left/Right Clock | **LRCK** | **GPIO19 / Pin 35** |
| Audio Data | **DIN** | **GPIO21 / Pin 40** |
| System Clock | **SCK** | **不需要 Pi 提供** |
| SCK 状态 | **LOW / GND** | — |
| FLT | H1 = L | Normal Latency |
| DEMP | H2 = L | De-emphasis OFF |
| XSMT | H3 = H | Soft Mute OFF |
| FMT | H4 = L | I²S |

---

## 最重要的三句话

```text
① GPIO18 → BCK，不是 SCK。

② GPIO19 → LRCK，GPIO21 → DIN。

③ SCK 可以拉到 GND，让 PCM5102 使用内部 PLL；
   但 BCK 绝对不能接地。
```

> **关于 `SCK` 旁边标有 `OPEN` 的焊盘：只有在确认该 PCB 的 `OPEN` 确实是 SCK-to-GND 配置焊桥后，才把它短接。不同厂家的 PCM5102 模块 PCB 并不完全相同。**