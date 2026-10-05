/**
 * @file src/main.cpp
 *
 * @brief The `lucid-fmt` executable: a thin CLI around lucid_formatter.
 *
 * ─── Phase 0 ──────────────────────────────────────────────────────────────
 * This is a stub. It exists so the build has an executable target and the
 * link line is exercised end-to-end. The real argument parsing, file
 * reading, and formatter invocation land in Phase 7.
 *
 * The current behavior: print a version banner and exit 0. It links
 * against every library so a missing symbol in any of them surfaces now,
 * not in Phase 7.
 */

#include <cstdio>

int main(int /*argc*/, char** /*argv*/) {
    std::printf("lucid-fmt (skeleton)\n");
    return 0;
}