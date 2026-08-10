/**
 * @file    main.cpp
 * @brief   硬件自测程序 — 第一步
 *
 * 目标：
 *   1. [屏幕测试] ST7789 2.4" SPI IPS 屏幕 —— 显示彩色图案 + 文字 + 帧率
 *   2. [音频测试] PCM5102A I2S DAC    —— 输出 440 Hz（A4）正弦波，持续 2 秒
 *                                         然后输出 1000 Hz，持续 2 秒，交替循环
 *
 * 引脚对照（来自 README.md）：
 *   ST7789  : MOSI=23, SCK=18, CS=5, DC=19, RST=4, BL=15
 *   PCM5102A: DIN=17, LRCK=25, BCK=26  (SCK 脚接 GND, 使能内部 PLL)
 *   音频LED : GPIO 2 (板载蓝色LED，有波形输出时灯亮)
 *
 * 成功判断：
 *   - 屏幕依次显示红/绿/蓝纯色背景，并在中间打印测试信息
 *   - 蜂鸣器（左声道）发出可听的交替音调
 *   - 串口打印帧率与测试状态
 */

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <driver/i2s_std.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────────────────────
//  常量定义
// ─────────────────────────────────────────────────────────────────────────────

// I2S / PCM5102A
#define I2S_PORT        I2S_NUM_0
#define I2S_DIN_PIN     17   // Data  -> PCM5102 DIN
#define I2S_LRCK_PIN    25   // WS    -> PCM5102 LRCK
#define I2S_BCK_PIN     26   // BCLK  -> PCM5102 BCK
#define SAMPLE_RATE     44100
#define I2S_BUFFER_LEN  512  // 每次写入的采样点数（双声道，每点 32-bit）

// 音频活动指示 LED（板载蓝色 LED，DOIT DevKit V1 接在 GPIO 2）
// 注意：TFT_DC 已移至 GPIO 19，I2S_DIN 已移至 GPIO 17，释放了 I2C 默认引脚 (21/22)。GPIO 2 现专用于此 LED
#define LED_AUDIO_PIN   2

// 正弦波
#define AMPLITUDE       (30000.0f)  // 接近最大音量 (16-bit 满量程为 32767)
#define TWO_PI_F        (2.0f * 3.14159265358979f)

// 测试频率（交替）
static const float TEST_FREQS[]  = {440.0f, 880.0f, 1000.0f};
static const char* FREQ_LABELS[] = {"440 Hz (A4)", "880 Hz (A5)", "1000 Hz"};
static const int   NUM_FREQS     = 3;

// ─────────────────────────────────────────────────────────────────────────────
//  全局对象
// ─────────────────────────────────────────────────────────────────────────────
TFT_eSPI tft;

// ─────────────────────────────────────────────────────────────────────────────
//  I2S 初始化
// ─────────────────────────────────────────────────────────────────────────────
i2s_chan_handle_t tx_chan;

void i2s_init() {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // 防止DMA下溢时重复播放最后一段声音
    esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] 创建通道失败: %s\n", esp_err_to_name(err));
        return;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)I2S_BCK_PIN,
            .ws   = (gpio_num_t)I2S_LRCK_PIN,
            .dout = (gpio_num_t)I2S_DIN_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // 强制使用 32-bit 槽宽 (BCLK = 64Fs = 2.82MHz) 以确保某些 PCM5102A 模块的内部 PLL 能够稳定锁定
    // 数据仍然是 16-bit，I2S 控制器会自动对齐 MSB。
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;

    err = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (err != ESP_OK) {
        Serial.printf("[I2S] 标准模式初始化失败: %s\n", esp_err_to_name(err));
        return;
    }

    err = i2s_channel_enable(tx_chan);
    if (err != ESP_OK) {
        Serial.printf("[I2S] 通道使能失败: %s\n", esp_err_to_name(err));
        return;
    }
    Serial.println("[I2S] (V5 API) 初始化完成 ✓");
}

// ─────────────────────────────────────────────────────────────────────────────
//  生成并发送一个正弦波音调
//  freq_hz : 频率（Hz）
//  duration_ms : 持续时间（毫秒）
// ─────────────────────────────────────────────────────────────────────────────
void play_tone(float freq_hz, uint32_t duration_ms) {
    static int16_t buf[I2S_BUFFER_LEN * 2]; // 左右声道交替，乘 2
    uint32_t total_samples = (uint64_t)SAMPLE_RATE * duration_ms / 1000;
    uint32_t written_samples = 0;
    float    phase = 0.0f;
    float    phase_inc = TWO_PI_F * freq_hz / (float)SAMPLE_RATE;

    // 有波形输出时点亮音频活动 LED
    digitalWrite(LED_AUDIO_PIN, HIGH);

    while (written_samples < total_samples) {
        uint32_t chunk = I2S_BUFFER_LEN;
        if (written_samples + chunk > total_samples)
            chunk = total_samples - written_samples;

        for (uint32_t i = 0; i < chunk; i++) {
            int16_t sample = (int16_t)(sinf(phase) * AMPLITUDE);
            buf[i * 2]     = sample; // 左声道（蜂鸣器）
            buf[i * 2 + 1] = sample; // 右声道（DAC_OUT）
            phase += phase_inc;
            if (phase >= TWO_PI_F) phase -= TWO_PI_F;
        }

        size_t bytes_written = 0;
        i2s_channel_write(tx_chan, buf, chunk * 2 * sizeof(int16_t),
                          &bytes_written, portMAX_DELAY);
        written_samples += chunk;
    }

    // 波形输出结束后熄灭 LED
    digitalWrite(LED_AUDIO_PIN, LOW);
}

