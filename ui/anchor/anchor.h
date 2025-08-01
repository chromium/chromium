// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANCHOR_ANCHOR_H_
#define UI_ANCHOR_ANCHOR_H_

#include <memory>

#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace ui {

class AnchorImpl;

// ui::Anchor is a generic reference to a UI element that can be used to
// position other UI elements. It can be backed by a view or a DOM element in
// WebUI.
//
// This is designed to be transparently constructed from a views::View*, so that
// code that previously uses an anchor view can easily migrate to accept a WebUI
// anchor.
//
// This class assumes that the anchor is hosted in a views::Widget window.
class COMPONENT_EXPORT(UI_ANCHOR) Anchor {
 public:
  // Creates an empty anchor.
  Anchor();

  Anchor(const Anchor& other);
  Anchor(Anchor&& other) noexcept;
  Anchor& operator=(const Anchor& other);
  Anchor& operator=(Anchor&& other) noexcept;
  ~Anchor();

  // Deliberately implicit.
  // A ui::Anchor can be transparently constructed from a views::View*.
  // Your code must depend on //ui/views to use this constructor.
  // NOLINTNEXTLINE(google-explicit-constructor)
  Anchor(views::View* anchor_view);

  // Returns true if the anchor is backed by a views::View.
  bool IsView() const;
  views::View* GetView();

  // Returns the widget hosting the anchor.
  views::Widget* GetWidget();

  bool IsEmpty() const;
  explicit operator bool() const { return !IsEmpty(); }

  // Returns the bounds of the anchor in screen coordinates.
  gfx::Rect GetScreenBounds() const;

 protected:
  explicit Anchor(std::unique_ptr<AnchorImpl> impl);

 private:
  std::unique_ptr<AnchorImpl> impl_;
};

class AnchorImpl {
 public:
  virtual ~AnchorImpl() = default;
  virtual std::unique_ptr<AnchorImpl> Clone() const = 0;
  virtual bool IsEmpty() const = 0;
  virtual gfx::Rect GetScreenBounds() const = 0;
  virtual views::Widget* GetWidget() = 0;
  virtual bool IsView() const = 0;
  virtual views::View* GetView() = 0;
};

}  // namespace ui

#endif  // UI_ANCHOR_ANCHOR_H_
