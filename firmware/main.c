/**
 * Single File RGB Spectrum Analyzer
 * Hardware: nrf52832, Radio, no PMIC
 **/

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_pwm.h"

#pragma region Configuration & Pin Definitions
// --------------------------------------------------------------------------
// Configuration & Pin Definitions
// --------------------------------------------------------------------------

// Button & Input Pins
#define PIN_BTN_LEFT 5
#define PIN_BTN_MID 6
#define PIN_BTN_RIGHT 7
#define PIN_ADC_INPUT 2
#define PIN_CHRG_STAT 3

// UART Pins (Reserved)
#define PIN_UART_TX 6
#define PIN_UART_RX 8

// Spectrum Analyzer Settings
#define SCAN_BASE_FREQ 2400 // in MHz
#define SCAN_START_FREQ 0
#define SCAN_END_FREQ 87
#define BANDWIDTH (SCAN_END_FREQ - SCAN_START_FREQ + 1)

// WS2812B LED Configuration
#define PIN_LED_DATA 4
#define NUM_LEDS 160
#define WS2812_PWM_TOP       20
#define WS2812_T0H_TICKS      5
#define WS2812_T1H_TICKS     14
#define WS2812_RESET_SLOTS   80
#define WS2812_POLARITY_BIT 0x8000u

// Display Modes
#define LED_MODE_INTENSITY_COLOR 0 // 强度对应颜色 (绿->红)
#define LED_MODE_FREQ_RAINBOW    1 // 频点对应彩虹色，强度对应亮度
#define LED_MODE_TEST_RAINBOW    2 // 测试模式：彩虹流光溢彩
#define LED_MODE_FREQ_RAINBOW_PEAK 3 // 频点彩虹+峰值保持衰减亮度
// 不同模式的 dBm 映射区间
#define LED_DBM_MIN_INTENSITY_COLOR   (-95.0f)
#define LED_DBM_MAX_INTENSITY_COLOR   (-75.0f)
#define LED_DBM_MIN_FREQ_RAINBOW      (-90.0f)
#define LED_DBM_MAX_FREQ_RAINBOW      (-20.0f)
// 不同模式的亮度配置(0.0~1.0)
#define LED_BRIGHTNESS_INTENSITY_COLOR        0.06f // 强度对应颜色的亮度
#define LED_BRIGHTNESS_TEST_RAINBOW           0.20f
#define LED_BRIGHTNESS_FREQ_RAINBOW_MIN       0.03f // 彩虹模式最低亮度

#define LED_BRIGHTNESS_FREQ_RAINBOW_MAX       0.80f
#define LED_BRIGHTNESS_FREQ_RAINBOW_PEAK_MIN  0.03f
#define LED_BRIGHTNESS_FREQ_RAINBOW_PEAK_MAX  0.80f
// IntensityColor 模式颜色范围（HSV Hue 角度）
// 默认：低强度=绿色(120)，高强度=红色(0)
// 常见 Hue 对照：0=红, 30=橙, 60=黄, 120=绿, 180=青, 240=蓝, 270=紫, 300=品红
#define LED_INTENSITY_COLOR_HUE_LOW_DBM       180.0f
#define LED_INTENSITY_COLOR_HUE_HIGH_DBM      0.0f
// 峰值保持配置（单位：扫描帧）
// 缺省配置供不使用峰值显示细分调参的模式复用（如 FreqRainbow/TestRainbow）
#define LED_PEAK_HOLD_FRAMES_DEFAULT                    1
#define LED_PEAK_DECAY_INTERVAL_FRAMES_DEFAULT          1
#define LED_PEAK_DECAY_STEP_DEFAULT                     8

#define LED_PEAK_HOLD_FRAMES_INTENSITY_COLOR            3 // 保留多少帧
#define LED_PEAK_DECAY_INTERVAL_FRAMES_INTENSITY_COLOR  1 // 每隔多少帧衰减一次
#define LED_PEAK_DECAY_STEP_INTENSITY_COLOR             1 // 衰减多少

