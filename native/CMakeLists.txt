add_subdirectory(core)
add_subdirectory(fx)
add_subdirectory(granular)
add_subdirectory(engine)

if(ANDROID OR AUTODJ_BUILD_JNI)
  add_subdirectory(platform)
endif()

if(AUTODJ_BUILD_BENCH OR ANDROID OR AUTODJ_BUILD_JNI)
  add_subdirectory(bench)
endif()

if(AUTODJ_BUILD_BENCH)
  add_subdirectory(tools)
endif()

if(AUTODJ_BUILD_TESTS)
  add_subdirectory(tests)
endif()
