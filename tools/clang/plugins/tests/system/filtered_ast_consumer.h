// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_FILTERED_AST_CONSUMER_H_
#define TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_FILTERED_AST_CONSUMER_H_

// Argument expansion followed by rescanning FORWARD's body visits earlier
// expansion locations again, even without modules.
DECLARE_ENUM(OutOfOrder, FORWARD(VALUE()));

DECLARE_ENUM(InOrder, 0);

DECLARE_ENUM(Repeated, FORWARD(0));

// Expanding the argument records VALUE() before DECLARE_ENUM in the body.
// DECLARE_ENUM then has the earlier location of SYSTEM_DECLARE_ENUM(), the
// only non-system expansion in this declaration's source range.
#define SYSTEM_DECLARE_ENUM(name, value) DECLARE_ENUM(name, value)
SYSTEM_DECLARE_ENUM(InSystemMacro, VALUE());

// This declaration has no user-defined macro expansions and stays filtered.
enum class SystemOnly { kOne = 1, kMaxValue = 0 };

#endif  // TOOLS_CLANG_PLUGINS_TESTS_SYSTEM_FILTERED_AST_CONSUMER_H_
