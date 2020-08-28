project(concurrent_test)

find_package(GTest 1.10.0 REQUIRED)

if (GTest_FOUND)
    file(GLOB TEST_SOURCES src/*.cpp)
    add_executable(concurrent_test ${TEST_SOURCES})

    # Disable for GTest build
    target_compile_options(concurrent_test PRIVATE -Wno-sign-compare)

    target_include_directories(concurrent_test PUBLIC include)
    target_link_libraries(concurrent_test GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock concurrent)
    add_test(test-1 concurrent_test)

    add_custom_target(check ALL COMMAND concurrent_test)
endif()
