add_executable(TengWangge main.cpp)
target_link_libraries(TengWangge PRIVATE tengwangge_builtin_tools)
tw_apply_warnings(TengWangge)

install(TARGETS TengWangge RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
