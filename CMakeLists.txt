cmake_minimum_required(VERSION 3.13)
project(concurrent_lib VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)

get_directory_property(IS_SUBPROJECT PARENT_DIRECTORY)

option(CONCURRENT_TESTING "Enable build tests for concurrent lib" ON)
option(CONCURRENT_EXAMPLES "Enable build examples for concurrent lib" ON)

add_compile_options(
    -Werror

    -Wall

    -Wcast-align
    -Wcast-qual
    -Wconversion
    -Wenum-compare
    -Wfloat-equal
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wsign-promo
)

if(NOT CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

file(GLOB SOURCES src/*.cpp)
add_library(concurrent ${SOURCES})
target_include_directories(concurrent INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})
install(TARGETS concurrent EXPORT ConcurrentConfig)
install(EXPORT ConcurrentConfig DESTINATION lib/cmake/Concurrent)

if(NOT CONCURRENT_TESTING)
    message(STATUS "Concurrent tests is OFF")
elseif(IS_SUBPROJECT)
    message(STATUS "Concurrent tests is OFF (lib is subproject)")
else()
    add_subdirectory(test)
    if (TARGET concurrent_test)
        message(STATUS "Concurrent tests is ON")
    else()
        message(STATUS "Concurrent tests is OFF (GTest not found)")
    endif()
endif()