include(GoogleTest)

function(autodj_add_test name)
  add_executable(${name} ${ARGN})
  target_link_libraries(${name} PRIVATE autodj_engine GTest::gtest_main)
  target_compile_definitions(${name} PRIVATE AUTODJ_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden")
  autodj_enable_rt_guard(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES TIMEOUT 600)
endfunction()

autodj_add_test(test_core
  test_spsc_queue.cpp
  test_triple_buffer.cpp
  test_params.cpp
  test_transport.cpp
  test_rt_guard.cpp
  test_source_table.cpp)

autodj_add_test(test_granular
  test_envelopes.cpp
  test_grain_pool.cpp
  test_kernels.cpp
  test_scheduler.cpp
  test_adaptive.cpp
  test_granular_node.cpp
  test_instrument.cpp
  test_chaos_fx.cpp)

autodj_add_test(test_engine
  test_engine.cpp
  test_golden.cpp)

# Static check: nothing on the audio-thread code paths may use banned constructs.
add_test(NAME rt_lint COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/rt_lint.py ${CMAKE_SOURCE_DIR})
# The lint itself is tested: every rule must fire on a bad sample and stay quiet on the good ones.
add_test(NAME rt_lint_self_test COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/rt_lint.py --self-test)

# The Kotlin side tests against a snapshot of the registry; keep it in sync with the native definition.
if(TARGET render_cli)
  add_test(NAME param_specs_snapshot
    COMMAND ${CMAKE_COMMAND} -DCLI=$<TARGET_FILE:render_cli> -DSNAP=${CMAKE_SOURCE_DIR}/app/src/test/resources/param_specs.json
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/param_specs_actual.json -P ${CMAKE_SOURCE_DIR}/cmake/CompareSpecs.cmake)
endif()
