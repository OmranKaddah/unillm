cmake_minimum_required(VERSION 3.28)

project(
  unillm
  VERSION 0.1.0
  DESCRIPTION "Unified C++23 SDK and local proxy for multiple LLM providers"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UNILLM_ENABLE_MODULES "Build C++23 module interface units when supported" OFF)
option(UNILLM_BUILD_TESTS "Build unit and integration tests" ON)

find_package(CURL REQUIRED)

if(UNILLM_ENABLE_MODULES)
  if(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
    message(FATAL_ERROR "UNILLM_ENABLE_MODULES requires a generator with C++ module support, such as Ninja.")
  endif()
endif()

add_library(unillm_core
  src/core/unillm.cpp
  src/core/curl_transport.cpp
  src/core/providers.cpp
)

target_include_directories(unillm_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(unillm_core
  PUBLIC
    CURL::libcurl
)

target_compile_features(unillm_core PUBLIC cxx_std_23)

if(UNILLM_ENABLE_MODULES)
  target_sources(unillm_core
    PUBLIC
      FILE_SET cxx_modules TYPE CXX_MODULES FILES
        modules/unillm.cppm
        modules/unillm.providers.cppm
        modules/unillm.proxy.cppm
  )
  target_compile_definitions(unillm_core PUBLIC UNILLM_HAS_MODULES=1)
endif()

add_library(unillm_proxy_lib
  src/proxy/proxy.cpp
)

target_include_directories(unillm_proxy_lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(unillm_proxy_lib
  PUBLIC
    unillm_core
)

add_executable(unillm_proxy
  src/proxy/main.cpp
)

target_link_libraries(unillm_proxy
  PRIVATE
    unillm_proxy_lib
)

add_executable(unillm_example
  examples/basic.cpp
)

target_link_libraries(unillm_example
  PRIVATE
    unillm_core
)

add_executable(unillm_nim_contact_example
  examples/nvidia_nim_contact_parser.cpp
)

target_link_libraries(unillm_nim_contact_example
  PRIVATE
    unillm_core
)

if(UNILLM_BUILD_TESTS)
  enable_testing()

  add_executable(unillm_core_tests
    tests/unit/test_core.cpp
  )

  target_include_directories(unillm_core_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(unillm_core_tests
    PRIVATE
      unillm_core
  )

  add_test(NAME unillm_core_tests COMMAND unillm_core_tests)

  add_executable(unillm_proxy_tests
    tests/integration/test_proxy.cpp
  )

  target_include_directories(unillm_proxy_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
  )

  target_link_libraries(unillm_proxy_tests
    PRIVATE
      unillm_proxy_lib
  )

  add_test(NAME unillm_proxy_tests COMMAND unillm_proxy_tests)
endif()
