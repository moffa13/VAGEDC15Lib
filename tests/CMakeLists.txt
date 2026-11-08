include(FetchContent)
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # MSVC
FetchContent_MakeAvailable(googletest)



add_executable(
    tests
    test_config.cpp
    test_edc15file.cpp
    test_command_interpreter.cpp
)
target_link_libraries(tests PRIVATE vagedc15core GTest::gtest_main)

target_compile_definitions(tests PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")

include(GoogleTest)
gtest_discover_tests(tests)