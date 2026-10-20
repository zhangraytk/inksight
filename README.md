# InkSight for XTEINK X4

面向 **XTEINK X4** 的实验性 InkSight 移植：把 800×480 墨水屏用作天气、便签、倒计时及其他信息模式的显示终端。

本仓库是 [datascale-ai/inksight](https://github.com/datascale-ai/inksight) 的独立 Fork，重点是 X4 的显示、实体按键与电源管理适配。感谢上游提供固件、后端、Web 配置和模式系统。本项目与 XTEINK 官方没有隶属关系，也不承诺兼容上游托管服务。

> **实验状态：** 源码可构建不等于实机验收。X4 与上游默认板型已有本地编译验证；休眠电流、电源保持、定时/按键唤醒及 OTA 恢复仍需完整实机验证。刷写前请先保存自己的原厂完整 Flash 备份。

## 这个 Fork 改了什么

- 新增 `xteink_x4` 固件环境：ESP32-C3、16MB Flash、GDEQ0426T82 / SSD1677、800×480 黑白屏。
- 接入 X4 电阻梯导航按键、电源按键、电池采样与供电保持控制。
- 支持上一/下一显示模式，后端增加 `prev=1` 切换请求，并调整部分布局以适配 800×480。
- 保留 InkSight 的自托管后端、Web 配置与模式体系；原有板型环境仍保留在 [platformio.ini](firmware/platformio.ini) 中。

这是信息屏固件移植，当前不提供 X4 原厂阅读器的电子书阅读功能。建议配合本 Fork 的后端自托管使用，尤其是上一模式切换等新增功能。

## 获取和编译固件

### 下载 GitHub Actions 构建产物

在本仓库的 [Actions](https://github.com/zhangraytk/inksight/actions/workflows/firmware.yml) 中选择一次成功的固件构建，在 **Artifacts** 下载对应板型：X4 使用 `firmware-xteink_x4`。其他构建产物以 `firmware-<环境名>` 命名，不能混刷到 X4。

下载包包含固件 `.bin`、校验值与构建清单。核对构建对应的提交和板型，并按包内文件名选择用途：`firmware_merged.bin` 是首次串口刷写用的合并镜像，`firmware.bin` 是应用镜像。Actions 构建成功仅表示编译通过，产物仍为实验版本；下载 Artifacts 通常需要登录 GitHub。

### 本地构建

安装 [PlatformIO](https://platformio.org/)，然后：

```bash
git clone https://github.com/zhangraytk/inksight.git
cd inksight/firmware
pio run -e xteink_x4
```

产物在 `firmware/.pio/build/xteink_x4/`，包括 `firmware.bin`、`bootloader.bin`、`partitions.bin` 与 `firmware_merged.bin`。

刷写方式、16MB 分区、引脚和恢复步骤见 [X4 移植与使用指南](docs/xteink-x4.md#7-固件烧录指南-esptool)。该文档也包含开发期设计说明，其中功耗数字尚未经过本 Fork 的完整实机测量，请勿作为续航保证。

## 先备份，再刷写

在写入新固件前，用 esptool 读取并保存设备自身的完整 16MB Flash 镜像，校验文件大小为 **16,777,216 字节**，另存校验值并保留离线副本。备份可能包含设备凭据及个人配置，请勿提交到公开仓库。

恢复必须使用与你设备匹配的备份；具体命令见 [原厂固件备份校验与恢复流程](docs/xteink-x4.md#8-原厂固件备份校验与恢复流程)。当前指南列出了备份校验与回刷步骤，首次读取备份时请根据所用 esptool 版本确认 `read_flash` / `read-flash` 命令。不要在备份前擦除 Flash，也不要把编译出的应用镜像当作原厂备份。

## 配置与使用

1. 按 [部署文档](docs/deploy.md) 启动本 Fork 的后端与 WebApp。
2. 刷入 X4 固件；长按返回键进入配网，连接 `InkSight-XXXX` 热点（后缀为设备 MAC 的末四位），访问 `192.168.4.1`。
3. 保存 Wi-Fi 与设备可访问的后端地址，然后在 Web 配置页选择内容模式。局域网部署请填服务器的局域网地址，设备上的 `localhost` 不指向你的电脑。

| 按键 | 当前行为 |
|---|---|
| 右 / 上，短按 | 下一显示模式 |
| 左 / 下，短按 | 上一显示模式 |
| 确认 / 返回，短按 | 刷新当前模式 |
| 返回，长按 | 进入配网 Portal |
| 电源，短按 | 进入定时休眠 |
| 电源，长按 | 请求关机 |

按键在设备运行时处理；深度休眠时通过电源键或定时器唤醒。配网 Portal 中，返回键用于重启退出。具体 ADC 阈值与电源电路约束见 [X4 指南](docs/xteink-x4.md)。

### 网络默认值

- HTTPS 默认验证服务器证书；正常连接不会因失败而自动跳过验证或降级 HTTP。
- DNS 保留 DHCP 下发的配置，支持依赖内网 DNS 的自托管域名。
- 配网页面保留“跳过 TLS 验证”的显式调试选项，默认关闭。已有设备保存过的配置仍可能启用该选项，请升级后检查。跳过验证或手动使用 `http://` 都会降低传输保护，仅在明确了解风险的调试环境使用。

## 已知限制与验收边界

- X4 启动后进入约 **10 分钟临时在线窗口**；代码中标注的“3 分钟闲置休眠”目前没有在该窗口生效，按键也不会延长这个窗口。请勿依据 3 分钟描述估算功耗。
- 休眠电流、GPIO13 电源保持、USB/电池切换、定时与电源键唤醒、长按关机和 OTA 恢复需要实机验收。
- 原厂阅读器功能与 microSD 电子书读取不在当前移植范围。
- 上游托管网站、在线刷机和社区模式服务可能与本 Fork 版本不同步；本仓库的构建产物和自托管服务是调试基准。
- 其他保留板型未因此获得完整硬件回归验证。问题反馈请附板型、提交号、供电方式、操作步骤及脱敏串口日志。

## 文档与贡献

- [X4 移植、引脚、刷写与恢复](docs/xteink-x4.md)
- [自托管部署](docs/deploy.md) / [English deployment](docs/en/deploy.md)
- [硬件](docs/hardware.md) / [组装](docs/assembly.md) / [通用刷机](docs/flash.md) / [配置](docs/config.md)
- [架构](docs/architecture.md) / [API](docs/api.md) / [自定义模式开发](docs/custom-mode-dev.md)
- [贡献指南](CONTRIBUTING.md)

通用文档继承自上游，硬件选型与刷机示例可能针对原有 DIY 板型，X4 请以专用指南和本 README 的限制说明为准。欢迎提交可复现问题与小范围改进；向上游贡献时建议把板型适配、通用 API 和渲染改进分别讨论、分别提交。

## 许可与归属

本项目沿用上游 [MIT License](LICENSE)，保留 `Copyright (c) 2026 datascale-ai`。第三方库遵循各自许可；移植中引用的驱动来源与说明见源码。InkSight、XTEINK 等名称仅用于说明项目来源和目标硬件。
