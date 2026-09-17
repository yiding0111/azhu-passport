# 阿猪 · AI Passport 桌宠固件

把[阿猪桌宠](https://github.com/yiding0111/azhu-desktop-pet)（原 Electron 桌面版）搬到 FoloToy **AI Passport**（ESP32-C3）上：屏上循环播放阿猪的 16 个动作，三枚实体键交互。原 Electron 那套（悬浮窗、行情联动、自动更新）不搬——那是桌面 OS 的东西；搬的是阿猪这个角色和它全部动作。

## 硬件

FoloToy AI Passport ｜ ESP32-C3（160MHz 单核，8MB Flash，无 PSRAM）｜ ST7789P3 240×320 ｜ 三键分压 ｜ ES8311 音频 ｜ CW2017 电量计 + 520mAh。
全部引脚见 [`main/board_config.h`](main/board_config.h)，取自 FoloToy 官方 `ai-passport-c3` 板级定义（不要凭猜改）。

## 它怎么工作

- 16 动作 × 4 帧，渲染成**全屏 240×320 的 8bpp 索引图 + 全局 256 色 RGB565 调色板**，打包成 `main/azhu_assets.bin`（约 4.7MB），用 `EMBED_FILES` 编进 app。
- C3 可用堆装不下整屏 RGB565 缓冲（150KB），所以走**行块 DMA 推屏**（每次 40 行，19KB 缓冲）。
- ST7789P3 有厂商专属初始化序列 + 必须颜色反相，见 `main/display.c`。
- 上/下键切动作，确认键随机来一个，约 9 秒无操作自动切「睡觉」。

## 一、云端编译（推荐，本机无需装 ESP-IDF）

推到 GitHub，Actions 自动用官方 ESP-IDF 镜像编译，产物可下载：

```bash
git init
git branch -M main
git add .
git commit -m "阿猪桌宠固件"
git remote add origin <你的-GitHub-仓库地址>
git push -u origin main
```

Actions 跑完，在 workflow 的 **azhu-passport-c3** 产物里拿到三个文件：`bootloader.bin`、`partition-table.bin`、`azhu_passport.bin`。

## 二、烧录前：先备份现在的 PokeWalk（必做）

板子现在跑的是宝可梦掌机 **PokeWalk**，烧阿猪会把它覆盖掉。先整片 dump 备份，否则找不回：

```bash
esptool --chip esp32c3 -p COM3 -b 460800 read_flash 0x0 0x800000 pokewalk_backup_8MB.bin
```

想刷回宝可梦：`esptool --chip esp32c3 -p COM3 write_flash 0x0 pokewalk_backup_8MB.bin`

## 三、烧录阿猪

```bash
esptool --chip esp32c3 -p COM3 -b 460800 write_flash \
  0x0     bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 azhu_passport.bin
```

（ESP32-C3 的 bootloader 在 `0x0`，不是 `0x1000`。）烧完自动重启，屏上就是阿猪。

## 改素材 / 加动作

换掉 `assets/actions/*.png` 或增删 `tools/build_assets.py` 里的 `ACTIONS`，重跑：

```bash
python tools/build_assets.py
```

生成新的 `main/azhu_assets.bin`，提交后 CI 自动出新固件。

## 按键

| 键 | 作用 |
|---|---|
| 上 | 上一个动作 |
| 下 | 下一个动作 |
| 确认 | 随机来一个动作 |
| （无操作 ~9 秒） | 自动睡觉 |

## esptool 怎么装

优先 `pip install esptool`。若被公司代理挡（装不上），用 Espressif 预编译独立版：[github.com/espressif/esptool/releases](https://github.com/espressif/esptool/releases) 里的 Windows 独立包，解压即用，无需 Python。

## 目录

```
CMakeLists.txt          顶层
partitions.csv          单 app 分区（app 6MB，放代码+素材）
sdkconfig.defaults      8MB flash / USB-Serial-JTAG console / esp32c3
main/
  board_config.h        板级引脚（官方定义）
  display.c/.h          ST7789P3 驱动 + 行块推屏 + 背光
  buttons.c/.h          ADC 三键分压
  azhu.c/.h             素材包解析
  main.c                主循环
  azhu_assets.bin       打包好的素材（EMBED 进 app）
assets/actions/         16 张动作源图（改素材用）
tools/build_assets.py   源图 → azhu_assets.bin
.github/workflows/      GitHub Actions 云编译
```

献给所有喜欢阿猪的人，非商业用途，如有侵权请联系。
