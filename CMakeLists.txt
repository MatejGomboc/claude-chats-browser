cmake_minimum_required(VERSION 3.24)

project(claude-chats-browser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Sql Svg Test)

qt_standard_project_setup()

qt_add_executable(claude-chats-browser
    WIN32 MACOSX_BUNDLE
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
)

qt_add_resources(claude-chats-browser "assets"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/theme.qss
        resources/icons/chat.svg
        resources/icons/close.svg
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
)

install(TARGETS claude-chats-browser
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET claude-chats-browser
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
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

# So ctest can find the Qt runtime DLLs (Windows has no rpath) without the caller
# having to put Qt's bin directory on PATH first.
set_tests_properties(conversation_tree PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
