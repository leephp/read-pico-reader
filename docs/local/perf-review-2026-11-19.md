# 性能与内存优化审查 — 2026-11-19

只读审查，未改任何源码。范围：`main/`、`boards/pico|metalio47/`、`components/` 主要路径。
证据基线：`flash/manifest.json`（pico rc109）、`flash/metalio/manifest.json`（S31 rc116）与当前源码行号。
`docs/local/` 已由 `.gitignore:44` 忽略，本文件不进库。

板型：**pico** = ESP32-S3，4MiB 应用槽；**metalio47** = S31，5MiB 应用槽。

---

## 进展 / Progress

| 项 | 状态 | 提交 |
| --- | --- | --- |
| §1.1 字形落屏逐像素 | 已完成：`main/font/fb_text_blit.h` 整行直写半字节 + 覆盖率查表；reader/设置/统计三帧与旧引擎逐字节一致 | `bdf04cc` |
| §1.2 冷开书全本扫描 | 未开始 | — |
| §2.1 书架每本 `nvs_open` | 未开始 | — |
| §2.2 832KB `continuous_du` | 未开始 | — |

---

## 0. 结论 / 先后顺序

| 顺序 | 项目 | 收益 | 风险 | 板型 |
| --- | --- | --- | --- | --- |
| 1 | 字形落屏逐像素 `epd_draw_pixel` (§1.1) | 20–40ms/页 CPU | 中 | 两板，pico 更重 |
| 2 | 书架每本书一次 `nvs_open` (§2.1) | 首屏与扫描 IO | 低 | 两板 |
| 3 | 832KB `continuous_du` 常驻 + 阅读路径 memset (§2.2) | 内存 + 每次 812KB 写入 | 低 | 两板 |
| 4 | 160KB/槽 死 IO 缓冲 (§2.4) | 最多 640KB PSRAM | 低 | 两板 |
| 5 | 冷开书全本 EPUB 扫描 (§1.2) | 秒级首屏 | 中 | 两板 |
| 6 | UI 字形缓存线性扫描 + 测量即栅格化 (§2.3) | UI 交互延迟 | 低 | 两板 |
| 7 | UI 图标 72×72 / 搜索表打包（§4） | 约 200KB+ 闪存 | 低 | pico 最紧 |

闪存余量是硬约束：pico 只剩 **512 B**，S31 剩 **52,640 B**。任何"用闪存换 CPU"的改动都要先腾空间。

---

## 1. 阅读/绘制热路径

### 1.1 每个墨点一次 `epd_draw_pixel`（最确定的 CPU 浪费）

- `main/font/ttf_font.c:3138-3151` `blit_upright` 内层：`epd_draw_pixel(pen_x + glyph->left + gx, yy, mix_ink_aa(...) << 4, framebuffer)`
- pico 实现 `boards/pico/src/components/epdiy/src/epdiy.c:76-94`：普通函数，`_rotate()` + 两次越界判断 + 两次 `epd_width()/epd_height()`。
- 已有的快路径 `blit_reader_portrait`（`ttf_font.c:3074-3093`）只对 `KIIKO_BOARD_METALIO47` 且 `EPD_ROT_INVERTED_PORTRAIT` + reader AA 生效：每字形只解析一次旋转，按逻辑列写连续物理行。
- 量级：一页约 480 个 CJK 字形 × ~250 墨点 ≈ 12 万次调用；@240MHz 约 20–40ms/页，纯地址换算与边界开销。
- 修法：把"每字形预解析旋转/行基址 + 直接写半字节"推广到 pico 与 UI 绘制路径；保留一条慢路径兜底。
- 风险：中（旋转寻址重复实现，需覆盖各 rotation）。

### 1.2 冷开书先解压+解析整本 EPUB

