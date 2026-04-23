cmake_minimum_required(VERSION 3.20)
project(scout_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# JSON library para output estruturado (Agent-First)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# Includes da biblioteca do Scout
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Biblioteca principal
add_library(scout_lib
    src/engines/class_search_engine.cpp
    src/engines/content_search_engine.cpp
    src/engines/xref_search_engine.cpp
    src/engines/resource_mapping_engine.cpp
    src/engines/cfg_engine.cpp
    src/formatters/sexpr_formatter.cpp

)
target_link_libraries(scout_lib PUBLIC nlohmann_json::nlohmann_json)

# Executável principal
add_executable(scout src/main.cpp)
target_link_libraries(scout PRIVATE scout_lib)

# ==========================================
# Configuração do Test-Driven Development (TDD)
# ==========================================
enable_testing()

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
  googlebenchmark
  URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# Testes de utils/filesystem.hpp
add_executable(test_filesystem tests/test_filesystem.cpp)
target_link_libraries(test_filesystem GTest::gtest_main)

# Testes de cli/parser.hpp
add_executable(test_cli_parser tests/test_cli_parser.cpp)
target_link_libraries(test_cli_parser GTest::gtest_main)

# Testes de core/scanner.hpp
add_executable(test_scanner tests/test_scanner.cpp)
target_link_libraries(test_scanner GTest::gtest_main scout_lib)

# Testes de core/smali_parser.hpp
add_executable(test_smali_parser tests/test_smali_parser.cpp)
target_link_libraries(test_smali_parser GTest::gtest_main nlohmann_json::nlohmann_json)

# Testes de analysis/xref.hpp (Now using XrefSearchEngine)
add_executable(test_xref tests/test_xref.cpp)
target_link_libraries(test_xref GTest::gtest_main scout_lib)

# Testes dos Motores de Busca
add_executable(test_class_search_engine tests/test_class_search_engine.cpp)
target_link_libraries(test_class_search_engine GTest::gtest_main scout_lib)

add_executable(test_content_search_engine tests/test_content_search_engine.cpp)
target_link_libraries(test_content_search_engine GTest::gtest_main scout_lib)

add_executable(test_xref_search_engine tests/test_xref_search_engine.cpp)
target_link_libraries(test_xref_search_engine GTest::gtest_main scout_lib)

add_executable(test_xref_advanced tests/test_xref_advanced.cpp)
target_link_libraries(test_xref_advanced GTest::gtest_main scout_lib)

# Testes de formatters/sexpr_formatter.hpp
add_executable(test_sexpr tests/test_sexpr.cpp)
target_link_libraries(test_sexpr GTest::gtest_main scout_lib)

add_executable(test_sexpr_edge tests/test_sexpr_edge.cpp)
target_link_libraries(test_sexpr_edge GTest::gtest_main scout_lib)

# Benchmarks de performance
add_executable(run_benchmarks tests/benchmark.cpp)
target_link_libraries(run_benchmarks benchmark::benchmark scout_lib)

include(GoogleTest)
gtest_discover_tests(test_filesystem)
gtest_discover_tests(test_cli_parser)
gtest_discover_tests(test_scanner)
gtest_discover_tests(test_smali_parser)
gtest_discover_tests(test_xref)
gtest_discover_tests(test_class_search_engine)
gtest_discover_tests(test_content_search_engine)
gtest_discover_tests(test_xref_search_engine)
gtest_discover_tests(test_xref_advanced)
gtest_discover_tests(test_sexpr)
gtest_discover_tests(test_sexpr_edge)

add_executable(test_new_engines tests/test_new_engines.cpp)
target_link_libraries(test_new_engines GTest::gtest_main scout_lib)
gtest_discover_tests(test_new_engines)