#define LED_PEAK_HOLD_FRAMES_FREQ_RAINBOW_PEAK           2
#define LED_PEAK_DECAY_INTERVAL_FRAMES_FREQ_RAINBOW_PEAK 1
#define LED_PEAK_DECAY_STEP_FREQ_RAINBOW_PEAK            2
#if LED_PEAK_DECAY_INTERVAL_FRAMES_DEFAULT < 1 || LED_PEAK_DECAY_STEP_DEFAULT < 1
#error "Default peak decay config must be >= 1"
#endif
#if LED_PEAK_DECAY_INTERVAL_FRAMES_INTENSITY_COLOR < 1 || LED_PEAK_DECAY_STEP_INTENSITY_COLOR < 1
#error "IntensityColor peak decay config must be >= 1"
#endif
#if LED_PEAK_DECAY_INTERVAL_FRAMES_FREQ_RAINBOW_PEAK < 1 || LED_PEAK_DECAY_STEP_FREQ_RAINBOW_PEAK < 1
#error "FreqRainbowPeak peak decay config must be >= 1"
#endif

#define LED_DEFAULT_DISPLAY_MODE LED_MODE_FREQ_RAINBOW_PEAK
#define LED_MODE_COUNT 4

// DFU Magic Number (Standard Nordic SDK value)
#define BOOTLOADER_DFU_START 0xB1


#pragma endregion Configuration & Pin Definitions

#pragma region Global Variables
// --------------------------------------------------------------------------
// Global Variables
// --------------------------------------------------------------------------

// The screen buffer
static uint8_t m_frame_buffer[1024];

// array with RSSI values for each frequency in the scan range, normally 20..90 (meaning -20dBm .. -90dBm)
static uint8_t m_rssi_current[BANDWIDTH];
// array with peak RSSI values for each frequency in the scan range. Both will use gradual falloff.
static uint8_t m_rssi_peak[BANDWIDTH];
static float m_rssi_floating[BANDWIDTH];
static uint16_t m_rssi_peak_hold_frames[BANDWIDTH];

// PWM buffer for WS2812B (24 bits per LED + Reset pulse)
static uint16_t m_led_pwm_data[NUM_LEDS * 24 + WS2812_RESET_SLOTS];
// 简单的 RGB 结构体
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color_t;

static uint8_t m_led_display_mode = LED_DEFAULT_DISPLAY_MODE;
static bool m_led_output_enabled = true;
static const char *m_led_mode_names[LED_MODE_COUNT] =
{
    "IntensityColor",
    "FreqRainbow",
    "TestRainbow",
    "FreqRainbowPeak",
};

#pragma endregion Global Variables

#pragma region uart
void uart_init(void)
{
    nrf_gpio_cfg_output(PIN_UART_TX);
    nrf_gpio_cfg_input(PIN_UART_RX, NRF_GPIO_PIN_PULLUP);

    NRF_UART0->PSELTXD = PIN_UART_TX;
    NRF_UART0->PSELRXD = PIN_UART_RX;
    NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud460800;
    NRF_UART0->ENABLE = UART_ENABLE_ENABLE_Enabled;
    NRF_UART0->TASKS_STARTTX = 1;
    NRF_UART0->EVENTS_RXDRDY = 0;
    NRF_UART0->TASKS_STARTRX = 1;
}

void uart_print(const char *str)
{
    while (*str)
    {
        NRF_UART0->TXD = *str++;
        NRF_UART0->EVENTS_TXDRDY = 0;
        while (NRF_UART0->EVENTS_TXDRDY == 0);
    }
}

/**
 * @brief 发送原始字节流到 UART。
 **/
void uart_write(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        NRF_UART0->TXD = data[i];
        NRF_UART0->EVENTS_TXDRDY = 0;
        // 等待发送完成
        while (NRF_UART0->EVENTS_TXDRDY == 0);
    }
}

