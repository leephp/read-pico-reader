include("${CMAKE_CURRENT_LIST_DIR}/../cmake/board.cmake")
set(main_requires
    kiiko_locale
    kiiko_board
    read_pico
    read_pico_transfer
    kiiko_device_sync
    read_pico_search
    espressif__qrcode
    espressif__esp_jpeg
    # 渐进式 JPEG；基线仍走 esp_jpeg。/ Progressive JPEG; baseline stays on esp_jpeg.
    jpegdec
    defensiblelogic__lowzag
    # WebP 插图与封面的解码器（libwebp 解码子集）。/ WebP decoder subset for illustrations and covers.
    read_pico_webp
    espressif__libpng
    espressif__zlib
    espressif__esp_tinyusb
    # BLE HID 主机（蓝牙翻页器）。本仓库开了 MINIMAL_BUILD，组件不进依赖图的话连它的
    # Kconfig 都不会加载，sdkconfig.defaults 里的 CONFIG_BT_* 会被判为未知符号而丢弃。
    # BLE HID host (Bluetooth page-turner). This tree sets MINIMAL_BUILD, so a component
    # outside the dependency graph does not even load its Kconfig and the CONFIG_BT_*
    # entries in sdkconfig.defaults are dropped as unknown symbols.
    bt
    # 蓝牙翻页器本体。/ The page-turner itself.
    ble_page_turner
    continuous_du
    e0470_epaper_waveform
    e0470_page_turn
    read_pico_pmu
    fatfs
    wear_levelling
    bootloader_support
    spi_flash
    heap
    nvs_flash
    esp_hw_support
    esp_hal_usb
    esp_rom
    esp_driver_gpio
    esp_driver_usb_serial_jtag
    esp_app_format
    app_update
    esp_http_client esp-tls
    mbedtls
    espressif__cjson
)
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "app_main.c"
        "boot_state.c"
        "apps/app_boot_recovery.c"
        "ota_update.c"
        "ota_online.c"
        "ota_release.c"
        "display.c"
        "display_main_waveform.c"
        "display_reader_waveform.c"
        "sleep.c"
        "time_service.c"
        "lock_pin.c"
        "lock_pin_flow.c"
        "lock_screen.c"
        "settings.c"
        "setup_state.c"
        "apps/app_onboarding.c"
        "factory_reset.c"
        "app/app_loop.c"
        "app/reader_capture.c"
        "app/crash_note.c"
        "app/app_registry.c"
        "ui/ui_kit.c"
        "ui/ui_text_edit.c"
        "ui/ui_text_input.c"
        "ui/ui_ime.c"
        "ui/ui_keyboard.c"
        "ui/ui_gesture.c"
        "ui/ui_glass.c"
        "ui/ui_pinpad.c"
        "ui/ui_power_dialog.c"
        "ui/ui_quick_menu.c"
        "ui/ui_lock_feedback.c"
        "ui/ui_quick_menu_input.c"
        "ui/ui_click_feedback.c"
        "ui/ui_wifi_qr.c"
        "ui/datamatrix.c"
        "ui/ui_menu.c"
        "ui/ui_nav.c"
        "ui/ui_home_widget.c"
        "ui/ui_home_clock.c"
        "ui/ui_image_dither.c"
        "ui/ui_wallpaper.c"
        "ui/ui_avatar.c"
        "ui/ui_reader_background.c"
        "apps/app_book.c"
        "apps/book_toc.c"
        "apps/app_dashboard.c"
        "apps/app_files.c"
        "apps/file_tree.c"
        "apps/file_search.c"
        "apps/app_device_settings.c"
        "apps/app_image.c"
        "apps/app_transfer.c"
        "apps/app_cloud_sync.c"
        "apps/usb_storage.c"
        "book/book_store.c"
        "book/book_xteink_sync.c"
        "book/book_progress.c"
        "book/book_history_backup.c"
        "book/book_title.c"
        "book/book_weread_cover.c"
        "book/book_added.c"
        "book/book_ticket.c"
        "book/book_excerpt.c"
        "book/book_lock_collage.c"
        "book/book_lock_bookmark.c"
        "book/book_source.c"
        "book/book_index_cache.c"
        "book/book_txt.c"
        "book/book_epub.c"
        "book/book_cover.c"
        "book/book_cover_jobs.c"
        "book/book_shelf_page_cache.c"
        "book/cover_ram_cache.c"
        "book/book_image_header.c"
        "book/book_cover_auto.c"
        "book/zip_reader.c"
        "book/html_text.c"
        "book/book_layout.c"
        "book/gbk.c"
        "font/ttf_font.c"
        "font/online_font.c"
        "font/online_font_store.c"
        "ui/ui_online_fonts.c"
        "font/app_font_context.c"
        "font/ui_font.c"
        "font/ui_hanzi.c"
        "factory/pmu_selftest.c"
        "factory/vcom_setup.c")
