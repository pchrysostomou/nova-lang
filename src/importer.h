#pragma once
#include "ast.h"
#include <memory>
#include <string>

// Resolves all ImportStmt nodes in `program` depth-first, loading and
// inlining each imported file's declarations in place of the import.
//
// baseDir: directory used to resolve relative import paths.
//          Pass the directory of the main source file.
//
// Guarantees:
//   - Each file is inlined at most once (deduplication).
//   - Circular imports are detected and reported as std::runtime_error.
//   - Missing files are reported as std::runtime_error.
//
// On success, returns a new Program with all ImportStmt nodes replaced
// by the inlined declarations. The returned program is ready for analysis.
std::unique_ptr<Program> resolveImports(
    std::unique_ptr<Program> program,
    const std::string& baseDir);
