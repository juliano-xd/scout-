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
    src/engines/manifest_engine.cpp
    src/engines/class_inspector_engine.cpp
    src/engines/ui_mapper_engine.cpp
    src/engines/deobf_engine.cpp
    src/engines/variable_tracker_engine.cpp
    src/engines/dominator_analyzer.cpp
    src/engines/smali_dump_engine.cpp
    src/engines/register_engines.cpp
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

# Massive Test Suites (100 tests per module)
add_executable(test_manifest_massive tests/test_manifest_massive.cpp)
target_link_libraries(test_manifest_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_manifest_massive)

add_executable(test_class_inspector_massive tests/test_class_inspector_massive.cpp)
target_link_libraries(test_class_inspector_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_class_inspector_massive)

add_executable(test_deobf_massive tests/test_deobf_massive.cpp)
target_link_libraries(test_deobf_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_deobf_massive)

add_executable(test_xref_massive tests/test_xref_massive.cpp)
target_link_libraries(test_xref_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_xref_massive)

add_executable(test_ui_mapper_massive tests/test_ui_mapper_massive.cpp)
target_link_libraries(test_ui_mapper_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_ui_mapper_massive)

add_executable(test_search_massive tests/test_search_massive.cpp)
target_link_libraries(test_search_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_search_massive)

add_executable(test_res_map_massive tests/test_res_map_massive.cpp)
target_link_libraries(test_res_map_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_res_map_massive)

add_executable(test_cfg_massive tests/test_cfg_massive.cpp)
target_link_libraries(test_cfg_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_cfg_massive)

add_executable(test_sexpr_massive tests/test_sexpr_massive.cpp)
target_link_libraries(test_sexpr_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_sexpr_massive)

add_executable(test_cli_massive tests/test_cli_massive.cpp)
target_link_libraries(test_cli_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_cli_massive)

add_executable(test_class_search_massive tests/test_class_search_massive.cpp)
target_link_libraries(test_class_search_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_class_search_massive)

add_executable(test_batch_massive tests/test_batch_massive.cpp)
target_link_libraries(test_batch_massive GTest::gtest_main scout_lib)
gtest_discover_tests(test_batch_massive)

add_executable(test_brain_massive tests/test_brain_massive.cpp)
target_link_libraries(test_brain_massive PRIVATE scout_lib GTest::gtest_main)

add_executable(test_variable_tracker tests/test_variable_tracker.cpp)
target_link_libraries(test_variable_tracker PRIVATE scout_lib GTest::gtest_main)

add_executable(test_variable_tracker_massive tests/test_variable_tracker_massive.cpp)
target_link_libraries(test_variable_tracker_massive PRIVATE scout_lib GTest::gtest_main)

add_executable(test_level16_forensics tests/test_level16_forensics.cpp)
target_link_libraries(test_level16_forensics scout_lib GTest::gtest_main)
add_test(NAME Level16ForensicsTest COMMAND test_level16_forensics)

gtest_discover_tests(test_brain_massive)
gtest_discover_tests(test_variable_tracker)
gtest_discover_tests(test_variable_tracker_massive)
gtest_discover_tests(test_level16_forensics)

# ==========================================
# Bug Hunting & Audit Tests
# ==========================================
add_executable(test_smali_dump tests/test_smali_dump.cpp)
target_link_libraries(test_smali_dump GTest::gtest_main scout_lib)
gtest_discover_tests(test_smali_dump)

add_executable(test_bug_manifest_fragile tests/audit/test_bug_manifest_fragile.cpp)
target_link_libraries(test_bug_manifest_fragile scout_lib GTest::gtest_main)
gtest_discover_tests(test_bug_manifest_fragile)

add_executable(test_chaos_edge tests/audit/test_chaos_edge.cpp)
target_link_libraries(test_chaos_edge scout_lib GTest::gtest_main)
gtest_discover_tests(test_chaos_edge)

# Novas baterias de testes para Utils e Core
add_executable(test_mmap_file tests/test_mmap_file.cpp)
target_link_libraries(test_mmap_file GTest::gtest_main scout_lib)
gtest_discover_tests(test_mmap_file)

add_executable(test_string_utils tests/audit/test_string_utils.cpp)
target_link_libraries(test_string_utils GTest::gtest_main scout_lib)
gtest_discover_tests(test_string_utils)

add_executable(test_analysis_context tests/audit/test_analysis_context.cpp)
target_link_libraries(test_analysis_context GTest::gtest_main scout_lib)
gtest_discover_tests(test_analysis_context)
add_executable(test_cfg_enhanced tests/test_cfg_enhanced.cpp)
target_link_libraries(test_cfg_enhanced GTest::gtest_main scout_lib)
add_executable(test_obfuscation_engine tests/test_obfuscation_engine.cpp)
target_link_libraries(test_obfuscation_engine GTest::gtest_main scout_lib)
add_executable(test_constant_search tests/test_constant_search.cpp)
target_link_libraries(test_constant_search GTest::gtest_main scout_lib)
add_executable(test_data_flow tests/test_data_flow.cpp)
target_link_libraries(test_data_flow GTest::gtest_main scout_lib)
