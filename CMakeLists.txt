cmake_minimum_required(VERSION 3.16)

project(TengWangge VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 安装后的二进制不携带构建机个人目录下的临时 RPATH，保证跨机器分发安全。
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TENGWANGGE_BUILD_TESTS "Build automated tests" ON)
option(TENGWANGGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(TENGWANGGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(TENGWANGGE_PREFER_SYSTEM_QT "Prefer system Qt5 (/usr/lib64 or /usr/lib) on Linux" ON)

# 在麒麟 KylinSec / openEuler / Ubuntu 上优先选择系统发行版自带的 Qt 5.15，避免误链个人目录私有 Qt。
if(TENGWANGGE_PREFER_SYSTEM_QT AND UNIX AND NOT APPLE AND NOT DEFINED Qt5_DIR)
    foreach(_tw_sys_qt_dir
        /usr/lib64/cmake/Qt5
        /usr/lib/x86_64-linux-gnu/cmake/Qt5
        /usr/lib/aarch64-linux-gnu/cmake/Qt5
        /usr/lib/cmake/Qt5)
        if(EXISTS "${_tw_sys_qt_dir}/Qt5Config.cmake")
            set(Qt5_DIR "${_tw_sys_qt_dir}" CACHE PATH "System Qt5 CMake directory")
            message(STATUS "Preferring system Qt5: ${Qt5_DIR}")
            break()
        endif()
    endforeach()
endif()

# Qt 5.15 only. Qt6 is intentionally not supported by this tree.
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets)

function(tw_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(TENGWANGGE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(TENGWANGGE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(TENGWANGGE_ENABLE_SANITIZERS)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PUBLIC -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

add_subdirectory(src/core)
add_subdirectory(src/shell)
add_subdirectory(src/tools)
add_subdirectory(src/app)

install(FILES packaging/TengWangge.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/tengwangge.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

if(TENGWANGGE_BUILD_TESTS)
    enable_testing()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
