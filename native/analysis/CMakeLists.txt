add_library(autodj_analysis STATIC
  src/Analyzer.cpp
  src/PeakPyramid.cpp)
target_include_directories(autodj_analysis PUBLIC include)
target_link_libraries(autodj_analysis PUBLIC autodj_core)
