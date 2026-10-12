add_library(autodj_fx STATIC
  src/ChaosFx.cpp)
target_include_directories(autodj_fx PUBLIC include)
target_link_libraries(autodj_fx PUBLIC autodj_core)
