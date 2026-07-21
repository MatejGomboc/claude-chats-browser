cmake_minimum_required(VERSION 3.24)

project(claude-chats-browser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(claude-chats-browser
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/main_window.cpp
    src/main_window.hpp
    src/main_window.ui
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
