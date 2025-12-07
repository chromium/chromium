// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_

#include <string_view>

#include "base/functional/callback_forward.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class Widget;

namespace examples {

inline constexpr char kExamplesWidgetName[] = "ExamplesWidget";

VIEWS_EXAMPLES_EXPORT bool CheckCommandLineUsage();

// Returns the current widget.
VIEWS_EXAMPLES_EXPORT Widget* GetExamplesWidget();

// Shows a window with the views examples in it. |extra_examples| contains any
// additional examples to add. |window_context| is used to determine where the
// window should be created (see |Widget::InitParams::context| for details).
VIEWS_EXAMPLES_EXPORT void ShowExamplesWindow(
    base::OnceClosure on_close,
    ExampleVector examples = CreateExamples(),
    gfx::NativeWindow window_context = gfx::NativeWindow());

// Prints `status` in the status area, at the bottom of the window.
VIEWS_EXAMPLES_EXPORT void PrintStatus(std::string_view status);

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
