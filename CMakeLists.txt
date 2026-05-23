cmake_minimum_required(VERSION 3.16)

project(cardputer-zero-shell VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PNG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
find_file(XDG_SHELL_PROTOCOL xdg-shell.xml
    PATHS
        /usr/share/wayland-protocols/stable/xdg-shell
        /usr/share/wayland-protocols/unstable/xdg-shell
        /usr/share/wayland
    REQUIRED
)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(XDG_SHELL_HEADER "${GENERATED_DIR}/xdg-shell-client-protocol.h")
set(XDG_SHELL_SOURCE "${GENERATED_DIR}/xdg-shell-protocol.c")

add_custom_command(
    OUTPUT "${XDG_SHELL_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
    COMMAND "${WAYLAND_SCANNER}" client-header "${XDG_SHELL_PROTOCOL}" "${XDG_SHELL_HEADER}"
    DEPENDS "${XDG_SHELL_PROTOCOL}"
)

add_custom_command(
    OUTPUT "${XDG_SHELL_SOURCE}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GENERATED_DIR}"
    COMMAND "${WAYLAND_SCANNER}" private-code "${XDG_SHELL_PROTOCOL}" "${XDG_SHELL_SOURCE}"
    DEPENDS "${XDG_SHELL_PROTOCOL}"
)

add_executable(zero-shell-wayland
    main/src/app_catalog.cpp
    main/src/image.cpp
    main/src/status.cpp
    main/src/zero_shell_wayland.cpp
    "${XDG_SHELL_HEADER}"
    "${XDG_SHELL_SOURCE}"
)

target_include_directories(zero-shell-wayland PRIVATE
    main/include
    "${GENERATED_DIR}"
)

target_compile_options(zero-shell-wayland PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(zero-shell-wayland PRIVATE
    PNG::PNG
    PkgConfig::WAYLAND_CLIENT
    m
)

install(TARGETS zero-shell-wayland RUNTIME DESTINATION /opt/cardputer-zero-shell/bin)
