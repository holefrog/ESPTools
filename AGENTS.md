# AGENTS.md — ESPTools 项目 AI 协作规则（唯一正文）

<!--
============ 放置与链接说明 ============
本文件放在 esptools 项目根目录（与 .git 同级），是唯一的规则正文。
各家垫片全部用软链接指向本文件，改 AGENTS.md 一处，全工具生效：

  GEMINI.md                       → Antigravity / Gemini CLI（优先级最高）
  CLAUDE.md                       → Claude Code
  .clinerules                     → Cline / Roo Code（VS Code 插件）
  .github/copilot-instructions.md → VS Code Copilot
  .agents/rules/00-core.md        → Antigravity 工作区规则（设为 Always On）

在项目根目录执行（目录不存在自动建；已有真实文件备份为 .bak.日期）：

link_shim() {
local target="$1" src="$2"
mkdir -p "$(dirname "$target")"
if [ -L "$target" ]; then echo "已是软链接，跳过：$target"; return; fi
if [ -e "$target" ]; then mv "$target" "$target.bak.$(date +%Y%m%d)"; echo "旧文件已备份：$target.bak.$(date +%Y%m%d)"; fi
ln -s "$src" "$target" && echo "已链接：$target -> $src"
}
link_shim GEMINI.md AGENTS.md
link_shim CLAUDE.md AGENTS.md
link_shim .clinerules AGENTS.md
link_shim .github/copilot-instructions.md AGENTS.md
link_shim .agents/rules/00-core.md ../../AGENTS.md

注意：
- 去 Antigravity 的 Customizations → Rules 把 .agents/rules/00-core.md 设为 Always On；
  之后不要在 Antigravity UI 里编辑它（会破坏软链接），改规则只改 AGENTS.md。
- VS Code 未装 AI 插件时，这些垫片零影响。
-->

> 本文件是所有 AI（Antigravity / Claude Code / Cline / Copilot / Gemini CLI 等）在本项目中的**唯一事实源**。
> 凡与本文件冲突的参数、引脚、结论，一律以本文件为准，不要自行发明。
> 详细设计文档在 `docs/`（或仓库对应目录）；`decisions/` 为决策日志，新话题先查索引。

## 0. 使用语言

- 与用户沟通用**简体中文**；先给结论再给推导；数字先行、形容词靠后。

## 1. 协作铁律（违反即算事故）

1. **用户的独立实测 / 手动追踪是最终裁决**——你的推导与用户实测冲突时，以用户实测为准，并帮他找原因，不要辩护设计。
2. **任何模型不得单方面宣布"定稿"/"完成"**——定稿需经用户实测确认（2026-10-05 的教训：曾提前宣布定稿，后被用户亲手推翻）。
3. 新结论若推翻旧决策，必须明确指出推翻的是 `decisions/` 里的哪一条、依据是什么。
4. 重要结论用条列输出：**结论 / 依据 / 影响 / 待确认项**；不确定的地方明确标注"未验证/待实测"，**严禁编造器件参数**。
5. 用户是各模型之间的唯一总线：你的关键输出会被转发给同步者合并进决策日志。

## 2. 质量红线（交付物）

1. **验证后交付**：发文件前先在自己这边充分研究、验证，一次修完，不要反复试错丢包。
2. 本机没有 KiCad 9 时，**拒绝直接生成 `.kicad_sch` 文件**（无法验证），改为交付精确的手动改动清单（删/改/增：位号、旧值→新值、位置描述）。
3. 用户给的实测文件（如 ESPTools-*.kicad_sch）是权威底版——**在它上面改，不从零重造**；脚本解析与用户目视冲突时，**信用户**。
4. KiCad 9 坐标系：lib_symbols 用 Y-up、原理图页用 Y-down；引脚 `(at x y)` 即电气连接点，`length` 只是图形 stub；实例 `(mirror x/y)` 要先做镜像变换再旋转。

## 3. 项目关键锁定事项（摘要，细节查 CONTEXT.md / docs）

- **MCU**：ESP32-S3 SuperMini（经典版 ADC DMA 会吃掉一路 I2S，MAX98357A 无家可归，故弃用）。
- **GPIO**（以实测为准）：GP1=ADC 输入｜GP2=4053.B（Y 输出档位）｜GP3=4053.A（X 输入档位）｜GP4=I2S LRCK｜GP5=I2S DOUT→PCM5102A｜GP6=I2S BCLK｜GP7=EC11 PUSH｜GP8=显示 DC｜GP9=显示 SDA｜GP10=显示 SCL｜GP11=I2S DOUT→MAX98357A｜GP12=EC11 A｜GP13=EC11 B｜TX/RX 悬空。
- **音频铁律**：MAX98357A 的 SD 接 5V 时只播**左声道**——固件必须把单声道放左槽或发 L=R，否则喇叭静音。I2S0 时钟常开、静音=DMA 喂零、切采样率先停 I2S1；**I2S1 从机 TX 稳定性待实测**。
- **模拟前端状态（2026-10-08）**：10-05"可定稿"结论已作废——4053 关断通道的 ESD 二极管经 R8 钳住节点 A，0.1× 档大信号被压缩。修复双分支待用户二选一：A=AQY212S PhotoMOS 保 1×（需下单）；B=单抽头 0.12×（零新增器件，丢约 18dB 小信号分辨率，固件须开 ADC range extension）。**PCB 布局暂停，等分支选定。**
- **固件约定**：开机零点校准（悬空/短接采约 100 点取平均存 NVS）；ADC 逐档校准（S3 非线性）。

## 4. 已知风险（动手时别丢）

1. PCM5102A 模块 A3V3 引脚定义按实物核对（防倒灌）。2. 各模块 1 脚方向对实物核对。
3. C7=100nF 紧挨 U1.VDD；I2S 时钟走线避开模拟区。4. Deep sleep + 20V 过载时钳位电流约 3.5mA 无负载吸收（概率极低）。

## 5. 跨工具说明

- 本文件即正文；仓库里其他位置的规则文件（`.agents/rules/00-core.md`、`GEMINI.md`、`CLAUDE.md`、`.clinerules`、`.github/copilot-instructions.md`）都是软链接到本文件的垫片，内容与本文件完全一致；若链接断裂导致内容不一致，以本文件为准。
