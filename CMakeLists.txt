cmake_minimum_required(VERSION 3.16)
project(jsp_pso LANGUAGES CXX)

option(JSP_NATIVE "Enable native CPU optimizations" OFF)
option(JSP_BUILD_TESTS "Build tests" ON)
option(JSP_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(jsp_core
    src/instance.cpp
    src/schedule.cpp
    src/pso.cpp
)
target_include_directories(jsp_core PUBLIC src)
if(MSVC)
  target_compile_options(jsp_core PRIVATE /O2 /W4)
else()
  target_compile_options(jsp_core PRIVATE -O3 -Wall -Wextra -Wpedantic)
endif()
if(JSP_NATIVE AND NOT MSVC)
  target_compile_options(jsp_core PRIVATE -march=native)
endif()
if(JSP_SANITIZE AND NOT MSVC)
  target_compile_options(jsp_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(jsp_core PRIVATE -fsanitize=address,undefined)
endif()

add_executable(jsp_pso src/main.cpp)
target_link_libraries(jsp_pso PRIVATE jsp_core)
if(JSP_SANITIZE AND NOT MSVC)
  target_compile_options(jsp_pso PRIVATE -fsanitize=address,undefined)
  target_link_options(jsp_pso PRIVATE -fsanitize=address,undefined)
endif()

if(JSP_BUILD_TESTS)
  enable_testing()
  add_executable(jsp_tests tests/test_core.cpp)
  target_link_libraries(jsp_tests PRIVATE jsp_core)
  if(JSP_SANITIZE AND NOT MSVC)
    target_compile_options(jsp_tests PRIVATE -fsanitize=address,undefined)
    target_link_options(jsp_tests PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME jsp_tests COMMAND jsp_tests)
endif()
