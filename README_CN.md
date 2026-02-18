# RGB 频谱分析仪

一个用 nRF52832 做的简易 2.4GHz 频谱分析 RGB 灯带（用作装饰或玩具）
项目很大部份基于：https://github.com/hb020/pixlAnalyzer （感谢[hb020](https://github.com/hb020)和[atc1441](https://github.com/atc1441)）

灵感来源于：https://youtu.be/moBCOEiqiPs?si=ToIqUjC3-g4FUg77

## 版本历史

2026-02-19: 首次发布

## 开发

可以使用vscode配合openocd进行调试，".vscode/"已经配置好,但是需要自行根据平台和系统进行调整。

## 烧录

使用SWD接口

## 编译

它现在是给 MacOS 用的，要想在 Linux和 Windows上跑可能需要做些改动。
你需要先装好：

* `make`
* `gcc-arm-none-eabi`
  * 根据你的操作系统，在 `/RGBSA/firmware/sdk/components/toolchain/gcc/Makefile.posix` 或 `.../Makefile.windows` 文件中设置 gcc 的 bin 目录路径。
  * 比如 MacOS: `GNU_INSTALL_ROOT ?= /Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin/`

## 频率范围和信道

2.4GHz 的 ISM 频段被分成好几条信道，不同协议会用到不同的信道。下面是一些常见的信道分配：

| Center Frequency<br>(MHz) | IEEE 802.15.4<br>channel, 5MHz wide | 802.11b/g/n WiFi<br>channel, 22/20/40MHz wide | BLE<br>channel, 2MHz wide |
|:----:|:--:|:--:|:--:|
| 2400 |    |    |    |
| 2401 |    |    |    |
| 2402 |    |    | 37 |
| 2403 |    |    |    |
| 2404 |    |    | 0  |
| 2405 | 11 |    |    |
| 2406 |    |    | 1  |
| 2407 |    |    |    |
| 2408 |    |    | 2  |
| 2409 |    |    |    |
| 2410 | 12 |    | 3  |
| 2411 |    |    |    |
| 2412 |    |  1 | 4  |
| 2413 |    |    |    |
| 2414 |    |    | 5  |
| 2415 | 13 |    |    |
| 2416 |    |    | 6  |
| 2417 |    |  2 |    |
| 2418 |    |    | 7  |
| 2419 |    |    |    |
| 2420 | 14 |    | 8  |
| 2421 |    |    |    |
| 2422 |    |  3 | 9  |
| 2423 |    |    |    |
| 2424 |    |    | 10 |
| 2425 | 15 |    |    |
| 2426 |    |    | 38 |
| 2427 |    | (4)|    |
| 2428 |    |    | 11 |
| 2429 |    |    |    |
| 2430 | 16 |    | 12 |
| 2431 |    |    |    |
| 2432 |    |  5 | 13 |
| 2433 |    |    |    |
| 2434 |    |    | 14 |
| 2435 | 17 |    |    |
| 2436 |    |    | 15 |
| 2437 |    |  6 |    |
| 2438 |    |    | 16 |
| 2439 |    |    |    |
| 2440 | 18 |    | 17 |
| 2441 |    |    |    |
| 2442 |    |  7 | 18 |
| 2443 |    |    |    |
| 2444 |    |    | 19 |
| 2445 | 19 |    |    |
| 2446 |    |    | 20 |
| 2447 |    |  8 |    |
| 2448 |    |    | 21 |
| 2449 |    |    |    |
| 2450 | 20 |    | 22 |
| 2451 |    |    |    |
| 2452 |    |  9 | 23 |
| 2453 |    |    |    |
| 2454 |    |    | 24 |
| 2455 | 21 |    |    |
| 2456 |    |    | 25 |
| 2457 |    |(10)|    |
| 2458 |    |    | 26 |
| 2459 |    |    |    |
| 2460 | 22 |    | 27 |
| 2461 |    |    |    |
| 2462 |    | 11 | 28 |
| 2463 |    |    |    |
| 2464 |    |    | 29 |
| 2465 | 23 |    |    |
| 2466 |    |    | 30 |
| 2467 |    | 12 |    |
| 2468 |    |    | 31 |
| 2469 |    |    |    |
| 2470 | 24 |    | 32 |
| 2471 |    |    |    |
| 2472 |    | 13 | 33 |
| 2473 |    |    |    |
| 2474 |    |    | 34 |
| 2475 | 25 |    |    |
| 2476 |    |    | 35 |
| 2477 |    |    |    |
| 2478 |    |    | 36 |
| 2479 |    |    |    |
| 2480 | 26 |    | 39 |
| 2481 |    |    |    |
| 2482 |    |    |    |
| 2483 |    |    |    |
| 2484 |    | 14 |    |
| 2485 |    |    |    |
| 2486 |    |    |    |
| 2487 |    |    |    |

![ISM band channels](ism_band_channels.png)

## nRF52832的SDK链接

* [nRF52832 Product Specification v1.9](https://docs.nordicsemi.com/bundle/nRF52832_PS_v1.9/resource/nRF52832_PS_v1.9.pdf)
* [RADIO — 2.4 GHz Radio](https://docs.nordicsemi.com/bundle/ps_nrf52832/page/radio.html)
* [SAADC — Successive approximation analog-to-digital converter](https://docs.nordicsemi.com/bundle/ps_nrf52832/page/saadc.html)
