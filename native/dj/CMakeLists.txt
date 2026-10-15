# DJ section: decks (key-preserving time stretch), mixer, mix planner and plan player.
add_library(autodj_dj STATIC
  src/Deck.cpp
  src/DjNode.cpp
  src/DjParams.cpp
  src/MixPlan.cpp
  src/MixPlanner.cpp)
target_include_directories(autodj_dj PUBLIC include)
# vendored header-only Signalsmith Stretch (MIT): SYSTEM so that -Werror does not apply to third-party code
target_include_directories(autodj_dj SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/signalsmith)
target_link_libraries(autodj_dj PUBLIC autodj_tracks autodj_core autodj_analysis)
