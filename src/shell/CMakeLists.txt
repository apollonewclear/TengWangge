add_library(tengwangge_shell STATIC
    elided_label.cpp
    elided_label.h
    i_tool.h
    icon_factory.cpp
    icon_factory.h
    log_panel.cpp
    log_panel.h
    main_window.cpp
    main_window.h
    page_chrome.cpp
    page_chrome.h
    status_strip.cpp
    status_strip.h
    theme.cpp
    theme.h
    tool_host.cpp
    tool_host.h
    tool_rail.cpp
    tool_rail.h
    tool_registry.cpp
    tool_registry.h
    tool_view.cpp
    tool_view.h
    tengwangge_shell.qrc
)

target_include_directories(tengwangge_shell PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tengwangge_shell PUBLIC tengwangge_core Qt5::Widgets)
tw_apply_warnings(tengwangge_shell)
