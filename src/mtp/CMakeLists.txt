set(example_as_test_suite)
if(${ENABLE_EXAMPLES})
  set(example_as_test_suite
      test/mtp-test-suite.cc
  )
endif()

build_lib(
  LIBNAME mtp
  SOURCE_FILES
    model/logical-process.cc
    model/mtp-interface.cc
    model/multithreaded-simulator-impl.cc
  HEADER_FILES
    model/logical-process.h
    model/mtp-interface.h
    model/multithreaded-simulator-impl.h
  LIBRARIES_TO_LINK ${libcore}
                    ${libnetwork}
  TEST_SOURCES ${example_as_test_suite}
)