bool uart_read_byte_nonblocking(uint8_t *byte)
{
    if (NRF_UART0->EVENTS_RXDRDY == 0)
    {
        return false;
    }
    NRF_UART0->EVENTS_RXDRDY = 0;
    *byte = (uint8_t)NRF_UART0->RXD;
    return true;
}

#pragma endregion uart

#pragma region ws2812
/**
 * @brief 初始化 WS2812B 驱动 (PWM0)
 **/
void ws2812_init(void)
{
    // 此板级连线下，PWM sample 需使用 polarity bit，空闲电平先置高可避免上电毛刺
    nrf_gpio_pin_set(PIN_LED_DATA);
    nrf_gpio_cfg_output(PIN_LED_DATA);
    
    NRF_PWM0->PSEL.OUT[0] = PIN_LED_DATA;
    NRF_PWM0->ENABLE      = 1;
    NRF_PWM0->MODE        = PWM_MODE_UPDOWN_Up;
    NRF_PWM0->PRESCALER   = PWM_PRESCALER_PRESCALER_DIV_1; // 16MHz
    NRF_PWM0->COUNTERTOP  = WS2812_PWM_TOP; // 1.25us period
    NRF_PWM0->DECODER     = (PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos);
    NRF_PWM0->LOOP        = 0; // 每次只发送一帧
    NRF_PWM0->SEQ[0].REFRESH = 0;
    NRF_PWM0->SEQ[0].ENDDELAY = 0;

    // 0 duty + polarity bit(0x8000) => 低电平 reset 窗口
    for (int i = 0; i < (int)(sizeof(m_led_pwm_data) / sizeof(m_led_pwm_data[0])); i++)
    {
        m_led_pwm_data[i] = WS2812_POLARITY_BIT;
    }
}

/**
 * @brief 设置单个 LED 颜色并更新 PWM 缓冲区
 **/
void ws2812_set_led(int idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx < 0 || idx >= NUM_LEDS) return;
    uint32_t color = (g << 16) | (r << 8) | b; // WS2812B 使用 GRB 顺序
    for (int i = 0; i < 24; i++)
    {
        m_led_pwm_data[idx * 24 + i] =
            (color & (1UL << (23 - i))) ? (WS2812_T1H_TICKS | WS2812_POLARITY_BIT)
                                        : (WS2812_T0H_TICKS | WS2812_POLARITY_BIT);
    }
}

/**
 * @brief 简单的 Hue 转 RGB 转换
 **/
void hue_to_rgb(uint8_t hue, uint8_t brightness, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t pr, pg, pb;
    if (hue < 85) {
        pr = 255 - hue * 3; pg = hue * 3; pb = 0;
    } else if (hue < 170) {
        hue -= 85; pr = 0; pg = 255 - hue * 3; pb = hue * 3;
    } else {
        hue -= 170; pr = hue * 3; pg = 0; pb = 255 - hue * 3;
    }
    *r = (pr * brightness) >> 8;
    *g = (pg * brightness) >> 8;
    *b = (pb * brightness) >> 8;
}

// ==========================================
// 辅助函数：HSV 转 RGB
// h: 0-360, s: 0-1.0, v: 0-1.0
// ==========================================
Color_t hsv2rgb(float h, float s, float v) {
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 60) { r = c; g = x; b = 0; }
    else if (h >= 60 && h < 120) { r = x; g = c; b = 0; }
    else if (h >= 120 && h < 180) { r = 0; g = c; b = x; }
    else if (h >= 180 && h < 240) { r = 0; g = x; b = c; }
    else if (h >= 240 && h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    Color_t rgb;
    rgb.r = (uint8_t)((r + m) * 255);
    rgb.g = (uint8_t)((g + m) * 255);
    rgb.b = (uint8_t)((b + m) * 255);
    return rgb;
}