- 门控 `main/book/book_epub.c:1472-1478`：`if (book->count > 256 && book->authored_count) return;`，随后要求所有章 entry ≤ `EPUB_PART_THRESHOLD`（128KiB）。
- `body_headings_scan`（`book_epub.c:1376-1383`）逐章 `load_entry`（整份 XHTML 进 PSRAM）+ `xml_next`。
- 触发：`main/apps/app_book.c:6028` 开书后立即 `save_progress` → `reader_current_chapter` → `navigation_prepare`（`book_epub.c:1563/1599`）。
- 成本：200 章 × 20KiB ≈ 4MiB 解压 + 词法扫描，全在 UI 任务，秒级；结果缓存（`body_scanned`），缓存失效后重现。
- 修法：只在打开目录时执行，或首屏后交给后台任务；加总字节预算（如 >8MiB 跳过）。目录/页脚暂用"第 N 节"回退。
- 风险：中（页脚章名与目录标签的行为变化，需验收）。

### 1.3 章节内回翻/锚点跳转从字节 0 重解压重解析

- `main/book/book_epub_parts.inc:206-213`：`offset < s->base || anchor || source_offset < s->source` 即 `parts_start`。
- `parts_start`（`:181-192`）`epub_parts_reset` 释放 ~61KiB `epub_parts_t` + ~36KiB `zip_stream`，重开流，并对之前每个 12KiB part 跑 `parse_chapter_html`；`:168-172` 还重跑 `chapter_css`（最多 256KiB）。
- 成本：1MiB 章节 ≈ 85 part → 1MiB 重解压 + 85 次文本/块/run 分配。
- 修法：保留上一 part 的 `html_text_t` + `source_begin/end`/`part_base`（12–48KiB PSRAM），回翻变成交接；章节切换时失效即可。
- 风险：低-中（缓存失效条件）。

### 1.4 位级 CRC32

- `main/book/zip_reader.c:109-116`：逐字节 8 次 `crc = (crc >> 1) ^ (0xedb88320 & (0 - (crc & 1)))`。
- 用于完整抽取（`:397`）与流式读取（`:470`）：章节、part、CSS、内嵌字体、图片、封面。1MiB ≈ 800 万次迭代，且同一 entry 重解压时重复付账。
- 修法：256 项查表（1KiB flash）或 `esp_rom_crc32_le`。
- 风险：无（保持字节序与 `~crc` 语义，跑 `tools/zip_reader_host_test.c`）。

### 1.5 图片尺寸在 draw 锁内反复解压 ZIP 头

- `main/apps/app_book.c:5773 lock_draw()` → `book_layout.c:533-540` `s_dims_fn` → `app_book.c:5528-5534` `reader_image_dims` → `book_epub.c:2505/2517` `zip_extract_prefix`（32B → 4KiB → 64KiB，各从 entry 头重新 inflate）。
- 排版锁横跨 SD 读 + miniz；字号/边距/行距/竖排每次重排都重来。
- `blk_t.image_width/image_height`（`html_text.h:53`）已存在，仅一条路径填过（`app_book.c:7425-7428`）。
- 修法：布局前预取缺失尺寸，`reader_image_dims` 读缓存；按 ZIP entry 缓存在 `book_epub_t`。
- 风险：低。

### 1.6 跨文件脚注整份 XHTML 每 part 重解压

- `main/book/book_epub.c:2141-2146`：每次 part 解析，对每个脚注 href `heap_caps_malloc(n)` + `zip_extract` + `html_extract_id_text`；`n` 上限 `EPUB_NOTE_FILE_MAX` 512KiB。
- 去重只在单次调用内；大章节多个 part 引用同一注释文件 = 反复整份解压。
- 修法：按 href 记忆化（≤12 × 2KiB ≈ 24KiB），章节内共享。
- 风险：低。

---

## 2. 内存

### 2.1 书架扫描：每本书一次 `nvs_open`（+ 回填再枚举一遍）

