// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cxx20/span.hpp"
#include "exceptions.hpp"
#include "instantiate.hpp"
#include "limits.hpp"
#include "module.hpp"
#include "types.hpp"
#include "value.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace fizzy
{
/// Execute a function from an instance.
///
/// @param  instance    The instance.
/// @param  func_idx    The function index. MUST be a valid index, otherwise undefined behaviour
///                     (including crash) happens.
/// @param  args        The pointer to the arguments. The number of items and their types must match
///                     the expected number of input parameters of the function, otherwise undefined
///                     behaviour (including crash) happens.
/// @param  depth       The call depth (indexing starts at 0). Can be left at the default setting.
/// @return             The result of the execution.
ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, const Value* args, int depth = 0) noexcept;
}  // namespace fizzy