# 设置页几乎全是文字与命中测试，Pico 的应用分区又贴着上限，因此只给这一页按体积编译。
# The settings page is nearly all text and hit-tests, and the Pico app partition sits at its ceiling,
# so only this page is compiled for size.
if(NOT CMAKE_SCRIPT_MODE_FILE)
foreach(kiiko_source IN LISTS kiiko_selected_sources)
    if(kiiko_source MATCHES "(app_device_settings|book_epub|book_layout|book_store|book_source|book_txt|zip_reader|html_text|gbk|app_files)\\.c$")
        set_source_files_properties("${kiiko_source}" PROPERTIES COMPILE_OPTIONS "-Os")
    endif()
endforeach()
endif()
idf_component_register(
    SRCS ${kiiko_selected_sources}
    REQUIRES ${main_requires}
    INCLUDE_DIRS "." "app" "apps" "font" "ui" "factory" "book"
    EMBED_FILES
        "assets/builtin.ttf"
        "assets/ui-hanzi.bin"
        "assets/display-test.png"
)

# 包住 IDF 的 panic 处理器：崩溃现场在重启前落进 RTC，没有串口时那是唯一的现场。
# 链接选项挂在 main 上，INTERFACE 会带到最终链接；写在 project() 之后的全局属性则太晚。
# Wrap the IDF panic handler so the crash scene reaches RTC before a restart; without a console
# that is the only place it survives. The option hangs off main, whose INTERFACE carries it into
# the final link; a global property set after project() is too late for the linker to see it.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")

# 本地 USB 测时入口，普通构建不包含。/ Local USB timing probe, omitted from ordinary product builds.
if("$ENV{KIIKO_LOCAL_LATENCY}" STREQUAL "1")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE KIIKO_LOCAL_LATENCY=1)
endif()


    # Keep recovery and storage compatible with the existing 4 MiB application slots.
    # Size-optimize settings/shelf journals and file/transfer screens; retain renderer/decoder loops.
    # 冷路径（设置、OTA、锁屏、壁纸、工厂页、封面索引）一律按体积编译；排版与字形仍走 PERF。
    # Cold paths (settings, OTA, lock screen, wallpaper, factory pages, cover index) compile for size;
    # layout and glyph paths keep PERF.
    foreach(source IN LISTS kiiko_selected_sources)
        if(source MATCHES "(^|/)(settings|lock_screen|factory_reset|ota_(update|online|release)|ui/ui_(wallpaper|avatar|home_widget|home_clock|online_fonts)|apps/(app_(device_settings|files|transfer|onboarding|dashboard|cloud_sync|image)|file_search|usb_storage)|factory/(pmu_selftest|vcom_setup)|book/(book_added|book_weread_cover|book_lock_collage|book_history_backup|book_index_cache|book_title))[.]c$")
            set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS "-Os")
        endif()
    endforeach()
    option(PICO_MAIN_CLEAN_EXPERIMENT "Local Pico stronger root-switch white erasure" OFF)
    if(PICO_MAIN_CLEAN_EXPERIMENT)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE KIIKO_PICO_MAIN_CLEAN=1)
    endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE KIIKO_UI_LOCALIZATION=1)
