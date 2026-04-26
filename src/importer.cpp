#include "importer.h"
#include "lexer.h"
#include "parser.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;

// ── Internal recursive collector ──────────────────────────────────────────────
//
// Walks `prog->statements` in order. Non-import nodes are moved into `out`.
// Import nodes trigger a recursive load: the imported file's nodes are
// inserted into `out` before the current file continues.
//
// done    — canonical paths already fully inlined (deduplication)
// active  — canonical paths currently on the call stack (cycle detection)

static void collect(
    Program*                 prog,
    const fs::path&          dir,
    std::set<std::string>&   done,
    std::set<std::string>&   active,
    NodeList&                out)
{
    for (auto& node : prog->statements) {
        if (node->kind != NodeKind::ImportStmt) {
            out.push_back(std::move(node));
            continue;
        }

        auto* imp     = static_cast<ImportStmt*>(node.get());
        fs::path full = (dir / imp->path).lexically_normal();
        std::string key = full.string();

        if (active.count(key))
            throw std::runtime_error(
                "circular import detected: '" + imp->path + "'");

        if (done.count(key))
            continue;   // already inlined — skip duplicate

        done.insert(key);

        std::ifstream f(full);
        if (!f)
            throw std::runtime_error(
                "cannot open imported file '" + imp->path + "'");

        std::string src((std::istreambuf_iterator<char>(f)), {});

        Lexer  lexer(src);
        Parser parser(lexer.tokenize());
        auto   sub = parser.parse();

        if (parser.hasErrors()) {
            std::string msg = "parse errors in '" + imp->path + "':\n";
            for (const auto& e : parser.errors())
                msg += "  line " + std::to_string(e.line) + ": " + e.message + "\n";
            throw std::runtime_error(msg);
        }

        active.insert(key);
        collect(sub.get(), full.parent_path(), done, active, out);
        active.erase(key);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

std::unique_ptr<Program> resolveImports(
    std::unique_ptr<Program> program,
    const std::string&       baseDir)
{
    fs::path dir = baseDir.empty() ? fs::current_path()
                                   : fs::path(baseDir).lexically_normal();

    std::set<std::string> done;
    std::set<std::string> active;
    NodeList out;

    collect(program.get(), dir, done, active, out);

    auto merged       = std::make_unique<Program>();
    merged->statements = std::move(out);
    return merged;
}
