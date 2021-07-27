// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_
#define TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_

namespace base {

template <typename T>
class raw_ptr {};

}  // namespace base

using base::raw_ptr;

#endif  // TOOLS_CLANG_PLUGINS_TESTS_BASE_MEMORY_RAW_PTR_H_