// ─────────────────────────────────────────────────────────────────────────────
//  屏幕测试：全屏填充纯色 + 中间打印文字
// ─────────────────────────────────────────────────────────────────────────────
void screen_color_test(uint16_t color, const char* label, uint32_t hold_ms) {
    tft.fillScreen(color);
    tft.setTextColor(TFT_WHITE, color);
    tft.setTextSize(2);
    tft.setCursor(10, 130);
    tft.printf("Color: %s", label);
    tft.setCursor(10, 160);
    tft.printf("ST7789 TEST OK");
    delay(hold_ms);
}

// ─────────────────────────────────────────────────────────────────────────────
//  屏幕测试：绘制彩色竖条纹
// ─────────────────────────────────────────────────────────────────────────────
void screen_stripe_test() {
    const uint16_t colors[] = {
        TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN,
        TFT_CYAN, TFT_BLUE,  TFT_MAGENTA, TFT_WHITE
    };
    int n = 8;
    int w = tft.width() / n;
    for (int i = 0; i < n; i++) {
        tft.fillRect(i * w, 0, w, tft.height(), colors[i]);
    }
    delay(1000);
}

// ─────────────────────────────────────────────────────────────────────────────
//  屏幕测试：帧率测量
// ─────────────────────────────────────────────────────────────────────────────
void screen_fps_test() {
    tft.fillScreen(TFT_BLACK);
    uint32_t t0 = millis();
    int frames = 0;
    while (millis() - t0 < 2000) {
        tft.fillScreen((frames % 2 == 0) ? TFT_NAVY : TFT_DARKGREEN);
        frames++;
    }
    uint32_t elapsed = millis() - t0;
    float fps = (float)frames * 1000.0f / elapsed;

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 100);
    tft.printf("FPS Test:");
    tft.setCursor(10, 130);
    tft.printf("%.1f fps", fps);
    tft.setCursor(10, 160);
    tft.printf("%d frames/2s", frames);
    Serial.printf("[屏幕] 帧率测试: %.1f fps (%d frames in %u ms)\n",
                  fps, frames, elapsed);
    delay(2000);
}

// ─────────────────────────────────────────────────────────────────────────────
//  setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // 初始化音频活动 LED（板载蓝色 LED，GPIO 2）
    pinMode(LED_AUDIO_PIN, OUTPUT);
    digitalWrite(LED_AUDIO_PIN, LOW);  // 默认熄灭

    delay(500);
    Serial.println("\n============================================");
    Serial.println("  ESP32 硬件自测 — 屏幕 + PCM5102A DAC");
    Serial.println("============================================");

    // ── 1. 初始化屏幕 ──────────────────────────────────────────────────────
    Serial.println("[屏幕] 初始化 ST7789...");
    tft.init();
    tft.setRotation(0);          // 竖屏 240x320
    tft.fillScreen(TFT_BLACK);
    Serial.printf("[屏幕] 分辨率: %d x %d ✓\n", tft.width(), tft.height());

    // 欢迎界面
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 80);
    tft.println("ESP32 HW Test");
    tft.setCursor(20, 110);
    tft.println("Screen: ST7789");
    tft.setCursor(20, 140);
    tft.println("Audio: PCM5102A");
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setCursor(20, 180);
    tft.println("Starting...");
    delay(1500);

    // ── 2. 初始化 I2S / PCM5102 ───────────────────────────────────────────
    i2s_init();

    Serial.println("[测试] 开始屏幕颜色测试...");
}

// ─────────────────────────────────────────────────────────────────────────────
//  loop —— 交替进行屏幕测试和音频测试
// ─────────────────────────────────────────────────────────────────────────────
static int test_phase = 0;
static int freq_idx   = 0;

void loop() {
    switch (test_phase) {

    // ── 阶段 0：纯色测试 ────────────────────────────────────────────────────
    case 0:
        Serial.println("[屏幕] 纯色测试: RED / GREEN / BLUE");
        screen_color_test(TFT_RED,   "RED",   800);
        screen_color_test(TFT_GREEN, "GREEN", 800);
        screen_color_test(TFT_BLUE,  "BLUE",  800);
        test_phase++;
        break;

    // ── 阶段 1：彩色条纹 ────────────────────────────────────────────────────
    case 1:
        Serial.println("[屏幕] 彩色条纹测试");
        screen_stripe_test();
        test_phase++;
        break;

    // ── 阶段 2：帧率测试 ────────────────────────────────────────────────────
    case 2:
        screen_fps_test();
        test_phase++;
        break;

    // ── 阶段 3：音频测试（循环播放各频率）───────────────────────────────────
    case 3: {
        float freq  = TEST_FREQS[freq_idx];
        const char* label = FREQ_LABELS[freq_idx];

        Serial.printf("[音频] 播放 %s, 持续 2 秒...\n", label);

        // 屏幕同步显示音频信息
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 60);
        tft.printf("Audio Test");
        tft.setCursor(10, 100);
        tft.printf("PCM5102A DAC");
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(10, 140);
        tft.printf("Freq: %s", label);
        tft.setCursor(10, 175);
        tft.printf("Listen: BUZ OUT");
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 220);
        tft.printf("Playing...");

        // 播放音调
        play_tone(freq, 2000);

        freq_idx = (freq_idx + 1) % NUM_FREQS;

        // 播放完一轮后重回屏幕测试
        if (freq_idx == 0) {
            Serial.println("[测试] 完成一个完整循环，重新开始...");
            Serial.println("--------------------------------------------");
            test_phase = 0;
        }
        break;
    }

    default:
        test_phase = 0;
        break;
    }
}
