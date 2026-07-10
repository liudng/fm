# 测试目标
set(FM_TEST_TARGETS
    test_config_manager
    test_favorite_manager
    test_shortcut_manager
)

# 辅助函数: 创建测试可执行文件
function(fm_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${name}
        ${ARG_SOURCES}
    )
    target_link_libraries(${name} PRIVATE
        fm_core
        GTest::gtest
    )
    set_target_properties(${name} PROPERTIES
        AUTOMOC ON
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ConfigManager 测试
fm_add_test(test_config_manager
    SOURCES
        test_config_manager.cpp
        test_main.cpp
)

# FavoriteManager 测试
fm_add_test(test_favorite_manager
    SOURCES
        test_favorite_manager.cpp
        test_main.cpp
)

# ShortcutManager 测试
fm_add_test(test_shortcut_manager
    SOURCES
        test_shortcut_manager.cpp
        test_main.cpp
)
