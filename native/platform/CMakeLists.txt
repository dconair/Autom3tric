# Android-only: Oboe audio driver + JNI bridge, packaged as libautodj_native.so.
# Oboe comes from Gradle (com.google.oboe:oboe, Prefab) - see app/build.gradle.kts.
find_package(oboe REQUIRED CONFIG)

add_library(autodj_native SHARED
  oboe/AudioDriver.cpp
  jni/NativeBridge.cpp)
target_include_directories(autodj_native PRIVATE oboe jni)
autodj_enable_rt_guard(autodj_native)
target_link_libraries(autodj_native PRIVATE autodj_engine autodj_bench oboe::oboe log android)
# Play requires 16 KB page alignment from Android 15 on (NDK r28+ does it by default; stated explicitly here).
target_link_options(autodj_native PRIVATE -Wl,-z,max-page-size=16384 -Wl,--gc-sections)
