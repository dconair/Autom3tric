add_library(autodj_engine STATIC
  src/Engine.cpp
  src/ToneNode.cpp
  src/Tracks.cpp)
target_include_directories(autodj_engine PUBLIC include)
target_link_libraries(autodj_engine PUBLIC autodj_granular autodj_analysis autodj_core)
