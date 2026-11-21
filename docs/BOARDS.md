# Pico 构建与 S31 历史归档 / Pico builds and S31 archive

自2026-11-21起，开发主线仅支持 `pico`。最后支持S31的源码保存在[归档标签 s31-final-2026-11-21](https://github.com/leephp/read-pico-reader/tree/s31-final-2026-11-21)，对应提交 `016f4f8808df3ba7f981986a673bcbadae12f313`。S31已发布的RC116及此前版本、下载链接、网页安装器和OTA清单保留；归档标签不代表发布新固件。
Since2026-11-21, main supports Pico only. The linked tag preserves the final S31 source. Published S31 RC116 and older images, downloads, installers and OTA feeds remain available. The archive tag does not publish new firmware.

## 源码与配置 / Source and configuration

- `main/`、`components/`：Pico实际源码，无板级源码覆盖。/ Canonical Pico sources, without overlays.
- `boards/pico/`：身份、硬件接口、分区、sdkconfig默认值和依赖锁。/ Identity, hardware interface, partitions, defaults and dependency lock.
- `tools/build_firmware.py`：隔离构建目录与包校验。/ Isolated build output and package validation.
- `tools/board_source_view.py`：为构建与宿主测试复制源码，清除旧生成文件。/ Copy source for builds/tests and prune stale generated files.

| 项目 / Item | Pico |
| --- | --- |
| 芯片 / Chip | ESP32-S3 / Xtensa |
| 项目名 / Project | `Read_Pico` |
| 本地版本 / Local version | `0.3.3-rc109` |
| 应用槽 / App slots | 2 × 4 MiB |
| 引导程序地址 / Bootloader offset | `0x0` |
| OTA数据地址 / OTA data offset | `0xd10000` |
| 设置命名空间 / Settings namespace | `read_pico` |

使用支持ESP32-S3的ESP-IDF v6.1；未知板型与冲突芯片目标会直接失败。
Use ESP-IDF v6.1 with ESP32-S3 support. Unknown boards and conflicting chip targets are rejected.

```sh
python3 tools/build_firmware.py --board pico --idf-pico /path/to/esp-idf --python /path/to/idf-python/bin/python
# Windows（先载入ESP-IDF环境 / load ESP-IDF first）
powershell -File tools/build_board.ps1 -Board pico
```

本地包位于 `build/boards/packages/pico/`。完整烧写命令见包内 `flash-command.txt`，工具不会连接或刷写设备。不能将应用Bin写到地址0。源码清理不更改分区、NVS、校准、原波形或已发布RC109固件。
Local packages are under `build/boards/packages/pico/`. Use their `flash-command.txt`; building never connects to or flashes hardware. Never flash an application-only image at address0. This cleanup preserves partitions, NVS, calibration, waveforms and published RC109 bytes.

## 验证 / Validation

```sh
python3 tools/board_selection_host_test.py --build-root build/boards
python3 tools/board_layer_host_test.py
python3 tools/pr59_driver_baseline_host_test.py
```

CI只构建和测试Pico。历史S31安装器仍做独立校验，防止保留入口失效。历史固件附件按原SHA-256校验；不能用当前同号构建替换正式字节。
CI builds/tests Pico only. Historical S31 installers retain separate checks to protect existing entries. Published attachments retain their original SHA-256; never replace them with current same-version builds.

### Pico 原生 USB 重连 / Native USB reconnection

本地包使用 `usb-reset` 进入下载模式。若复位时端口短暂消失或编号改变，使用 ESP-IDF Python 环境运行 `tools/pico_esptool.py --serial <USB序列号> -- <esptool命令及参数>`。序列号可由 `python -m serial.tools.list_ports -v` 查看。工具只重试连接，并按精确序列号重找同一设备；开始写入后不自动重试。写入地址与文件必须来自本次构建的 `flash_args`，日常本地应用测试只写其中的应用项，保留其他分区。

Local bundles enter download mode with `usb-reset`. For disappearing or renumbered ports, run `tools/pico_esptool.py --serial <USB-serial-number> -- <esptool-command-and-arguments>` in the ESP-IDF Python environment. Find the serial number with `python -m serial.tools.list_ports -v`. Only connection is retried, matching the exact device identity; writes are never automatically retried. Use addresses and files from the current build's `flash_args`; local application testing writes only its application entry, preserving other partitions.
