cmake_minimum_required(VERSION 3.20)
project(SanttosVirtualCableCore LANGUAGES CXX)

enable_testing()

add_executable(ring_buffer_tests tests/ring_buffer_tests.cpp)
target_include_directories(ring_buffer_tests PRIVATE src/core)
target_compile_features(ring_buffer_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(ring_buffer_tests PRIVATE /W4 /WX)
else()
    target_compile_options(ring_buffer_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME ring_buffer_tests COMMAND ring_buffer_tests)

add_executable(audio_quality_tests tests/audio_quality_tests.cpp)
target_include_directories(audio_quality_tests PRIVATE src/core)
target_compile_features(audio_quality_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(audio_quality_tests PRIVATE /W4 /WX)
else()
    target_compile_options(audio_quality_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME audio_quality_tests COMMAND audio_quality_tests)

if(WIN32)
    add_executable(SanttosCableDiagnostics WIN32 app/SanttosCableDiagnostics.cpp)
    target_include_directories(SanttosCableDiagnostics PRIVATE src/core)
    target_compile_features(SanttosCableDiagnostics PRIVATE cxx_std_20)
    target_compile_definitions(SanttosCableDiagnostics PRIVATE UNICODE _UNICODE)

    if(MSVC)
        target_compile_options(SanttosCableDiagnostics PRIVATE /W4 /WX /permissive-)
    endif()
endif()
