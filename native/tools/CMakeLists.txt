add_executable(render_cli render_cli.cpp)
target_link_libraries(render_cli PRIVATE autodj_engine)

add_executable(stress_test stress_test.cpp)
target_link_libraries(stress_test PRIVATE autodj_engine)
autodj_enable_rt_guard(stress_test)
