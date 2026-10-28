# Common compile options for every native target.
add_library(autodj_options INTERFACE)
target_compile_features(autodj_options INTERFACE cxx_std_17)
target_compile_options(autodj_options INTERFACE
  -Wall -Wextra -Wshadow -Wno-unused-parameter
  # Reproducible float results across compilers/architectures: no implicit FMA contraction in scalar code.
  # (The NEON kernels use explicit fused multiply-add intrinsics.)
  -ffp-contract=off -fno-math-errno -fno-trapping-math
  # An unoptimised audio callback underruns; keep Debug at -O2 (Gradle's default native Debug is -O0).
  $<$<CONFIG:Debug>:-O2>)
if(AUTODJ_WERROR)
  target_compile_options(autodj_options INTERFACE -Werror)
endif()
if(AUTODJ_RT_GUARD)
  target_compile_definitions(autodj_options INTERFACE AUTODJ_RT_GUARD=1)
endif()
if(AUTODJ_FORCE_SCALAR)
  target_compile_definitions(autodj_options INTERFACE AUTODJ_FORCE_SCALAR=1)
endif()
if(AUTODJ_SANITIZE)
  target_compile_options(autodj_options INTERFACE -fsanitize=${AUTODJ_SANITIZE} -fno-omit-frame-pointer -g)
  target_link_options(autodj_options INTERFACE -fsanitize=${AUTODJ_SANITIZE})
endif()

# Links the global operator new/delete replacement into an executable or shared library.
function(autodj_enable_rt_guard target)
  if(AUTODJ_RT_GUARD)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:autodj_rt_alloc_guard>)
  endif()
endfunction()
