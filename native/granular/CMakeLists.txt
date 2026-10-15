add_library(autodj_granular STATIC
  src/GranularParams.cpp
  src/EnvelopeTables.cpp
  src/GrainKernels.cpp
  src/GranularNode.cpp)
target_include_directories(autodj_granular PUBLIC include)
target_link_libraries(autodj_granular PUBLIC autodj_core autodj_fx)
