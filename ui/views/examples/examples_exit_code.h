// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_EXIT_CODE_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_EXIT_CODE_H_

namespace views::examples {

enum class ExamplesExitCode {
  // Comparison succeeded.
  kSucceeded = 0,
  // Screenshot image empty.
  kImageEmpty,
  // Comparison failed.
  kFailed,
  // No comparison attempted.
  kNone,
};

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_EXIT_CODE_H_
