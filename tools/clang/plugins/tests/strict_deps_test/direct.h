// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_STRICT_DEPS_TEST_DIRECT_H_
#define TOOLS_CLANG_PLUGINS_TESTS_STRICT_DEPS_TEST_DIRECT_H_

#include "impl_dep.h"           // OK (it should only verify the main module).
#include "undeclared_header.h"  // OK (it should only verify the main module)

#endif  // TOOLS_CLANG_PLUGINS_TESTS_STRICT_DEPS_TEST_DIRECT_H_
