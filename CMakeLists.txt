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

# Executável principal
add_executable(scout src/main.cpp)
target_link_libraries(scout PRIVATE nlohmann_json::nlohmann_json)

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

# Testes de utils/filesystem.hpp
add_executable(test_filesystem tests/test_filesystem.cpp)
target_link_libraries(test_filesystem GTest::gtest_main)

# Testes de cli/parser.hpp
add_executable(test_cli_parser tests/test_cli_parser.cpp)
target_link_libraries(test_cli_parser GTest::gtest_main)

# Testes de core/scanner.hpp
add_executable(test_scanner tests/test_scanner.cpp)
target_link_libraries(test_scanner GTest::gtest_main)

# Testes de core/smali_parser.hpp
add_executable(test_smali_parser tests/test_smali_parser.cpp)
target_link_libraries(test_smali_parser GTest::gtest_main nlohmann_json::nlohmann_json)

# Testes de analysis/xref.hpp
add_executable(test_xref tests/test_xref.cpp)
target_link_libraries(test_xref GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_filesystem)
gtest_discover_tests(test_cli_parser)
gtest_discover_tests(test_scanner)
gtest_discover_tests(test_smali_parser)
gtest_discover_tests(test_xref)