static float clamp01f(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static float map_brightness_range(float normalized_level, float minv, float maxv)
{
    minv = clamp01f(minv);
    maxv = clamp01f(maxv);
    if (maxv < minv) maxv = minv;
    normalized_level = clamp01f(normalized_level);
    return minv + (maxv - minv) * normalized_level;
}

static void get_mode_dbm_range(uint8_t mode, float *dbm_min, float *dbm_max)
{
    if (mode == LED_MODE_INTENSITY_COLOR)
    {
        *dbm_min = LED_DBM_MIN_INTENSITY_COLOR;
        *dbm_max = LED_DBM_MAX_INTENSITY_COLOR;
    }
    else
    {
        *dbm_min = LED_DBM_MIN_FREQ_RAINBOW;
        *dbm_max = LED_DBM_MAX_FREQ_RAINBOW;
    }
}

typedef struct
{
    uint16_t hold_frames;
    uint16_t decay_interval_frames;
    uint8_t decay_step;
} led_peak_cfg_t;

static led_peak_cfg_t get_mode_peak_cfg(uint8_t mode)
{
    led_peak_cfg_t cfg;
    switch (mode)
    {
    case LED_MODE_INTENSITY_COLOR:
        cfg.hold_frames = LED_PEAK_HOLD_FRAMES_INTENSITY_COLOR;
        cfg.decay_interval_frames = LED_PEAK_DECAY_INTERVAL_FRAMES_INTENSITY_COLOR;
        cfg.decay_step = LED_PEAK_DECAY_STEP_INTENSITY_COLOR;
        break;
    case LED_MODE_FREQ_RAINBOW_PEAK:
        cfg.hold_frames = LED_PEAK_HOLD_FRAMES_FREQ_RAINBOW_PEAK;
        cfg.decay_interval_frames = LED_PEAK_DECAY_INTERVAL_FRAMES_FREQ_RAINBOW_PEAK;
        cfg.decay_step = LED_PEAK_DECAY_STEP_FREQ_RAINBOW_PEAK;
        break;
    default:
        cfg.hold_frames = LED_PEAK_HOLD_FRAMES_DEFAULT;
        cfg.decay_interval_frames = LED_PEAK_DECAY_INTERVAL_FRAMES_DEFAULT;
        cfg.decay_step = LED_PEAK_DECAY_STEP_DEFAULT;
        break;
    }
    return cfg;
}

static float offset_deg = 0.0f; // 彩虹相位偏移（角度），使用浮点获得更丝滑过渡
void update_led_display(void)
{
    if (!m_led_output_enabled)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            ws2812_set_led(i, 0, 0, 0);
        }

        NRF_PWM0->EVENTS_SEQEND[0] = 0;
        NRF_PWM0->SEQ[0].PTR = (uint32_t)m_led_pwm_data;
        NRF_PWM0->SEQ[0].CNT = NUM_LEDS * 24 + WS2812_RESET_SLOTS;
        NRF_PWM0->TASKS_SEQSTART[0] = 1;
        return;
    }

    if (m_led_display_mode == LED_MODE_TEST_RAINBOW)
    {
        for (int i = 0; i < NUM_LEDS; i++)
        {
            float base_deg = 0.0f;
            if (NUM_LEDS > 1)
            {
                base_deg = ((float)i * 360.0f) / (float)(NUM_LEDS - 1);
            }
            float hue_deg = fmodf(base_deg + offset_deg, 360.0f);
            Color_t color = hsv2rgb(hue_deg, 1.0f, clamp01f(LED_BRIGHTNESS_TEST_RAINBOW));
            ws2812_set_led(i, color.r, color.g, color.b);
        }
        offset_deg += 6.0f;
        if (offset_deg >= 360.0f) offset_deg -= 360.0f;
    }
    else
    {
        float dbm_min = 0.0f;
        float dbm_max = 0.0f;
        get_mode_dbm_range(m_led_display_mode, &dbm_min, &dbm_max);

        for (int i = 0; i < NUM_LEDS; i++)
        {
            float pos_f = 0.0f;
            if (NUM_LEDS > 1)
            {
                pos_f = ((float)i * (float)(BANDWIDTH - 1)) / (float)(NUM_LEDS - 1);
            }
            int left = (int)pos_f;
            int right = left + 1;
            if (right >= BANDWIDTH) right = BANDWIDTH - 1;
            float t = pos_f - (float)left;

            float rssi_interp = m_rssi_floating[left] * (1.0f - t) + m_rssi_floating[right] * t;
            if (m_led_display_mode == LED_MODE_FREQ_RAINBOW_PEAK ||
                m_led_display_mode == LED_MODE_INTENSITY_COLOR)
            {
                float rssi_peak_interp =
                    (-1.0f * (float)m_rssi_peak[left]) * (1.0f - t) +
                    (-1.0f * (float)m_rssi_peak[right]) * t;
                rssi_interp = rssi_peak_interp;
            }

            float intensity = (rssi_interp - dbm_min) / (dbm_max - dbm_min);
            if (intensity < 0) intensity = 0;
            if (intensity > 1.0f) intensity = 1.0f;

            uint8_t r, g, b;
            if (m_led_display_mode == LED_MODE_INTENSITY_COLOR)
            {
                float hue_deg = LED_INTENSITY_COLOR_HUE_LOW_DBM +
                                intensity * (LED_INTENSITY_COLOR_HUE_HIGH_DBM - LED_INTENSITY_COLOR_HUE_LOW_DBM);
                Color_t color = hsv2rgb(hue_deg, 1.0f, clamp01f(LED_BRIGHTNESS_INTENSITY_COLOR));
                r = color.r;
                g = color.g;
                b = color.b;
            }
            else
            {
                float hue_deg = 0.0f;
                if (NUM_LEDS > 1)
                {
                    hue_deg = ((float)i * 270.0f) / (float)(NUM_LEDS - 1);
                }
                float brightness;
                if (m_led_display_mode == LED_MODE_FREQ_RAINBOW_PEAK)
                {
                    brightness = map_brightness_range(
                        intensity,
                        LED_BRIGHTNESS_FREQ_RAINBOW_PEAK_MIN,
                        LED_BRIGHTNESS_FREQ_RAINBOW_PEAK_MAX);
                }
                else
                {
                    brightness = map_brightness_range(
                        intensity,
                        LED_BRIGHTNESS_FREQ_RAINBOW_MIN,
                        LED_BRIGHTNESS_FREQ_RAINBOW_MAX);
                }
                Color_t color = hsv2rgb(hue_deg, 1.0f, brightness);
                r = color.r;
                g = color.g;
                b = color.b;
            }

            ws2812_set_led(i, r, g, b);
        }
    }

    NRF_PWM0->EVENTS_SEQEND[0] = 0;
    NRF_PWM0->SEQ[0].PTR = (uint32_t)m_led_pwm_data;
    NRF_PWM0->SEQ[0].CNT = NUM_LEDS * 24 + WS2812_RESET_SLOTS;
    NRF_PWM0->TASKS_SEQSTART[0] = 1;
}
#pragma endregion ws2812

