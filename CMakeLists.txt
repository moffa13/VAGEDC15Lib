cmake_minimum_required(VERSION 3.20)
project(VAGEDC15Logger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VAG_ASAN "Enable MSVC AddressSanitizer" OFF)
if(VAG_ASAN AND MSVC)
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()


add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>

    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wpedantic>
)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_subdirectory(tests)
endif()
