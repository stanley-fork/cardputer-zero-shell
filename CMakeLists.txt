cmake_minimum_required(VERSION 3.16)

project(cardputer-zero-shell VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(zero-shell
    main/src/app_catalog.cpp
    main/src/framebuffer_canvas.cpp
    main/src/input_device.cpp
    main/src/main.cpp
    main/src/process_runner.cpp
    main/src/shell_ui.cpp
    main/src/status.cpp
    main/src/pty_terminal.cpp
)

target_include_directories(zero-shell PRIVATE main/include)

target_compile_options(zero-shell PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(zero-shell PRIVATE util)

install(TARGETS zero-shell RUNTIME DESTINATION /opt/cardputer-zero-shell/bin)
