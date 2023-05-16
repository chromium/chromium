// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_
#define TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_

namespace base {

template <typename T>
class raw_ptr {};

template <typename T>
class raw_ref {};

}  // namespace base

using base::raw_ptr;
using base::raw_ref;

#endif  // TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_
