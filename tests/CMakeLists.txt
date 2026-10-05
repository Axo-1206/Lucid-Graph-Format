# ─── Catch2 via FetchContent ───────────────────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
)

FetchContent_MakeAvailable(Catch2)

# ─── Fixture directory ─────────────────────────────────────────────────────

set(LUCID_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# ─── test_core ─────────────────────────────────────────────────────────────

add_executable(test_core
    core/test_core_smoke.cpp
)

target_link_libraries(test_core PRIVATE
    lucid_core
    Catch2::Catch2WithMain
)

target_compile_definitions(test_core PRIVATE
    FIXTURE_DIR="${LUCID_FIXTURE_DIR}"
)

add_test(NAME core COMMAND test_core)

# ─── test_parser ───────────────────────────────────────────────────────────

add_executable(test_parser
    parser/test_parser_smoke.cpp
)

target_link_libraries(test_parser PRIVATE
    lucid_core
    lucid_parser
    Catch2::Catch2WithMain
)

target_compile_definitions(test_parser PRIVATE
    FIXTURE_DIR="${LUCID_FIXTURE_DIR}"
)

add_test(NAME parser COMMAND test_parser)

# ─── test_formatter ────────────────────────────────────────────────────────

add_executable(test_formatter
    formatter/test_formatter_smoke.cpp
)

target_link_libraries(test_formatter PRIVATE
    lucid_core
    lucid_parser
    lucid_formatter
    Catch2::Catch2WithMain
)

target_compile_definitions(test_formatter PRIVATE
    FIXTURE_DIR="${LUCID_FIXTURE_DIR}"
)

add_test(NAME formatter COMMAND test_formatter)