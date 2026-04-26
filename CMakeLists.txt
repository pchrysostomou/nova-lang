cmake_minimum_required(VERSION 3.20)
project(NovaLang)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings για καλύτερο debugging
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ── Shared library (lexer — θα προσθέτουμε εδώ parser, interpreter αργότερα)
add_library(nova_lib
    src/lexer.cpp
)

# ── Test: Lexer
add_executable(test_lexer
    tests/test_lexer.cpp
)
target_link_libraries(test_lexer nova_lib)
