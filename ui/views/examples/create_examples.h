// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_CREATE_EXAMPLES_H_
#define UI_VIEWS_EXAMPLES_CREATE_EXAMPLES_H_


#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
namespace examples {

// Creates the default set of examples.
ExampleVector VIEWS_EXAMPLES_EXPORT
CreateExamples(ExampleVector extra_examples = ExampleVector());

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_CREATE_EXAMPLES_H_
