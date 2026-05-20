cmake_minimum_required(VERSION 3.20)
project(crdtpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

include(FetchContent)

FetchContent_Declare(
        asio
        GIT_REPOSITORY http://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-30-2
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(asio)

FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.6.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

add_library(crdtpp_lib INTERFACE)
target_include_directories(crdtpp_lib INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${asio_SOURCE_DIR}/asio/include
)

target_link_libraries(crdtpp_lib INTERFACE
    nlohmann_json::nlohmann_json
)

target_compile_definitions(crdtpp_lib INTERFACE
    ASIO_STANDALONE
)

find_package(Threads REQUIRED)
target_link_libraries(crdtpp_lib INTERFACE Threads::Threads)

add_executable(editor_demo
        demo/editor_demo.cpp
        src/node.cpp
)
target_link_libraries(editor_demo PRIVATE crdtpp_lib)

enable_testing()

add_executable(crdt_tests
    tests/crdt_tests.cpp
)
target_include_directories(crdt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(crdt_tests PRIVATE crdtpp_lib Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(crdt_tests)
















