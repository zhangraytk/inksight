# XTEINK X4 硬件移植开发与使用指南

本文档详细说明 InkSight 固件在 **XTEINK X4** 电子书硬件设备上的移植架构、引脚定义、电源管理、按键交互、编译构建、固件烧录与原厂恢复指南。

> 实验性移植：以下电源与休眠流程描述的是实现设计，尚未完成本 Fork 的整机功耗、定时唤醒和 OTA 恢复实机验收。已知启动后的临时在线窗口为 10 分钟，代码中的 3 分钟闲置判断尚未在此窗口生效；按键不会延长该窗口。

---

## 1. 硬件规格概述

| 项目 | 参数 | 说明 |
|---|---|---|
| **MCU** | ESP32-C3FH4 / ESP32-C3 | 单核 RISC-V 32位，主频 160MHz |
| **SRAM** | 400KB | 片上集成，无片外 PSRAM |
| **Flash** | 16MB (128Mbit) | 运行于 DIO 模式，80MHz |
| **显示屏** | 4.26 英寸 E-Paper (GDEQ0426T82) | 佳显高分黑白电子墨水屏 |
| **分辨率** | 800 × 480 像素 | 原生横屏 (219 PPI) |
| **控制器** | SSD1677 | 支持全刷 (1.6s)、局刷 / 快刷 (0.42s) 与深休眠 |
| **接口** | 硬件 SPI (当前配置 20MHz) | 与 microSD 接口复用 SCK/MOSI |
| **电池** | 650mAh 锂电池 | 软包锂电池，GPIO0 ADC 采集 |
| **电源保持** | GPIO13 | 电池供电保持开关 (P-MOSFET Latch) |
| **按键** | 1个数字电源键 + 6个电阻梯按键 | 电源键 (GPIO3), ADC1 (GPIO1), ADC2 (GPIO2) |
| **USB 检测** | GPIO20 | 高电平代表外部 USB 接入供电 |
| **扩展存储** | microSD 卡槽 | CS=12, MISO=7, SCK=8, MOSI=10 |

---

## 2. 引脚与电气接口映射表

| 硬件功能 | ESP32-C3 GPIO | 属性 / 电气说明 | 代码宏定义 (`config.h`) |
|---|---|---|---|
| **EPD SCK** | `GPIO8` | 硬件 SPI 时钟，输出 | `PIN_EPD_SCK` |
| **EPD MOSI** | `GPIO10` | 硬件 SPI 数据，输出 | `PIN_EPD_MOSI` |
| **EPD CS** | `GPIO21` | 片选，低电平有效，输出 | `PIN_EPD_CS` |
| **EPD DC** | `GPIO4` | 数据/命令选择，输出 | `PIN_EPD_DC` |
| **EPD RST** | `GPIO5` | 屏复位引脚，低电平脉冲复位 | `PIN_EPD_RST` |
| **EPD BUSY** | `GPIO6` | 忙状态指示，高电平表示忙 | `PIN_EPD_BUSY` |
| **电源保持 (PWR_LATCH)** | `GPIO13` | 电池供电保持控制，高电平导通 | `PIN_PWR_LATCH` |
| **电源按键 (PWR_BTN)** | `GPIO3` | 数字输入，内部上拉，低电平触发 | `PIN_CFG_BTN` |
| **ADC 按键输入 1** | `GPIO1` | ADC1 通道1，电阻分压网络 | `PIN_BTN_ADC1` |
| **ADC 按键输入 2** | `GPIO2` | ADC1 通道2，电阻分压网络 | `PIN_BTN_ADC2` |
| **电池电压 ADC** | `GPIO0` | ADC1 通道0，10k/10k 电阻分压 | `PIN_BAT_ADC` |
| **USB 接入检测** | `GPIO20` | 数字输入，高电平表示 USB 接入 | `PIN_USB_DETECT` |
| **microSD CS** | `GPIO12` | SPI 片选，输出 | `PIN_SD_CS` |
| **microSD MISO** | `GPIO7` | SPI 主机输入，从机输出 | `PIN_SD_MISO` |

