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
    core/test_source_location.cpp
    core/test_string_pool.cpp
    core/test_ast_arena.cpp
    core/test_tokens.cpp
    core/test_base_ast.cpp
    core/test_attribute_ast.cpp
    core/test_type_ast.cpp
    core/test_value_ast.cpp
    core/test_decl_ast.cpp
    core/test_module_ast.cpp
    core/test_trivia.cpp
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
    parser/test_lexer.cpp
    parser/test_token_stream.cpp
    parser/test_parser_context.cpp
    parser/test_grammar_positions.cpp
    parser/test_error_recovery.cpp
    parser/test_parse_type.cpp
    parser/test_parse_value.cpp
    parser/test_parse_node.cpp
    parser/test_parse_decl.cpp
    parser/test_parse_file.cpp
    parser/test_json_writer.cpp
    parser/test_json_dumper.cpp
    parser/test_fixtures.cpp
    parser/test_tokenize_with_trivia.cpp
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
    formatter/test_format_options.cpp
    formatter/test_indent_writer.cpp
    formatter/test_formatter_api.cpp
    formatter/test_layout_builder.cpp
    formatter/test_formatter_fixtures.cpp
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

# ─── lucid-fixture-regen ─────────────────────────────────────────────────
#
# Regenerates all fixture expected files:
#   - parser/good/*.lucid  → *.json   (parse + AST dump)
#   - formatter/canonical/*.lucid → *.expected  (parse + format)
#   - sema/*/input.lucid → expected.json (compile + graph dump)
#
# Also verifies formatter idempotence at generation time: if
# format(format(x)) != format(x), the tool exits non-zero.
#
# Run it with:
#
#   cmake --build build --target regen-fixtures
#
# This builds and runs the tool. It is not part of the default build.

add_executable(lucid-fixture-regen
    tools/fixture_regen.cpp
)

target_link_libraries(lucid-fixture-regen PRIVATE
    lucid_core
    lucid_parser
    lucid_formatter
    lucid_sema
)

target_compile_definitions(lucid-fixture-regen PRIVATE
    FIXTURE_DIR="${LUCID_FIXTURE_DIR}"
)

# A custom target that builds and runs the tool.
add_custom_target(regen-fixtures
    COMMAND lucid-fixture-regen
    DEPENDS lucid-fixture-regen
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Regenerating parser and formatter fixtures"
    VERBATIM
)

# ─── test_cli ──────────────────────────────────────────────────────────────

add_executable(test_cli
    cli/test_cli_options.cpp
    cli/test_format_command.cpp
    cli/test_check_command.cpp
)

target_link_libraries(test_cli PRIVATE
    lucid_cli
    Catch2::Catch2WithMain
)

target_compile_options(test_cli PRIVATE ${LUCID_CXX_WARNINGS})

add_test(NAME cli COMMAND test_cli)

# ─── test_sema ─────────────────────────────────────────────────────────────

add_executable(test_sema
    sema/test_node_kind.cpp
    sema/test_type_id.cpp
    sema/test_literal.cpp
    sema/test_registry.cpp
    sema/test_graph.cpp
    sema/test_sema_api.cpp
    sema/test_sema_smoke.cpp
    sema/test_symbol_table.cpp
    sema/test_symbol_collector.cpp
    sema/test_resolution_map.cpp
    sema/test_resolver.cpp
    sema/test_primitives.cpp
    sema/test_type_map.cpp
    sema/test_type_checker.cpp
    sema/test_constant_value_map.cpp
    sema/test_graph_builder.cpp
    sema/test_dead_code.cpp
    sema/test_import_resolver.cpp
    sema/test_multi_module.cpp
    sema/test_sema_fixtures.cpp
    sema/test_pipeline_smoke.cpp
)

target_link_libraries(test_sema PRIVATE
    lucid_sema
    Catch2::Catch2WithMain
)

target_include_directories(test_sema PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tools"
    "${CMAKE_CURRENT_SOURCE_DIR}/../sema/src"
)

target_compile_definitions(test_sema PRIVATE
    FIXTURE_DIR="${LUCID_FIXTURE_DIR}"
)

target_compile_options(test_sema PRIVATE ${LUCID_CXX_WARNINGS})

add_test(NAME sema COMMAND test_sema)

# ─── test_serialization ─────────────────────────────────────────────────────────────

add_executable(test_serialization
    serialization/test_serializer.cpp
    serialization/test_roundtrip.cpp
    serialization/test_deserialize_errors.cpp
)

target_link_libraries(test_serialization PRIVATE
    lucid_serialization
    Catch2::Catch2WithMain
)

target_compile_options(test_serialization PRIVATE ${LUCID_CXX_WARNINGS})

add_test(NAME serialization COMMAND test_serialization)