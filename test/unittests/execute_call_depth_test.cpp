// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

/*
(I would list the possible cases as:
- call internal function
- call imported wasm function
- call imported host function
- Internal function - imported host function - wasm function sandwich)
 */

/// Executing at DepthLimit call stack depth immediately traps.
/// E.g. to create "space" for n calls use `DepthLimit - n`.
constexpr auto DepthLimit = 2048;
static_assert(DepthLimit == CallStackLimit);

TEST(execute_call_depth, execute_internal_function)
{
    /* wat2wasm
    (func (result i32) (i32.const 1))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040041010b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
}

TEST(execute_call_depth, execute_imported_wasm_function)
{
    /* wat2wasm
    (func (export "f") (result i32) (i32.const 1))
    */
    const auto exported_wasm =
        from_hex("0061736d010000000105016000017f03020100070501016600000a0601040041010b");

    /* wat2wasm
    (func (import "exporter" "f") (result i32))
    */
    const auto executor_wasm =
        from_hex("0061736d010000000105016000017f020e01086578706f7274657201660000");

    auto exporter = instantiate(parse(exported_wasm));
    auto executor = instantiate(parse(executor_wasm), {*find_exported_function(*exporter, "f")});
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit), Traps());
}

TEST(execute_call_depth, execute_imported_host_function)
{
    /* wat2wasm
    (func (import "host" "f") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020a0104686f737401660000");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance&, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return ExecutionResult{Value{1_u32}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}), Result(1));
    EXPECT_EQ(recorded_depth, 0);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Result(1));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, call_internal_function)
{
    /* wat2wasm
    (func $internal (result i32) (i32.const 1))
    (func (result i32) (call $internal))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0b02040041010b040010000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
}

TEST(execute_call_depth, call_imported_wasm_function)
{
    /* wat2wasm
    (func (export "f") (result i32) (i32.const 1))
    */
    const auto exported_wasm =
        from_hex("0061736d010000000105016000017f03020100070501016600000a0601040041010b");

    /* wat2wasm
    (func $exporter_f (import "exporter" "f") (result i32))
    (func (result i32) (call $exporter_f))
    */
    const auto executor_wasm = from_hex(
        "0061736d010000000105016000017f020e01086578706f7274657201660000030201000a0601040010000b");

    auto exporter = instantiate(parse(exported_wasm));
    auto executor = instantiate(parse(executor_wasm), {*find_exported_function(*exporter, "f")});
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 1), Traps());
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit), Traps());
}

TEST(execute_call_depth, call_imported_host_function)
{
    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040010000b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance&, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return ExecutionResult{Value{1_u32}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Result(1));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, execute_internal_infinite_recursion_function)
{
    // This execution must always trap.

    /* wat2wasm
    (func $f (call $f))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0601040010000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
}
