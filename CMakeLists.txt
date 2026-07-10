cmake_minimum_required(VERSION 3.21)
project(fm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认带调试信息的优化构建（用户可通过 -DCMAKE_BUILD_TYPE= 覆盖）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# ---- ASan/LSan 构建选项 ----
# cmake -DFM_ENABLE_ASAN=ON ..
option(FM_ENABLE_ASAN "Enable AddressSanitizer + LeakSanitizer" OFF)
if(FM_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
endif()

# 标准安装目录变量（跨发行版兼容）
include(GNUInstallDirs)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Core Gui Concurrent DBus Network LinguistTools)

qt_standard_project_setup()

# ---- 核心逻辑库（无 QtWidgets 依赖，可独立单元测试） ----
qt_add_library(fm_core STATIC
    src/core/config_manager.h
    src/core/config_manager.cpp
    src/core/favorite_manager.h
    src/core/favorite_manager.cpp
    src/core/shortcut_manager.h
    src/core/shortcut_manager.cpp
    src/core/session_state.h
    src/core/session_state.cpp
    src/core/open_with_manager.h
    src/core/open_with_manager.cpp
    src/core/clipboard_manager.h
    src/core/clipboard_manager.cpp
    src/core/volume_manager.h
    src/core/volume_manager.cpp
    src/fileops/trash_can.h
    src/fileops/trash_can.cpp
    src/filelist/file_item.h
    src/panel/panel_id.h
)

target_link_libraries(fm_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Network
)

target_include_directories(fm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ---- 主程序（UI + 主入口） ----
qt_add_executable(fm
    src/main.cpp
    src/app/fm_application.h
    src/app/fm_application.cpp
    src/app/single_instance.h
    src/app/single_instance.cpp
    src/dialogs/about_dialog.h
    src/dialogs/about_dialog.cpp
    src/dialogs/conflict_dialog.h
    src/dialogs/conflict_dialog.cpp
    src/dialogs/error_dialog.h
    src/dialogs/error_dialog.cpp
    src/dialogs/input_name_dialog.h
    src/dialogs/input_name_dialog.cpp
    src/dialogs/isettings_page.h
    src/dialogs/open_with_dialog.h
    src/dialogs/open_with_dialog.cpp
    src/dialogs/properties_dialog.h
    src/dialogs/properties_dialog.cpp
    src/dialogs/settings_dialog.h
    src/dialogs/settings_dialog.cpp
    src/dialogs/settings_pages.h
    src/dialogs/settings_pages.cpp
    src/core/column_manager.h
    src/core/column_manager.cpp
    src/filelist/file_list_model.h
    src/filelist/file_list_model.cpp
    src/filelist/file_list_sort_proxy.h
    src/filelist/file_list_sort_proxy.cpp
    src/filelist/file_list_view.h
    src/filelist/file_list_view.cpp
    src/fileops/conflict_resolver.h
    src/fileops/conflict_resolver.cpp
    src/fileops/copy_move_job.h
    src/fileops/copy_move_job.cpp
    src/fileops/file_job.h
    src/fileops/file_job.cpp
    src/fileops/file_operations.h
    src/fileops/file_operations.cpp
    src/fileops/progress_dialog.h
    src/fileops/progress_dialog.cpp
    src/panel/file_tab_bar.h
    src/panel/file_tab_bar.cpp
    src/panel/panel_widget.h
    src/panel/panel_widget.cpp
    src/panel/panel_container.h
    src/panel/panel_container.cpp
    src/ui/favorites_menu_controller.h
    src/ui/favorites_menu_controller.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
    src/ui/volume_menu_controller.h
    src/ui/volume_menu_controller.cpp
    resources.qrc
)

target_link_libraries(fm PRIVATE
    fm_core
    Qt6::Widgets
    Qt6::Concurrent
)

# 翻译文件 (.ts → .qm → 嵌入资源)
# 源码以英文为源语言（tr() 默认值），仅维护中文翻译
qt_add_translations(fm
    TS_FILES
        translations/fm_zh.ts
    RESOURCE_PREFIX "/i18n"
)

install(TARGETS fm RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 安装 man 手册（压缩为 .gz）
install(FILES fm.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

# 安装 .desktop 文件
install(FILES fm.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

# 安装应用图标到 hicolor 主题（512x512）
install(FILES fm.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)

# ---- 单元测试 ----
# 仅使用系统已安装的 GTest，不再通过 FetchContent 下载
option(FM_BUILD_TESTS "Build unit tests (requires system-installed GTest)" ON)
if(FM_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# ---- 质量检查自定义 target（方便单独运行） ----
# 使用方式：
#   cmake --build build --target check-format   # 格式检查
#   cmake --build build --target clang-tidy      # 静态分析
#   cmake --build build --target check-tests     # 运行测试
#   cmake --build build --target check-asan      # ASan/LSan 内存安全
#   cmake --build build --target check-all       # 以上全部
#
# 注意：这些 target 不会在普通 build 时自动触发，需显式指定 --target。

# 收集待检查的源文件（src/ 与 tests/ 下的 .cpp/.h）
file(GLOB_RECURSE FM_CHECK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h"
)
# clang-tidy 只处理 .cpp 实现（跳过头文件与 tests 的 test_main.cpp）
file(GLOB_RECURSE FM_TIDY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# --- 1. 格式检查 (clang-format --dry-run --Werror) ---
find_program(CLANG_FORMAT_BIN NAMES clang-format clang-format-19 clang-format-18)
if(CLANG_FORMAT_BIN)
    add_custom_target(check-format
        COMMAND "${CLANG_FORMAT_BIN}" --dry-run --Werror ${FM_CHECK_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running clang-format check (dry-run, Werror)"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; 'check-format' target unavailable")
endif()

# --- 2. 静态分析 (clang-tidy, 依赖 compile_commands.json) ---
find_program(CLANG_TIDY_BIN NAMES clang-tidy clang-tidy-19 clang-tidy-18)
if(CLANG_TIDY_BIN)
    add_custom_target(clang-tidy
        COMMAND "${CLANG_TIDY_BIN}" -p "${CMAKE_BINARY_DIR}" ${FM_TIDY_SOURCES}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
else()
    message(STATUS "clang-tidy not found; 'clang-tidy' target unavailable")
endif()

# --- 3. 运行单元测试 (ctest) ---
if(FM_BUILD_TESTS)
    add_custom_target(check-tests
        COMMAND "${CMAKE_CTEST_COMMAND}" --output-on-failure
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        COMMENT "Running unit tests via ctest"
        DEPENDS test_config_manager test_favorite_manager test_shortcut_manager
        VERBATIM
    )
endif()

# --- 4. ASan/LSan 内存安全验证 ---
# 在独立的 build/asan 子目录中重新配置构建（启用 -DFM_ENABLE_ASAN=ON），
# 避免污染主 build 目录。运行前需确保 build/ 已 configure 过。
add_custom_target(check-asan
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/asan"
    COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_SOURCE_DIR}" -B "${CMAKE_BINARY_DIR}/asan"
            -DCMAKE_BUILD_TYPE=Debug -DFM_ENABLE_ASAN=ON
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}/asan" --parallel
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${CMAKE_BINARY_DIR}/asan" --output-on-failure
    COMMENT "Building & testing with AddressSanitizer + LeakSanitizer"
    VERBATIM
)

# --- 5. 一键全部检查（便利 target） ---
add_custom_target(check-all)
if(CLANG_FORMAT_BIN)
    add_dependencies(check-all check-format)
endif()
if(CLANG_TIDY_BIN)
    add_dependencies(check-all clang-tidy)
endif()
if(FM_BUILD_TESTS)
    add_dependencies(check-all check-tests)
endif()
add_dependencies(check-all check-asan)
