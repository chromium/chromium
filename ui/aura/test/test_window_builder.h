// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_WINDOW_BUILDER_H_
#define UI_AURA_TEST_TEST_WINDOW_BUILDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"

namespace aura::test {

struct WindowBuilderParams {
  raw_ptr<WindowDelegate> delegate = nullptr;
  raw_ptr<Window> parent = nullptr;
  gfx::Rect bounds;
  client::WindowType window_type = client::WINDOW_TYPE_NORMAL;
  ui::LayerType layer_type = ui::LAYER_TEXTURED;
  int window_id = Window::kInitialId;
  bool show = true;
};

// A builder to create a aura::Window for testing purpose. Use this when you
// simply need need a window without a meaningful content (except for a color)
// or capability to drag a window or drag to resize a window. If you need these
// properties in your test, use views::test::TestWidgetBuilder instead.
//
// The builder object can be used only once to create a single window because
// ownership of some parameters has to be transferred to a created window.
class TestWindowBuilder {
 public:
  explicit TestWindowBuilder(WindowBuilderParams params = {});
  TestWindowBuilder(TestWindowBuilder& other);
  TestWindowBuilder& operator=(TestWindowBuilder& params) = delete;
  ~TestWindowBuilder();

  // Sets parameters that are used when creating a test window.
  TestWindowBuilder& SetParent(Window* parent);
  TestWindowBuilder& SetWindowType(client::WindowType type);
  TestWindowBuilder& SetWindowId(int id);
  TestWindowBuilder& SetBounds(const gfx::Rect& bounds);

  // Having a non-empty title helps avoid accessibility paint check failures
  // in tests. For instance, `WindowMiniView` gets its accessible name from
  // the window title.
  TestWindowBuilder& SetWindowTitle(const std::u16string& title);

  // Set a WindowDelegate used by a test window.
  TestWindowBuilder& SetDelegate(WindowDelegate* delegate);

  // Use this to create a window whose content is painted with |color|.
  // This uses aura::test::ColorTestWindowDelegate as a WindowDelegate.
  TestWindowBuilder& SetColorWindowDelegate(SkColor color);

  // Use aura::test::TestWindowDelegate as a WindowDelegate.
  TestWindowBuilder& SetTestWindowDelegate();

  // Allows the window to be resizable, maximizable and minimizable.
  TestWindowBuilder& AllowAllWindowStates();

  // A window is shown when created by default. Use this if you want not
  // to show when created.
  TestWindowBuilder& SetShow(bool show);

  // Sets the window property to be set on a test window.
  template <typename T>
  TestWindowBuilder& SetWindowProperty(const ui::ClassProperty<T>* property,
                                       T value) {
    init_properties_.SetProperty(property, value);
    return *this;
  }

  // Build a window based on the parameter already set. This can be called only
  // once and the object cannot be used to create multiple windows.
  [[nodiscard]] virtual std::unique_ptr<Window> Build();

 protected:
  std::unique_ptr<Window> CreateWindowInternal();

  const WindowBuilderParams& params() const { return params_; }
  bool built() const { return built_; }

  // Subclass needs a write access to parent. Reset it after it is returned to
  // avoid dangling pointer.
  Window* release_parent() {
    auto* r = params_.parent.get();
    params_.parent = nullptr;
    return r;
  }

 private:
  WindowBuilderParams params_;
  std::u16string window_title_;
  ui::PropertyHandler init_properties_;
  bool built_ = false;
};

}  // namespace aura::test

#endif  // UI_AURA_TEST_TEST_WINDOW_BUILDER_H_
