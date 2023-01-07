// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_RESIZE_AREA_DELEGATE_H_
#define UI_VIEWS_CONTROLS_RESIZE_AREA_DELEGATE_H_

namespace views {

// An interface implemented by objects that want to be notified about the resize
// event.
class ResizeAreaDelegate {
 public:
  // OnResize is sent when resizing is detected. |resize_amount| specifies the
  // number of pixels that the user wants to resize by, and can be negative or
  // positive (depending on direction of dragging and flips according to
  // locale directionality: dragging to the left in LTR locales gives negative
  // |resize_amount| but positive amount for RTL). |done_resizing| is true if
  // the user has released the pointer (mouse, stylus, touch, etc.).
  virtual void OnResize(int resize_amount, bool done_resizing) = 0;

 protected:
  virtual ~ResizeAreaDelegate() = default;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_RESIZE_AREA_DELEGATE_H_