---

## 3. 电源管理与休眠架构

XTEINK X4 使用 GPIO13 驱动电池供电电路中的 MOSFET 开关。固件通过两套互不干扰的电源模式来兼顾低功耗待机与定时刷新：

### 模式 1：定时休眠模式 (Timed Deep Sleep)
- **应用场景**：常规桌面便携时钟/日历/资讯刷新周期。
- **控制逻辑**：
  1. 断开 Wi-Fi 射频，驱动墨水屏进入深度休眠 (`epdSleep`)。
  2. 将 GPIO13 置为 HIGH，并调用 `gpio_hold_en(GPIO_NUM_13)` 和 `gpio_deep_sleep_hold_en()` 锁定引脚状态。
  3. 配置 `esp_deep_sleep_enable_gpio_wakeup(1ULL << 3, ESP_GPIO_WAKEUP_GPIO_LOW)`，允许用户按电源键唤醒。
  4. 配置 `esp_sleep_enable_timer_wakeup` 定时唤醒。
  5. 设计目标是保持电池供电回路与 RTC 定时器工作；整机休眠电流和定时唤醒需实测确认。

### 模式 2：整机关电模式 (Full Power Off)
- **应用场景**：电池电量耗尽保护、长按电源键 (>3s) 用户主动关机。
- **控制逻辑**：
  1. 等待电源按键释放，避免误触发。
  2. 释放 GPIO13 保持并拉低为 LOW (`digitalWrite(13, LOW)` 并 hold)。
  3. 电池供电回路断开，整机彻底断电。此时不会自动定时开机，只能通过用户物理按压电源键接通初级回路开机。

---

## 4. 按键交互与 ADC 阈值规范

X4 的物理按键分为电源键（GPIO3 数字中断）与导航按键组（GPIO1 与 GPIO2 模拟分压电阻梯）。

### 物理按键功能映射

| 物理按键 | 信号来源 | 触发条件 | InkSight 对应功能 |
|---|---|---|---|
| **右键 (Right)** | GPIO1 ADC | 短按 (50~1500ms) | **切换下一显示模式 (Next Mode)** |
| **左键 (Left)** | GPIO1 ADC | 短按 (50~1500ms) | **切换上一显示模式 (Prev Mode)** |
| **确认键 (Confirm)** | GPIO1 ADC | 短按 (50~1500ms) | **手动触发内容刷新 (Manual Refresh)** |
| **返回键 (Back)** | GPIO1 ADC | **长按 (>2000ms)** | **强制进入 Wi-Fi Captive Portal 配网页面** |
| **电源键 (Power)** | GPIO3 | 短按 (50~1500ms) | **进入定时休眠 (Deep Sleep)** |
| **电源键 (Power)** | GPIO3 | **长按 (>3000ms)** | **整机关机断电 (Power Off)** |
| **上键 (Up)** | GPIO2 ADC | 短按 | 切换下一显示模式 |
| **下键 (Down)** | GPIO2 ADC | 短按 | 切换上一显示模式 |

### ADC 采样分档标定值（12-bit ADC，11dB 衰减）

* **GPIO1 (ADC1)**：
  - 未按下：`ADC > 3800`
  - **Back**：`3100 < ADC <= 3800` (参考均值 ~3512)
  - **Confirm**：`2090 < ADC <= 3100` (典型均值 ~2694)
  - **Left**：`750 < ADC <= 2090` (典型均值 ~1493)
  - **Right**：`ADC <= 750` (典型均值 ~5)

* **GPIO2 (ADC2)**：
  - 未按下：`ADC > 3200`
  - **Up**：`1120 < ADC <= 3200` (参考均值 ~2242)
  - **Down**：`ADC <= 1120` (典型均值 ~5)

---

## 5. 电池与供电检测

