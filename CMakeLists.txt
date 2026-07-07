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

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets Core Gui Concurrent DBus LinguistTools)

qt_standard_project_setup()

qt_add_executable(fm
    src/main.cpp
    src/app/fm_application.h
    src/app/fm_application.cpp
    src/core/config_manager.h
    src/core/config_manager.cpp
    src/core/session_state.h
    src/core/session_state.cpp
    src/filelist/file_item.h
    src/filelist/file_item.cpp
    src/filelist/file_list_model.h
    src/filelist/file_list_model.cpp
    src/filelist/file_list_sort_proxy.h
    src/filelist/file_list_sort_proxy.cpp
    src/filelist/file_list_view.h
    src/filelist/file_list_view.cpp
    src/panel/panel_id.h
    src/panel/file_tab_bar.h
    src/panel/file_tab_bar.cpp
    src/panel/panel_widget.h
    src/panel/panel_widget.cpp
    src/panel/panel_container.h
    src/panel/panel_container.cpp
    src/ui/main_window.h
    src/ui/main_window.cpp
)

target_link_libraries(fm PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
    Qt6::DBus
)

install(TARGETS fm RUNTIME DESTINATION bin)