- `main/apps/app_book.c:6497` `book_progress_load(...)` → `main/book/book_progress.c:148` 每次 `nvs_open(NS, NVS_READONLY)`；`read_record`/`disk_read` 还会开 1–2 个进度文件。
- 紧接着 `shelf_backfill_read_books()` → `book_progress_list` → `disk_walk`，把每条记录再开一遍。
- 现成 API 未用：`book_progress_load_batch`（`book_progress.c:163`，单句柄、`BOOK_PROGRESS_BATCH_MAX` 64 本）。
- 100 本书 ≈ 100 次 NVS 会话 + ~500 次 `fopen`，全在主任务上。
- 修法：一次 `book_progress_list` 建 map，目录遍历时查表；回填跳过已有条目。
- 风险：低。

### 2.2 832KB `continuous_du` 为实验页常驻，阅读路径还 memset

- `components/continuous_du/continuous_du.c:84-85`：`heap_caps_calloc(epd_width()*epd_height())` = 1216×684 = **831,744 B** 永久 PSRAM。
- 开机即分配：`main/app/app_loop.c:1065` `.continuous_ready = continuous_du_init()`。
- 唯一使用方是触摸调试页；但阅读缩放/翻页路径会 `continuous_du_reset()`：`app_book.c:4594/8491/8525/8941/9263` → `continuous_du.c:100-104` 整块 memset（812KB 写入/次）。
- 修法：`continuous_du_init()` 改为首次 `mark_*`/`scan` 时惰性分配，离开该页 `deinit`；`reset()` 已有 `s_states==NULL` 早退。
- 风险：低（失败回退路径已存在）。

### 2.3 UI 字形缓存：256 槽线性扫描，测量即分配+栅格化

- `main/font/ui_font.c:139-180` `glyph()`：`for (i < UI_GLYPH_SLOTS)` 全表查；miss 时 `heap_caps_malloc` + `stbtt_MakeCodepointBitmap`（或补字逐点合成）。
- `ui_font_text_width_px`（`:192-198`）走同一条路径，所以"测量宽度"会分配并栅格化。
- 调用面很广：`main/ui/ui_kit.c:394-406`（固定宽度上下文测量每个标签/按键）、`:574-580`（标题逐字测量）、`ui_text_title_fit`/`ui_nav` 循环重测。
- 还有第二份预算 `UI_GLYPH_LIMIT`（`ui_font.c:23`，512KB）叠在 ttf 缓存的 320KB（pico）/1MB（S31）之上。
- 修法：`(cp,px)` 哈希桶（照 `ttf_font.c` 的 256 桶）；新增只取 advance 的测量路径，不分配。
- 风险：低。

### 2.4 每字形 2–3 次 PSRAM 分配/释放

- `main/font/ttf_font.c:2106-2143`：`glyph_entry` + `bitmap` + （描边时）`adjusted`，最后一个 free 前一个。
- pico 缓存预算 320KB（`boards/pico/kiiko_board_config.h:103`）只装约 400 个正文 CJK，跨页命中率近零 → 每次翻页 1000+ 次 malloc/free。文件注释自己把碎片列为 384KB 图片分配失败的成因。
- 修法：每槽 bitmap bump arena（随 `cache_reset` 归零）+ 固定 `glyph_entry_t` slab；描边按 padding 后边界一次分配。
- 风险：中（arena 与淘汰预算的边界）。

### 2.5 160KB/槽 块 IO 缓冲对内存字体是死重

- `ttf_font.c:2509` `load_opened_font` 无条件 `io_ensure()`；常量 `ttf_font.c:116-119`：`TTF_IO_SLOTS 32 × 4KiB` + `TTF_IO_RUN_MAX 8 × 4KiB` = **160KiB/槽**，4 槽最多 **640KiB**。
- 内建/嵌入字体 `font_mem != NULL`、`font_fd == -1`、`glyf_ram` 为 NULL，`file_read_at` 永远走内存路径（`ttf_font.c:640-644`）；`warm_text_io`（`:2258`）却没判 `font_mem`，仍逐字符去重、`io_touch`、`io_flush_touches` 的 qsort，最后 `lseek(-1)` 全失败，每次测量+绘制各一遍。
- 修法：只在 `font_fd >= 0` 时建缓冲（或 `io_load_block` 内惰性建）；`warm_text_io` 加 `font_mem` 早退。
- 风险：低。