- **采集引脚**：GPIO0 (ADC1_CHANNEL_0)。
- **分压电阻**：$R_1 = 10\text{k}\Omega, R_2 = 10\text{k}\Omega$，实际分压系数为 2.0。
- **校准算法**：调用 `analogReadMilliVolts(PIN_BAT_ADC)` 自动应用 ESP32-C3 芯片内部 eFuse 标定参数，再计算：
  $$\text{V}_{\text{bat}} = \frac{\text{mV}_{\text{pin}}}{1000} \times 2.0$$
- **USB 状态**：通过读取 GPIO20 电平识别外部供电，用于优化休眠与状态上报。

---

## 6. 编译与构建

在固件工程根目录下，使用 PlatformIO 构建 `xteink_x4` 环境：

```bash
cd firmware

# 仅构建 xteink_x4 固件
pio run -e xteink_x4
```

### 构建产物路径
- **应用固件**：`firmware/.pio/build/xteink_x4/firmware.bin`
- **分区表**：`firmware/.pio/build/xteink_x4/partitions.bin`
- **引导程序**：`firmware/.pio/build/xteink_x4/bootloader.bin`
- **一键合并刷机包**：`firmware/.pio/build/xteink_x4/firmware_merged.bin`（由 `merge_firmware.py` 自动生成，包含 0x0 引导、0x8000 分区表、0x10000 应用镜像）。

---

## 7. 固件烧录指南 (esptool)

> [!WARNING]
> 刷机操作必须在实体设备已断电或处于 Bootloader 模式（按住 Boot/下载引脚连接 USB）下进行。切勿盲目擦除整个 Flash！

### 方式一：整包合并烧录（推荐）
直接写入合成镜像到 Flash 首地址 `0x0`：

```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem* --baud 921600 \
    --before default_reset --after hard_reset write_flash \
    -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 firmware/.pio/build/xteink_x4/firmware_merged.bin
```

### 方式二：分段烧录（保留已有引导/NVS配置）
```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem* --baud 921600 \
    --before default_reset --after hard_reset write_flash \
    -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0000 firmware/.pio/build/xteink_x4/bootloader.bin \
    0x8000 firmware/.pio/build/xteink_x4/partitions.bin \
    0x10000 firmware/.pio/build/xteink_x4/firmware.bin
```

---

## 8. 原厂固件备份校验与恢复流程

若用户已备份原厂 16MB 镜像（例如 `x4_factory_backup_16mb.bin`）：

### 1. 核验备份文件大小
```bash
# 必须严格等于 16,777,216 字节 (16MB)
ls -l x4_factory_backup_16mb.bin
```

### 2. 原厂固件完整回刷
```bash
esptool.py --chip esp32c3 --port /dev/cu.usbmodem* --baud 921600 \
    --before default_reset --after hard_reset write_flash \
    -z --flash_mode dio --flash_freq 80m --flash_size 16MB \
    0x0 x4_factory_backup_16mb.bin
```

---

## 9. 首次上电实机验收 Checklist

1. **上电开机**：
   - 插入 USB 或按压电源键，串口以 115200 波特率打印 `[X4-PWR]` 及 `=== InkSight ===`。
   - 屏幕清屏并刷新出初始配网提示界面或上次缓存。
2. **Wi-Fi 配网 Portal**：
   - 长按返回键（Back 键）2 秒以上，设备进入热点模式，广播 `InkSight-XXXX`（MAC 末四位）。
   - 连接后访问 `192.168.4.1`，输入 Wi-Fi 账号密码及后端服务地址保存。
3. **分辨率与内容渲染**：
   - 观察墨水屏内容应以原生 800×480 显示，文字清晰边缘整齐，无位移、拉伸、反色或黑白颠倒。
4. **按键切换**：
   - 短按右键，切换下一模式；短按左键，切换上一模式；短按确认键，触发当前模式立即刷新。
5. **休眠与唤醒**：
   - 屏幕刷新后，系统打印 `[X4-PWR] Preparing timed deep sleep`。
   - 拔出 USB 仅靠电池供电，设备进入低功耗休眠，到达指定分钟后自动唤醒刷新并继续休眠。
   - 在休眠状态下短按电源键，能够立即被外部 GPIO3 中断唤醒。
