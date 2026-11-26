cmake_minimum_required(VERSION 3.16)

project(TengWangge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TENGWANGGE_BUILD_TESTS "Build automated tests" ON)
option(TENGWANGGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# Qt 5.15 only. Qt6 is intentionally not supported by this tree.
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Widgets)

function(tw_apply_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(TENGWANGGE_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE -Werror)
    endif()
endfunction()

add_subdirectory(src/core)
add_subdirectory(src/shell)
add_subdirectory(src/tools)
add_subdirectory(src/app)

install(FILES packaging/TengWangge.desktop DESTINATION share/applications)
install(FILES packaging/tengwangge.svg DESTINATION share/icons/hicolor/scalable/apps)

if(TENGWANGGE_BUILD_TESTS)
    enable_testing()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
