cmake_minimum_required(VERSION 3.24)

project(claude-chats-browser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Svg Test Concurrent)

qt_standard_project_setup()

qt_add_executable(claude-chats-browser
    WIN32 MACOSX_BUNDLE
    src/artifact_reconstructor.cpp
    src/artifact_reconstructor.hpp
    src/artifacts_panel.cpp
    src/artifacts_panel.hpp
    src/bar_chart.cpp
    src/bar_chart.hpp
    src/code_highlighter.cpp
    src/code_highlighter.hpp
    src/collapsible_section.cpp
    src/collapsible_section.hpp
    src/conversation_list_model.cpp
    src/conversation_list_model.hpp
    src/conversation_reader.cpp
    src/conversation_reader.hpp
    src/conversation_tree.cpp
    src/conversation_tree.hpp
    src/database.cpp
    src/database.hpp
    src/find_bar.cpp
    src/find_bar.hpp
    src/icon_util.cpp
    src/icon_util.hpp
    src/import_worker.cpp
    src/import_worker.hpp
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
    src/main_window.ui
    src/message_widget.cpp
    src/message_widget.hpp
    src/stats_collector.cpp
    src/stats_collector.hpp
    src/stats_panel.cpp
    src/stats_panel.hpp
)

qt_add_resources(claude-chats-browser "assets"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/theme.qss
        resources/icons/chat.svg
        resources/icons/close.svg
        resources/icons/copy.svg
)

target_compile_definitions(claude-chats-browser
    PRIVATE
        APP_VERSION_STRING="${PROJECT_VERSION}"
)

set_target_properties(claude-chats-browser PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(MSVC)
    target_compile_options(claude-chats-browser PRIVATE /W4 /WX)
else()
    target_compile_options(claude-chats-browser PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(claude-chats-browser
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        Qt6::Svg
        Qt6::Concurrent
)

install(TARGETS claude-chats-browser
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# --compiler-runtime bundles the MSVC C++ runtime DLLs so the deployed app
# runs on machines without the VC redistributable (windeployqt-only option).
if(WIN32)
    set(deploy_tool_options DEPLOY_TOOL_OPTIONS --compiler-runtime)
else()
    set(deploy_tool_options "")
endif()

qt_generate_deploy_app_script(
    TARGET claude-chats-browser
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    ${deploy_tool_options}
)
install(SCRIPT ${deploy_script})

# Tests
enable_testing()

qt_add_executable(tst_conversation_tree
    tests/tst_conversation_tree.cpp
    src/conversation_tree.cpp
    src/conversation_tree.hpp
)

target_include_directories(tst_conversation_tree PRIVATE src)

if(MSVC)
    target_compile_options(tst_conversation_tree PRIVATE /W4 /WX)
else()
    target_compile_options(tst_conversation_tree PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(tst_conversation_tree
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME conversation_tree COMMAND tst_conversation_tree)

# Replaying create/update/rewrite artifact ops must yield each artifact's final content.
qt_add_executable(tst_artifact_reconstructor
    tests/tst_artifact_reconstructor.cpp
    src/artifact_reconstructor.cpp
    src/artifact_reconstructor.hpp
)

target_include_directories(tst_artifact_reconstructor PRIVATE src)

if(MSVC)
    target_compile_options(tst_artifact_reconstructor PRIVATE /W4 /WX)
else()
    target_compile_options(tst_artifact_reconstructor PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(tst_artifact_reconstructor
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Qt6::Test
)

add_test(NAME artifact_reconstructor COMMAND tst_artifact_reconstructor)

# The archive-statistics aggregation must count blocks, months and tools correctly.
qt_add_executable(tst_stats_collector
    tests/tst_stats_collector.cpp
    src/stats_collector.cpp
    src/stats_collector.hpp
)

target_include_directories(tst_stats_collector PRIVATE src)

if(MSVC)
    target_compile_options(tst_stats_collector PRIVATE /W4 /WX)
else()
    target_compile_options(tst_stats_collector PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(tst_stats_collector
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME stats_collector COMMAND tst_stats_collector)

# GUI test: opening a large conversation must stream in without freezing the UI.
qt_add_executable(tst_reader_performance
    tests/tst_reader_performance.cpp
    src/code_highlighter.cpp
    src/code_highlighter.hpp
    src/collapsible_section.cpp
    src/collapsible_section.hpp
    src/conversation_reader.cpp
    src/conversation_reader.hpp
    src/conversation_tree.cpp
    src/conversation_tree.hpp
    src/icon_util.cpp
    src/icon_util.hpp
    src/message_widget.cpp
    src/message_widget.hpp
)

target_include_directories(tst_reader_performance PRIVATE src)

if(MSVC)
    target_compile_options(tst_reader_performance PRIVATE /W4 /WX)
else()
    target_compile_options(tst_reader_performance PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(tst_reader_performance
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        Qt6::Svg
        Qt6::Test
)

add_test(NAME reader_performance COMMAND tst_reader_performance)

# Proves the conversation search runs off the UI thread (never blocks on a slow query).
qt_add_executable(tst_search_async
    tests/tst_search_async.cpp
    src/conversation_list_model.cpp
    src/conversation_list_model.hpp
    src/database.cpp
    src/database.hpp
    src/icon_util.cpp
    src/icon_util.hpp
)

target_include_directories(tst_search_async PRIVATE src)

if(MSVC)
    target_compile_options(tst_search_async PRIVATE /W4 /WX)
else()
    target_compile_options(tst_search_async PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(tst_search_async
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Sql
        Qt6::Svg
        Qt6::Concurrent
        Qt6::Test
)

add_test(NAME search_async COMMAND tst_search_async)

# So ctest can find the Qt runtime DLLs (Windows has no rpath) without the caller
# having to put Qt's bin directory on PATH first, and run the GUI test headless.
set_tests_properties(conversation_tree PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

set_tests_properties(artifact_reconstructor PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

set_tests_properties(stats_collector PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

# Runs on the real platform (the offscreen QPA ships no fonts, which makes text layout
# pathologically slow); harmless locally, and CI can supply a font + offscreen later.
set_tests_properties(reader_performance PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

set_tests_properties(search_async PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
