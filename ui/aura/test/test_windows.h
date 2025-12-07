// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOWS_H_
#define UI_AURA_TEST_TEST_WINDOWS_H_

#include <optional>
#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_builder.h"
#include "ui/aura/test/test_window_delegate.h"

namespace aura {
class Env;

namespace test {

// Sets the Env to use for creation of new Windows. If null, Env::GetInstance()
// is used.
void SetEnvForTestWindows(Env* env);
Env* GetEnvForTestWindows();

// Creates a test window. It internally uses TestWindowBuilder. If `color` is
// specified, it'll create a test delegate that fills the content with the given
// color.
[[nodiscard]] std::unique_ptr<Window> CreateTestWindow(
    WindowBuilderParams params = {},
    std::optional<SkColor> color = std::nullopt);

// Returns true if |upper| is above |lower| in the window stacking order.
bool WindowIsAbove(Window* upper, Window* lower);

// Returns true if |upper|'s layer is above |lower|'s layer in the layer
// stacking order.
bool LayerIsAbove(Window* upper, Window* lower);

// Returns a string containing the id of each of the child windows (bottommost
// first) of |parent|. The format of the string is "id1 id2 id...".
std::string ChildWindowIDsAsString(aura::Window* parent);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_WINDOWS_H_
