#include "lexer.h"
#include "parser.h"
#include "analyzer.h"
#include "codegen.h"
#include "vm.h"
#include "error.h"
#include "importer.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

// ── Helpers ────────────────────────────────────────────────────────────────────

static void printUsage(const char* argv0) {
    std::cerr << "Usage: nova <file.nova>\n"
              << "\n"
              << "Examples:\n"
              << "  nova hello.nova\n"
              << "  nova programs/factorial.nova\n";
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "nova: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

// ── Main ───────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string path   = argv[1];
    const std::string source = readFile(path);

    // ── Phase 1: Lexer ─────────────────────────────────────────────────────────
    std::vector<Token> tokens;
    try {
        Lexer lexer(source);
        tokens = lexer.tokenize();
    } catch (const std::exception& e) {
        std::cerr << path << ": lexer error: " << e.what() << "\n";
        return 1;
    }

    // ── Phase 2: Parser ────────────────────────────────────────────────────────
    Parser                   parser(std::move(tokens));
    std::unique_ptr<Program> program = parser.parse();
    if (parser.hasErrors()) {
        renderErrors(path, parser.errors(), source);
        return 1;
    }

    // ── Phase 2b: Import Resolution ────────────────────────────────────────────
    try {
        std::string dir = std::filesystem::path(path).parent_path().string();
        program = resolveImports(std::move(program), dir);
    } catch (const std::exception& e) {
        std::cerr << path << ": import error: " << e.what() << "\n";
        return 1;
    }

    // ── Phase 3: Semantic Analyzer ─────────────────────────────────────────────
    Analyzer analyzer;
    if (!analyzer.analyze(program.get())) {
        renderErrors(path, analyzer.errors(), source);
        return 1;
    }

    // ── Phase 4 + 5: Code Generation + VM ─────────────────────────────────────
    try {
        CodeGen codegen;
        VM      vm(codegen.generate(program.get()));
        vm.run();
    } catch (const std::exception& e) {
        std::cerr << path << ": runtime error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
