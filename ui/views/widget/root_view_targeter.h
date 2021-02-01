// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_ROOT_VIEW_TARGETER_H_
#define UI_VIEWS_WIDGET_ROOT_VIEW_TARGETER_H_

#include "base/macros.h"
#include "ui/views/view_targeter.h"
#include "ui/views/views_export.h"

namespace views {

namespace internal {
class RootView;
}  // namespace internal

class View;
class ViewTargeterDelegate;

// A derived class of ViewTargeter that defines targeting logic for cases
// needing to access the members of RootView. For example, when determining the
// target of a gesture event, we need to know if a previous gesture has already
// established the View to which all subsequent gestures should be targeted.
class VIEWS_EXPORT RootViewTargeter : public ViewTargeter {
 public:
  RootViewTargeter(ViewTargeterDelegate* delegate,
                   internal::RootView* root_view);
  ~RootViewTargeter() override;

 private:
  // ViewTargeter:
  View* FindTargetForGestureEvent(View* root,
                                  const ui::GestureEvent& gesture) override;
  ui::EventTarget* FindNextBestTargetForGestureEvent(
      ui::EventTarget* previous_target,
      const ui::GestureEvent& gesture) override;

  // A pointer to the RootView on which |this| is installed.
  internal::RootView* root_view_;

  DISALLOW_COPY_AND_ASSIGN(RootViewTargeter);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_ROOT_VIEW_TARGETER_H_
