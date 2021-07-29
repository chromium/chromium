// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test file simulates source files similar to //base/third_party.  Such
// files are part of Chromium repo (according to |git|) and therefore would be
// rewritten by the |apply_edits.py| tool.  OTOH, we don't want to rewrite such
// files:
// 1. We want to minimize the delta between the upstream third-party repo and
//    the Chromium copy
// 2. Such files very often limit themselves to C-only and CheckedPtr requires
//    C++11.  (OTOH, if needed this item might be more directly addressable by
//    looking at |extern "C"| context declarations, or by looking at the
//    language of the source code - as determined by
//    |clang_frontend_input_file.getKind().getLanguage()|).

class SomeClass;

struct MyStruct {
  // No rewrite expected, because the path of this source file contains
  // "third_party" substring.
  SomeClass* ptr_field;
};
