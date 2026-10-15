# Benchmark core: shared by the host CLI (bench_granular) and the on-device runner (JNI).
add_library(autodj_bench STATIC BenchCore.cpp)
target_include_directories(autodj_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(autodj_bench PUBLIC autodj_engine)

if(NOT ANDROID)
  add_executable(bench_granular bench_granular.cpp)
  target_link_libraries(bench_granular PRIVATE autodj_bench)
endif()
