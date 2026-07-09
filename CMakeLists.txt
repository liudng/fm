cmake_minimum_required(VERSION 3.21)
project(fm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认 Release 风格的调试信息
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Core Gui Concurrent DBus Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(fm
    src/main.cpp
    src/app/fm_application.h
    src/app/fm_application.cpp
    src/app/single_instance.h
    src/app/single_instance.cpp
    src/core/clipboard_manager.h
    src/core/clipboard_manager.cpp
    src/core/column_manager.h
    src/core/column_manager.cpp
    src/core/config_manager.h
    src/core/config_manager.cpp
    src/core/favorite_manager.h
    src/core/favorite_manager.cpp
    src/core/open_with_manager.h
    src/core/open_with_manager.cpp
    src/core/session_state.h
    src/core/session_state.cpp
    src/core/shortcut_manager.h
    src/core/shortcut_manager.cpp
    src/core/volume_manager.h
    src/core/volume_manager.cpp
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
    src/filelist/file_item.h
    src/filelist/file_item.cpp
    src/filelist/file_list_model.h
    src/filelist/file_list_model.cpp
    src/filelist/file_list_sort_proxy.h
    src/filelist/file_list_sort_proxy.cpp
    src/filelist/file_list_view.h
    src/filelist/file_list_view.cpp
    src/fileops/conflict_resolver.h
    src/fileops/conflict_resolver.cpp
    src/fileops/file_operations.h
    src/fileops/file_operations.cpp
    src/fileops/progress_dialog.h
    src/fileops/progress_dialog.cpp
    src/fileops/trash_can.h
    src/fileops/trash_can.cpp
    src/panel/panel_id.h
    src/panel/file_tab_bar.h
    src/panel/file_tab_bar.cpp
    src/panel/panel_widget.h
    src/panel/panel_widget.cpp
    src/panel/panel_container.h
    src/panel/panel_container.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
    resources.qrc
)

target_link_libraries(fm PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::DBus
    Qt6::Network
)

# 翻译文件 (.ts → .qm → 嵌入资源)
qt_add_translations(fm
    TS_FILES
        translations/fm_zh.ts
        translations/fm_en.ts
    RESOURCE_PREFIX "/i18n"
)

install(TARGETS fm RUNTIME DESTINATION bin)

# 安装 man 手册（压缩为 .gz）
install(FILES fm.1 DESTINATION share/man/man1)

# 安装 .desktop 文件
install(FILES fm.desktop DESTINATION share/applications)

# 安装应用图标到 hicolor 主题（512x512）
install(FILES fm.png DESTINATION share/icons/hicolor/512x512/apps RENAME fm.png)
