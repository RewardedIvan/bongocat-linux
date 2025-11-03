cmake_minimum_required(VERSION 3.10)
project(bongocat-linux)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(vendor/raylib EXCLUDE_FROM_ALL)

add_executable(bongocat src/main.c)

target_include_directories(bongocat PRIVATE vendor/raylib/src vendor/raylib/src/external/glfw/include)
target_link_libraries(bongocat PRIVATE raylib)

set(CMAKE_CXX_STANDARD 23)
add_library(hypr_bongocat SHARED src/server.c src/hypr.cpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pangocairo
    pixman-1
    wayland-server
    xkbcommon
)
target_link_libraries(hypr_bongocat PRIVATE rt PkgConfig::deps)

install(TARGETS bongocat
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)

install(DIRECTORY assets/
        DESTINATION share/bongocatl/assets)
