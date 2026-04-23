# CMake generated Testfile for 
# Source directory: /home/trindade/Projetos/scout++
# Build directory: /home/trindade/Projetos/scout++/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/home/trindade/Projetos/scout++/build/test_filesystem[1]_include.cmake")
include("/home/trindade/Projetos/scout++/build/test_cli_parser[1]_include.cmake")
include("/home/trindade/Projetos/scout++/build/test_scanner[1]_include.cmake")
include("/home/trindade/Projetos/scout++/build/test_smali_parser[1]_include.cmake")
include("/home/trindade/Projetos/scout++/build/test_xref[1]_include.cmake")
subdirs("_deps/nlohmann_json-build")
subdirs("_deps/googletest-build")
