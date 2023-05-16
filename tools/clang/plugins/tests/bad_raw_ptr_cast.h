// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_PLUGINS_TESTS_BAD_RAW_PTR_CAST_H_
#define TOOLS_CLANG_PLUGINS_TESTS_BAD_RAW_PTR_CAST_H_

#include "base/memory/raw_ptr.h"

class BadRawPtr {
 public:
  void Check();

 private:
  raw_ptr<int> foo_;
};

#endif  // TOOLS_CLANG_PLUGINS_TESTS_BAD_RAW_PTR_CAST_H_