#pragma region Timer
void timer_init(void)
{
    NRF_TIMER0->TASKS_STOP = 1;
    NRF_TIMER0->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER0->PRESCALER = 4; // 1MHz resolution
    NRF_TIMER0->TASKS_CLEAR = 1;
    NRF_TIMER0->TASKS_START = 1;
}

uint32_t get_time_ms(void)
{
    NRF_TIMER0->TASKS_CAPTURE[0] = 1;
    return (NRF_TIMER0->CC[0]) / 1000;
}

#pragma endregion Timer

#pragma region Radio Scanner
// --------------------------------------------------------------------------
// Radio Scanner
// --------------------------------------------------------------------------


/**
 * @brief Initialize the radio for scanning
 **/
void radio_init_scanner(void)
{
    NRF_RADIO->TASKS_DISABLE = 1;
    while (NRF_RADIO->EVENTS_DISABLED == 0)
        ;
    NRF_RADIO->POWER = 1;
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Ble_1Mbit << RADIO_MODE_MODE_Pos);
}


/**
 * @brief scans the radio.
 * This function fills the `m_rssi_current[]` array with RSSI values for each frequency in the scan range.
 **/
void scan_band(void)
{
    if ((NRF_CLOCK->HFCLKSTAT & (CLOCK_HFCLKSTAT_SRC_Msk | CLOCK_HFCLKSTAT_STATE_Msk)) !=
        (CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos | CLOCK_HFCLKSTAT_STATE_Running << CLOCK_HFCLKSTAT_STATE_Pos))
    {
        NRF_CLOCK->TASKS_HFCLKSTART = 1;
        while ((NRF_CLOCK->HFCLKSTAT & CLOCK_HFCLKSTAT_STATE_Msk) == 0)
            ;
    }
    for (uint32_t freq = SCAN_START_FREQ; freq <= SCAN_END_FREQ; freq++)
    {
        NRF_RADIO->FREQUENCY = freq;
        NRF_RADIO->DATAWHITEIV = 0x40;
        NRF_RADIO->EVENTS_READY = 0;
        NRF_RADIO->TASKS_RXEN = 1;
        while (NRF_RADIO->EVENTS_READY == 0)
            ;
        NRF_RADIO->EVENTS_RSSIEND = 0;
        NRF_RADIO->TASKS_RSSISTART = 1;
        while (NRF_RADIO->EVENTS_RSSIEND == 0)
            ;
        uint8_t val = NRF_RADIO->RSSISAMPLE;
        if ((freq - SCAN_START_FREQ) < BANDWIDTH)
            m_rssi_current[freq - SCAN_START_FREQ] = val;
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->TASKS_DISABLE = 1;
        while (NRF_RADIO->EVENTS_DISABLED == 0)
            ;
    }
}

