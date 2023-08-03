// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_TARGETER_H_
#define UI_VIEWS_VIEW_TARGETER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_targeter.h"
#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class GestureEvent;
class KeyEvent;
class ScrollEvent;
}  // namespace ui

namespace views {

class View;
class ViewTargeterDelegate;

// A ViewTargeter is installed on a View that wishes to use the custom
// hit-testing or event-targeting behaviour defined by `delegate`.
class VIEWS_EXPORT ViewTargeter : public ui::EventTargeter {
 public:
  // The delegate may, but does not have to, be owned by `ViewTargeter`.
  explicit ViewTargeter(ViewTargeterDelegate* delegate);
  explicit ViewTargeter(std::unique_ptr<ViewTargeterDelegate> delegate);

  ViewTargeter(const ViewTargeter&) = delete;
  ViewTargeter& operator=(const ViewTargeter&) = delete;

  ~ViewTargeter() override;

  // A call-through to DoesIntersectRect() on `delegate_`.
  bool DoesIntersectRect(const View* target, const gfx::Rect& rect) const;

  // A call-through to TargetForRect() on `delegate_`.
  View* TargetForRect(View* root, const gfx::Rect& rect) const;

 protected:
  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindNextBestTarget(ui::EventTarget* previous_target,
                                      ui::Event* event) override;

 private:
  View* FindTargetForKeyEvent(View* root, const ui::KeyEvent& key);
  View* FindTargetForScrollEvent(View* root, const ui::ScrollEvent& scroll);

  virtual View* FindTargetForGestureEvent(View* root,
                                          const ui::GestureEvent& gesture);
  virtual ui::EventTarget* FindNextBestTargetForGestureEvent(
      ui::EventTarget* previous_target,
      const ui::GestureEvent& gesture);

  // ViewTargeter may or may not own `delegate_`; if it doesn't, `delegate_`
  // must outlive it.
  std::unique_ptr<ViewTargeterDelegate> owned_delegate_;
  raw_ptr<ViewTargeterDelegate> delegate_;
};

}  // namespace views

#endif  // UI_VIEWS_VIEW_TARGETER_H_
