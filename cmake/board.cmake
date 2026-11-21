# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：只在配置期选择板型，未知设备或目标冲突立即失败。/ English: Select a board at configure time; reject unknown boards and conflicting targets.
# 中文：需求扫描在每个组件自己的作用域中运行，板型变量必须逐次初始化。/ English: Requirement discovery gives each component its own scope; initialize board variables on every include.
get_filename_component(KIIKO_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED KIIKO_BOARD)
    if(DEFINED ENV{KIIKO_BOARD} AND NOT "$ENV{KIIKO_BOARD}" STREQUAL "")
        set(KIIKO_BOARD "$ENV{KIIKO_BOARD}")
    else()
        set(KIIKO_BOARD "pico")
    endif()
endif()
if(NOT KIIKO_BOARD STREQUAL "pico")
    message(FATAL_ERROR "Only Pico is supported; S31 source is archived at s31-final-2026-11-21")
endif()
set(KIIKO_BOARD_ROOT "${KIIKO_SOURCE_ROOT}/boards/${KIIKO_BOARD}")
include("${KIIKO_BOARD_ROOT}/board.cmake")
# Pico sources live directly in main/ and components/; there are no source overlays.
function(kiiko_board_sources output component_dir)
    set(${output} ${ARGN} PARENT_SCOPE)
endfunction()
