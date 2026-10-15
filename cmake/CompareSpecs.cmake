# ctest helper: the Kotlin tests and UI previews use a snapshot of the native parameter registry
# (app/src/test/resources/param_specs.json). This fails when the registry has changed without refreshing it.
execute_process(
  COMMAND ${CLI} --seconds 0.01 --out ${OUT}.wav --dump-params ${OUT}
  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "render_cli failed (${rc})")
endif()
file(READ ${OUT} actual)
file(READ ${SNAP} expected)
if(NOT actual STREQUAL expected)
  message(FATAL_ERROR
    "The native parameter registry changed. Refresh the snapshot:\n"
    "  ${CLI} --seconds 0.01 --out /tmp/x.wav --dump-params ${SNAP}")
endif()
