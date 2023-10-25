// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/examples/create_examples.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/views_examples_export.h"

namespace views {
class Widget;

namespace examples {

VIEWS_EXAMPLES_EXPORT extern const char kExamplesWidgetName[];

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

// Prints |string| in the status area, at the bottom of the window.
VIEWS_EXAMPLES_EXPORT void LogStatus(const std::string& string);

// Same as LogStatus(), but with a format string.
template <typename... Args>
void PrintStatus(const char* format, Args... args) {
  LogStatus(base::StringPrintfNonConstexpr(format, args...));
}

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_H_
