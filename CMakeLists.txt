cmake_minimum_required(VERSION 3.20)
project(scout_cpp VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Flags de otimização para Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto")
set(CMAKE_CXX_FLAGS "-O3 -march=native -flto")

# Includes da biblioteca do Scout
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Google Test para testes unitários
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(googletest)

# Biblioteca principal
add_library(scout_lib
    src/engines/class_search/class_search_engine.cpp
    src/engines/content_search/content_search_engine.cpp
    src/engines/xref_search/xref_search_engine.cpp
    src/engines/resource_mapping/resource_mapping_engine.cpp
    src/engines/cfg/cfg_engine.cpp
    src/engines/manifest/manifest_engine.cpp
    src/engines/class_inspector/class_inspector_engine.cpp
    src/engines/ui_mapper/ui_mapper_engine.cpp
    src/engines/deobf/deobf_engine.cpp
    src/engines/variable_tracker/variable_tracker_engine.cpp
    src/engines/cfg/dominator_analyzer.cpp
    src/engines/smali_dump/smali_dump_engine.cpp
    src/engines/register_engines.cpp
    src/formatters/sexpr_formatter.cpp
    src/utils/progress_reporter.cpp
)

# Executável principal
add_executable(scout src/main.cpp)
target_link_libraries(scout PRIVATE scout_lib)

# Testes unitários
enable_testing()
add_subdirectory(tests)
