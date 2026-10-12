# Decoded + analysed tracks (shared by the DJ section and the engine, hence a library of its own)
add_library(autodj_tracks STATIC src/Tracks.cpp)
target_include_directories(autodj_tracks PUBLIC include)
target_link_libraries(autodj_tracks PUBLIC autodj_analysis autodj_core)

add_library(autodj_engine STATIC
  src/Engine.cpp
  src/ToneNode.cpp
  src/MixExport.cpp)
target_include_directories(autodj_engine PUBLIC include)
target_link_libraries(autodj_engine PUBLIC autodj_granular autodj_dj autodj_tracks autodj_analysis autodj_core)
