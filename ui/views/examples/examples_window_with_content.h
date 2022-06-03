// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_WITH_CONTENT_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_WITH_CONTENT_H_

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/examples/views_examples_with_content_export.h"

namespace content {
class BrowserContext;
}

namespace views {
namespace examples {

// Shows a window with the views examples in it.
VIEWS_EXAMPLES_WITH_CONTENT_EXPORT void ShowExamplesWindowWithContent(
    base::OnceClosure on_close,
    content::BrowserContext* browser_context,
    gfx::NativeWindow window_context);

}  // namespace examples
}  // namespace views

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_WINDOW_WITH_CONTENT_H_