### 2.6 TXT 章节缓冲 3× 超配 + 新旧章节并存

- `main/book/book_txt.c:826`：`size_t cap = (size_t)(end - start) * 3 + 1;`（`BOOK_CHAPTER_BYTES_MAX` 96KiB → 288KiB，与编码无关）。
- `app_book.c:5815`：旧 `s_text` 在新布局成功后才释放 → 切章峰值 = 旧 288KiB + 新 288KiB + 两张块表。
- EPUB 侧 `html_text.c:483-500` 按需倍增；`book_epub.c:2202-2209` 解析期间整份 XHTML 仍在。
- 修法：TXT 按编码取界（GBK ≤2×；UTF-8 只在 0x800+ 取 3×）；EPUB 已知输入长度时一次 `reserve_text(len+1)`；>64KiB 优先走流式 part 路径。
- 风险：低估边界会截断（保留硬上限 + 按需增长）。

---

## 3. 启动 / 持久化 / 刷新策略

### 3.1 `boot_state` 每次 checkpoint 全量 ~900B blob + commit

- `main/boot_state.c:116-137`：`nvs_set_blob(h, "state", packed, size)` + `nvs_commit(h)`，无 dirty 判断；`BOOT_PACKED_MAX` = 头 + 3×`PICO_BOOT_PATH_MAX`。
- 调用点：`pico_boot_init`（`:216`）、`pico_boot_ready`（`:222`）、`pico_boot_save_resume`（`:244`）、`hold_resume`、`mark_frame_cached`、`take_resume`、`clear_resume`、`book_begin/end`。开机 ≥2 次，锁/解锁 2–3 次。
- 修法：缓存上次落盘字节，`memcmp` 相同即跳过；`starting` 拆成 1 字节键。
- 风险：低。

### 3.2 设置逐键 open/commit；`nvs_put_u8` 吞错误

- `main/settings.c:698-703`：每次 `nvs_open` + `nvs_set_u8` + `nvs_commit`，返回 `void`。
- `app_book.c:7272-7275`：一次样式调整连发 `set_book_px/margin/line_spacing/paragraph_spacing` + `save_progress`（后者还有 `seq` 的独立 commit，`book_progress.c:289-310`）。
- `settings.c:776-781` 一类 void setter：先改 RAM 再写 NVS，NVS 满时不报错 → UI 显示新值、重启回退且无提示（该文件明确不自动擦除，`256-262`）。
- 修法：分组 setter（单句柄单 commit）或面板关闭/空闲时合并落盘；失败时回滚并置"未保存"标志。
- 风险：低-中（断电丢最后一次调整）。

### 3.3 5ms 固定轮询 + 400kHz I²C 触摸每 tick 一次

- `main/app/app_loop.c:115` `LOOP_TICK_MS 5`、`:1132` `cst836u_read(...)`、`:1971` 延时。
- `components/cst836u/cst836u.c:48-52`：`i2c_master_transmit` + `esp_rom_delay_us(5)` + `i2c_master_receive`，15 字节 @400kHz；INT 脚只配输入、`GPIO_INTR_DISABLE`（`:66-73`）。
- ≈0.4ms/5ms ≈ 单核 8%，且 200 次/秒唤醒阻止任何空闲/降频窗口（S31 走 `metalio_ft6336.c` 的 IRQ worker，无此开销）。
- 同一节奏让 `ble_pt_sync` 在失败退避期热：`components/ble_page_turner/src/ble_page_turner.c:1860-1876` 先 `boot_tries_load()`+`why_load()` 两次 NVS open，再判"locked/backing off"。
- 修法：无触摸时降到 20–30ms 并先用 INT 电平门控；BLE 退避/锁判断提前到 NVS 读取之前。
- 风险：中（INT 极性需实机确认）。

### 3.4 S31 每次"二值系统页"输出整屏归一化