/**
 * @brief 处理扫描到的 RSSI 数据。
 * 将当前的 RSSI 值转换为浮点数并更新峰值数组。
 **/
void handle_band_scaner(void)
{
    led_peak_cfg_t peak_cfg = get_mode_peak_cfg(m_led_display_mode);

    for (int i = 0; i < BANDWIDTH; i++)
    {
        // 1. 转换为真实的 dBm 浮点数 (nRF52 RSSI 为绝对值，需取负)
        m_rssi_floating[i] = -1.0f * (float)m_rssi_current[i];

        // 2. 更新峰值记录 (数值越小代表信号越强)
        if (m_rssi_current[i] <= m_rssi_peak[i])
        {
            m_rssi_peak[i] = m_rssi_current[i];
            m_rssi_peak_hold_frames[i] = 0;
        }
        else
        {
            // 3. 峰值保持+衰减：
            // 先保持 peak_cfg.hold_frames 帧，再按间隔逐步衰减到 127
            if (m_rssi_peak_hold_frames[i] < 0xFFFFu)
            {
                m_rssi_peak_hold_frames[i]++;
            }

            if (m_rssi_peak_hold_frames[i] > peak_cfg.hold_frames)
            {
                uint16_t decay_phase = m_rssi_peak_hold_frames[i] - peak_cfg.hold_frames;
                if ((decay_phase % peak_cfg.decay_interval_frames) == 0 && m_rssi_peak[i] < 127)
                {
                    uint16_t peak_next = (uint16_t)m_rssi_peak[i] + (uint16_t)peak_cfg.decay_step;
                    if (peak_next > 127) peak_next = 127;
                    m_rssi_peak[i] = (uint8_t)peak_next;
                }
            }
        }
    }
}

