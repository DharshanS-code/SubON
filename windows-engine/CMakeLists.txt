cmake_minimum_required(VERSION 3.20)
project(SubOnWindowsEngine VERSION 0.1.0 LANGUAGES CXX)

option(SUBON_BUILD_TESTS "Build native DSP smoke tests" ON)

add_library(subon_dsp STATIC
    src/SubOnDsp.cpp
)
target_include_directories(subon_dsp PUBLIC include)
target_compile_features(subon_dsp PUBLIC cxx_std_17)
set_target_properties(subon_dsp PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
    target_compile_options(subon_dsp PRIVATE /W4 /permissive-)
else()
    target_compile_options(subon_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(SUBON_BUILD_TESTS)
    enable_testing()
    add_executable(subon_dsp_smoke_test tests/smoke_test.cpp)
    target_link_libraries(subon_dsp_smoke_test PRIVATE subon_dsp)
    add_test(NAME subon_dsp_smoke_test COMMAND subon_dsp_smoke_test)
endif()