- `boards/metalio47/src/main/display.c:662`：`if (s_system_binary){ system_normalize(hl); ... }`，对任意 `area` 都执行。
- `system_normalize`（`:309-314`）→ `display_monochrome_frame`（`:283-294`）遍历 416KiB；`system_navigation_gray_changed`（`:312-324`）再扫 44KiB 导航条。
- `main/app/app_loop.c:579-580`：设置/文件页点击反馈前又做一次 content 归一化，然后才推一个小区域。
- 修法：局部 `area` 已二值时，只归一化裁剪区；去掉点击反馈前的 app 层预归一化。
- 风险：中（局部更新区域若含灰阶需保留）。

### 3.5 Pico 快刷切页：整屏归一化 + 带缓存拷贝 + 二次扫描

- `boards/pico/src/main/display.c:304-305`：`main_normalize_body` 读改写 684×544 ≈ 372KiB，`main_body_changed` 再读 372KiB。
- `:309` 每次 `heap_caps_aligned_alloc` 带缓存；`:200` 分段 `memcpy` 出、`:395` 拷回；`:406` 再对灰阶带做一次 `epd_hl_update_area(..., MODE_GL16, ...)`。
- 修法：归一化与脏行走合并（跳过未变 32 位字/整行）；带缓存改常驻静态缓冲；灰阶带与黑白同一扫描（S31 的 `e0470_update_regions` 思路）。
- 风险：中-高（波形改动需面板验证）。

### 3.6 S31 差分卸载超时 `abort()`

- `boards/metalio47/src/components/epdiy/src/render.c:663-680`：主任务 `xSemaphoreTake(s_diff_done, 2000ms)`；失败后取消再等 100ms，仍失败直接 `abort()`。
- 条件 `crop >= 32×128` 且 `epd_pipeline_enabled()`（普通构建恒真）→ 几乎每次局刷/全刷都走。
- 后果：core1 卡顿冻结 UI 2s；第二次超时在屏幕半帧状态 panic，重启留条纹。
- 修法：超时窗口缩到 30–50ms，一次失败即内联计算；永不 `abort()`，置故障标志走既有清白恢复。
- 风险：中（退出时共享 `difference_fb`/脏数组的归属要沿用现有 cancel-and-join）。

### 3.7 翻页引擎忙等补齐 tick 预算

- `boards/pico/src/components/e0470_page_turn/e0470_page_turn.c:376-379`、`boards/metalio47/.../e0470_page_turn.c:423-426`：
  `if (s_tick_us > 0 && used < s_tick_us) esp_rom_delay_us(s_tick_us - used);`
- `E0470_TURN_FAST_TICK_US 11090`，一轮最多几十 tick → 空闲时满频空转，且持显示递归锁。
- 修法：改成 `vTaskDelay`/`vTaskDelayUntil` 到同一 deadline。
- 风险：中（1ms tick 粒度带来抖动，须两板实屏比对残影）。

---

## 4. 闪存 / 分区债务（约束所有取舍）

### 4.1 应用槽贴顶，LTO 未开

- `flash/manifest.json`：pico `firmware.bin` **4,193,792** B / `ota_0` `0x400000` = 4,194,304 → **剩 512 B**。
- `flash/metalio/manifest.json`：**5,190,240** / `0x500000` = 5,242,880 → **剩 52,640 B（1%）**。
- `boards/pico/sdkconfig.defaults:34`、`boards/metalio47/sdkconfig.defaults:17` 只有 `CONFIG_COMPILER_OPTIMIZATION_PERF=y`，**都没有 `CONFIG_COMPILER_LTO`**。
- 后果：`#pragma GCC optimize("Os")` 已蔓延到阅读路径（`main/apps/book_reader_chapter.inc:5`、`book_reader_heading.inc:4`、`book_chapter_label.h:13`），CMake 又给 `book_epub/book_layout/zip_reader/html_text/gbk/app_files/app_device_settings` 加 `-Os`（`main/CMakeLists.txt:143-150`、`:337-347`、`:369-376`）。
- 修法：先删死资产（4.2/4.4）腾出余量，再试 `CONFIG_COMPILER_LTO=y` 测体积，最后按实测收回热路径的 `-Os`。
- 风险：LTO 影响内联/栈深/IRAM 与 `--wrap`，需要完整真机回归。

