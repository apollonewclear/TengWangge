# 每个工具是一个独立的静态库 tw_tool_<name>，只依赖外壳提供的 ITool 接口。
# 新增工具：建目录、在其 CMakeLists.txt 里调用 tw_add_tool()，然后在这里
# add_subdirectory，并在 builtin_tools.cpp 里注册一行。详见 docs/ADDING_A_TOOL.md。
set_property(GLOBAL PROPERTY TW_TOOL_TARGETS "")

function(tw_add_tool name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_library(tw_tool_${name} STATIC ${ARG_SOURCES})
    target_include_directories(tw_tool_${name} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(tw_tool_${name} PUBLIC tengwangge_shell)
    tw_apply_warnings(tw_tool_${name})
    set_property(GLOBAL APPEND PROPERTY TW_TOOL_TARGETS tw_tool_${name})
endfunction()

add_subdirectory(welcome)
add_subdirectory(compare)
add_subdirectory(task_demo)
add_subdirectory(sysinfo)

get_property(_tw_tool_targets GLOBAL PROPERTY TW_TOOL_TARGETS)
add_library(tengwangge_builtin_tools STATIC builtin_tools.cpp builtin_tools.h)
target_include_directories(tengwangge_builtin_tools PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tengwangge_builtin_tools PUBLIC tengwangge_shell ${_tw_tool_targets})
tw_apply_warnings(tengwangge_builtin_tools)