/**
 * @brief 按帧发送 RSSI 数据。
 * 每轮发送 BANDWIDTH 帧，每帧包含：[频点下标(float), RSSI(float), 结束标志(4字节)]
 **/
void send_rssi_frames(void)
{
    const uint8_t delimiter[] = {0x00, 0x00, 0x80, 0x7f};
    for (int i = 0; i < BANDWIDTH; i++)
    {
        float freq_idx = (float)i;
        uart_write((const uint8_t *)&freq_idx, sizeof(float));
        uart_write((const uint8_t *)&m_rssi_floating[i], sizeof(float));
        uart_write(delimiter, 4);
    }
}

static void uart_report_led_status(void)
{
    char msg[96];
    snprintf(msg, sizeof(msg),
             "LED mode=%u(%s), output=%s\r\n",
             (unsigned)m_led_display_mode,
             m_led_mode_names[m_led_display_mode],
             m_led_output_enabled ? "ON" : "OFF");
    uart_print(msg);
}

static void handle_uart_command(uint8_t cmd)
{
    if (cmd == '\r' || cmd == '\n' || cmd == ' ')
    {
        return;
    }

    if (cmd == 'm' || cmd == 'M')
    {
        m_led_display_mode = (uint8_t)((m_led_display_mode + 1u) % LED_MODE_COUNT);
        uart_report_led_status();
        return;
    }

    if (cmd >= '0' && cmd <= '3')
    {
        m_led_display_mode = (uint8_t)(cmd - '0');
        uart_report_led_status();
        return;
    }

    if (cmd == 'l' || cmd == 'L')
    {
        m_led_output_enabled = !m_led_output_enabled;
        uart_report_led_status();
        return;
    }

    if (cmd == '?' || cmd == 'h' || cmd == 'H')
    {
        uart_print("CMD: m=next mode, 0/1/2/3=set mode, l=toggle led, ?=help\r\n");
        uart_report_led_status();
    }
}

#pragma endregion Radio Scanner

#pragma region DFU Bootloader Entry

/**
 * @brief Tell the user the device goes to DFU mode, and then reset to the DFU bootloader.
 * This function does not return.
 **/
void goto_dfu(void)
{
    // Signal to bootloader to enter DFU mode
    NRF_POWER->GPREGRET = BOOTLOADER_DFU_START;
    NVIC_SystemReset(); // This function does not return
}

#pragma endregion DFU Bootloader Entry

#pragma region Main Application
// --------------------------------------------------------------------------
// Main Application
// --------------------------------------------------------------------------
int main(void)
{
    // Init Hardware
    timer_init();
    uart_init();
    ws2812_init();
    uart_print("UART LED control ready. '?' for help.\r\n");

    // Init Radio
    radio_init_scanner();

    // Clear Data
    memset(m_rssi_peak, 127, sizeof(m_rssi_peak)); // 初始化为最弱信号
    memset(m_rssi_floating, 0, sizeof(m_rssi_floating));
    memset(m_rssi_peak_hold_frames, 0, sizeof(m_rssi_peak_hold_frames));

    uint32_t last_scan_time = 0;
    float scan_period_ms;
    while(1)
    {
        uint8_t cmd;
        while (uart_read_byte_nonblocking(&cmd))
        {
            handle_uart_command(cmd);
        }

        // 控制真实扫描间隔
        if(get_time_ms() - last_scan_time < 16) // 降低延时以提高帧率 (约 60FPS)
        {
            nrf_delay_us(200); // 避免过快循环
            continue;
        }

        // scan_period_ms = get_time_ms() - last_scan_time;
        // uart_write((const uint8_t *)&scan_period_ms, sizeof(float));

        scan_band();
        handle_band_scaner();

        update_led_display();
        // send_rssi_frames();

        last_scan_time = get_time_ms();
    }
}

#pragma endregion Main Application