### 4.2 pico 分区尾部 3.00MB 未分配

- `boards/pico/partitions.csv`：`otadata` 结束于 `0xD12000`，flash 16MiB → 空洞 `0x2EE000` ≈ **3,072,000 B**。
- 现状 `storage` 仅 5MiB（S31 用 5.875MiB 且用满）。
- 修法：新布局构建把 `storage` 扩到 `0x7EE000`（或拆出 coredump）。
- 风险：改分区布局会破坏既有 FAT/NVS/OTA 假设，必须版本化，不能静默替换。

### 4.3 `assets/loading_4bpp.bin` 406KB 只出现在 pico `EMBED_FILES`

- `main/CMakeLists.txt:360` 嵌入；全仓代码无 `_binary_loading_4bpp_bin` 引用（只有 `tools/simulator/` 自己加载）。
- 要么被 `--gc-sections` 丢弃（只是链接输入噪音），要么真占 406KB。先删。
- 风险：无（删前确认没有资源指针表间接引用）。

### 4.4 其他可回收闪存

- S31 搜索表用未打包格式：`components/read_pico_search/read_pico_search.c:13-31`，`search_table.h` 32 位码点（649.5KB）vs `search_packed.h` 18 位（826.9KB 源，pico 用），约省 **89KB**；反向候选表与读音表互为镜像（约 55KB）。
- UI 图标 72×72 4bpp：`boards/{pico,metalio47}/src/main/assets/ui_icons.h`，`UI_ICON_MASK_BYTES = 2592`，51/53 个 → **132/137KB**；实际绘制多为 24–56px（`app_book.c:2016`、`app_files.c:701-712`、`ui_nav.c:37-40`），仅 `ui_power_dialog.c:231` 用 72px。
- 修法：重新生成 48×48/56×56 掩模（省 50–70KB），或照 `ui_hanzi.c` 的 miniz 惰性解压模式压缩。

### 4.5 板级字库资产是生效的（避免误判）

`tools/board_source_view.py` 的 `materialize()` 会把 `boards/<board>/src/**` 覆盖到构建视图的 `main/assets/`，所以 `EMBED_FILES` 看似指向 `main/assets`，实际打进镜像的是板级 PIF4 字库与板级 `builtin.ttf`。审查中曾因此误判，记录在此。

---

## 5. 次级项（记录，不展开）

- `chapter_breaks_prepare`（`book_epub.c:1233-1323`）对每标题线性扫 authored navigation（`:1195`），并有 O(blocks²) 的 `repeated_later`（`:1279-1281`）；128KiB 整章 + 3000 项目录时会放大。
- 每次章节重入都重新解析+重排版，无章节文本/页缓存。
- `book_layout_build_blocks` 每次 build 都 `book_layout_free()` 再重建页数组（`book_layout.c:1240-1246`、`727-750`）。
- `doc/footnote` 的 `duplicate` 去重只在单次调用内（见 §1.6）。
- `file_tree_delete/copy` 两遍遍历（`file_tree.c:117-121/190-196`），每层目录一次 PSRAM 帧。
- `scan_shelf` 在 pico 上不检查触摸中断（`app_book.c:6434-6444` 仅 S31 有 `shelf_scan_interrupted`），大库扫描期间冻结输入。
- 书架索引：`shelf_cache_load` 已 `stat` 全部路径（`app_book.c:6697-6701`），随后 `scan_shelf` 再 `stat` 一遍（`:6448`），最后整表重写（`:7005`，临时文件+rename，~696B/条）。
- `ui_hanzi_aa_scale.h:28-88` 补字 AA：逐像素重算 4×4 浮点权重 + 每像素 2–4 次整数除法 + 16 次二分（约 4k 次边缘求值）/字形；权重只依赖 x/y，可提出循环，二分可换 256 桶前缀和。
- `stb_truetype` 实现在 `ui_font.c:21-27` 与 `ttf_font.c:70-77` 各编译一份（static），flash 里有潜在双份栅格化器。
