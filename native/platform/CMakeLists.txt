# Platform layer: audio driver + JNI bridge, packaged as the shared library the JVM loads (libautodj_native).
#   Android: Oboe driver (Oboe comes from Gradle, com.google.oboe:oboe via Prefab - see app/build.gradle.kts)
#   macOS:   CoreAudio driver, used by the desktop app (desktopApp/); enable with -DAUTODJ_BUILD_JNI=ON
if(ANDROID)
  find_package(oboe REQUIRED CONFIG)

  add_library(autodj_native SHARED
    oboe/AudioDriver.cpp
    jni/NativeBridge.cpp)
  target_include_directories(autodj_native PRIVATE oboe jni)
  autodj_enable_rt_guard(autodj_native)
  target_link_libraries(autodj_native PRIVATE autodj_engine autodj_bench oboe::oboe log android)
  # Play requires 16 KB page alignment from Android 15 on (NDK r28+ does it by default; stated explicitly here).
  target_link_options(autodj_native PRIVATE -Wl,-z,max-page-size=16384 -Wl,--gc-sections)
elseif(APPLE)
  find_package(JNI REQUIRED)

  add_library(autodj_native SHARED
    coreaudio/AudioDriver.cpp
    jni/NativeBridge.cpp)
  target_include_directories(autodj_native PRIVATE coreaudio jni ${JNI_INCLUDE_DIRS})
  autodj_enable_rt_guard(autodj_native)
  target_link_libraries(autodj_native PRIVATE autodj_engine autodj_bench
    "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation")
  # The JVM looks for libautodj_native.dylib (System.load / java.library.path).
  set_target_properties(autodj_native PROPERTIES OUTPUT_NAME autodj_native)
else()
  message(FATAL_ERROR "AUTODJ_BUILD_JNI is only implemented for Android and macOS")
endif()
