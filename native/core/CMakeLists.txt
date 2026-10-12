find_package(Threads REQUIRED)

add_library(autodj_core STATIC
  src/Param.cpp
  src/ParamSerializer.cpp
  src/SampleBuffer.cpp
  src/DemoSources.cpp
  src/RtGuard.cpp)
target_include_directories(autodj_core PUBLIC include)
target_link_libraries(autodj_core PUBLIC autodj_options nlohmann_json::nlohmann_json Threads::Threads)

if(AUTODJ_RT_GUARD)
  add_library(autodj_rt_alloc_guard OBJECT src/RtAllocGuard.cpp)
  target_include_directories(autodj_rt_alloc_guard PRIVATE include)
  target_link_libraries(autodj_rt_alloc_guard PRIVATE autodj_options)
endif()
