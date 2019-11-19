// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MASKED_TARGETER_DELEGATE_H_
#define UI_VIEWS_MASKED_TARGETER_DELEGATE_H_

#include "base/macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/views_export.h"

class SkPath;

namespace gfx {
class Rect;
}

namespace views {
class View;

// Defines the default behaviour for hit-testing a rectangular region against
// the bounds of a View having a custom-shaped hit test mask. Views define
// such a mask by extending this class.
class VIEWS_EXPORT MaskedTargeterDelegate : public ViewTargeterDelegate {
 public:
  MaskedTargeterDelegate() = default;
  ~MaskedTargeterDelegate() override = default;

  // Sets the hit-test mask for the view which implements this interface,
  // in that view's local coordinate space. Returns whether a valid mask
  // has been set in |mask|.
  virtual bool GetHitTestMask(SkPath* mask) const = 0;

  // ViewTargeterDelegate:
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MaskedTargeterDelegate);
};

}  // namespace views

#endif  // UI_VIEWS_MASKED_TARGETER_DELEGATE_H_
