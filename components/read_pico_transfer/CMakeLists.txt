include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 独立热点传书组件与嵌入页面。/ Standalone hotspot transfer component and embedded page.
kiiko_board_sources(kiiko_selected_sources "${CMAKE_CURRENT_LIST_DIR}" "read_pico_transfer.c")
idf_component_register(SRCS ${kiiko_selected_sources}
    INCLUDE_DIRS "include"
    REQUIRES kiiko_locale ble_page_turner read_pico_search esp_wifi esp_netif esp_event esp_http_server nvs_flash fatfs esp_timer espressif__cjson
    EMBED_TXTFILES "upload.html")
