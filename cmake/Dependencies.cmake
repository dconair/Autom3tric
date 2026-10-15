# Third-party code is fetched at configure time (pinned tags). For offline builds point
# FETCHCONTENT_SOURCE_DIR_<NAME> at a local checkout, e.g. -DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=...
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# JSON for presets / parameter specs (control-thread code only, never on the audio thread).
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

if(AUTODJ_BUILD_TESTS)
  set(INSTALL_GTEST OFF CACHE INTERNAL "")
  set(gtest_force_shared_crt ON CACHE INTERNAL "")
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googletest)
endif()

# Planned for later milestones (kept here so the choice is recorded, not fetched yet):
#   pffft            marton78/pffft            v1.1.0   FFT for beat/key analysis (M3)
#   signalsmith      Signalsmith-Audio/signalsmith-stretch 1.4.0 (MIT) time-stretching (M4)
#   oboe             com.google.oboe:oboe via Gradle/Prefab (Android only)
