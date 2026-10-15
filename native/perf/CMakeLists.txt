# Performance section: master effects (echo, reverb, flanger, beat roll, stutter) and the drum pads with their step sequencer.
add_library(autodj_perf STATIC
  src/PerfParams.cpp
  src/MasterFx.cpp
  src/PadKit.cpp
  src/PadNode.cpp)
target_include_directories(autodj_perf PUBLIC include)
target_link_libraries(autodj_perf PUBLIC autodj_fx autodj_core)
