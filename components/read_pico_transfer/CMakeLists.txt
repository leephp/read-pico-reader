include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 独立热点传书组件与嵌入页面。/ Standalone hotspot transfer component and embedded page.
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "read_pico_transfer.c")
idf_component_register(SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES kiiko_locale ble_page_turner read_pico_search esp_wifi esp_netif esp_event esp_http_server nvs_flash fatfs esp_timer espressif__cjson)

# 页面源文件保留可读文本，固件仅嵌入构建时生成的 gzip。
# Keep the editable page source; embed only the build-generated gzip in firmware.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    set(upload_gzip "${CMAKE_CURRENT_BINARY_DIR}/upload.html.gz")
    set(gzip_tool "${CMAKE_CURRENT_LIST_DIR}/gzip_asset.py")
    add_custom_command(OUTPUT "${upload_gzip}"
        COMMAND "${PYTHON}" "${gzip_tool}" "${CMAKE_CURRENT_LIST_DIR}/upload.html" "${upload_gzip}"
        DEPENDS "${CMAKE_CURRENT_LIST_DIR}/upload.html" "${gzip_tool}"
        VERBATIM)
    target_add_binary_data(${COMPONENT_LIB} "${upload_gzip}" BINARY)
endif()
