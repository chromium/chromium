// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_PROC_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_PROC_H_

#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_exit_code.h"

namespace views::examples {

ExamplesExitCode ExamplesMainProc(bool under_test = false,
                                  ExampleVector examples = ExampleVector());

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_MAIN_PROC_H_
