cmake_minimum_required(VERSION 3.20)
project(NovaLang)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Warnings για καλύτερο debugging
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ── Shared library
add_library(nova_lib
    src/lexer.cpp
    src/parser.cpp
    src/analyzer.cpp
    src/vm.cpp
    src/codegen.cpp
)

# ── Test: Lexer
add_executable(test_lexer
    tests/test_lexer.cpp
)
target_link_libraries(test_lexer nova_lib)

# ── Test: Parser
add_executable(test_parser
    tests/test_parser.cpp
)
target_link_libraries(test_parser nova_lib)

# ── Test: Analyzer
add_executable(test_analyzer
    tests/test_analyzer.cpp
)
target_link_libraries(test_analyzer nova_lib)

# ── Test: CodeGen + VM
add_executable(test_codegen
    tests/test_codegen.cpp
)
target_link_libraries(test_codegen nova_lib)
