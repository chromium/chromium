// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_ELEMENT_TRACKER_WIDGET_STATE_H_
#define UI_VIEWS_INTERACTION_ELEMENT_TRACKER_WIDGET_STATE_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views::internal {

class VIEWS_EXPORT ElementTrackerWidgetState : public WidgetObserver {
 public:
  // Observes events on the specified widget.
  class Delegate {
   public:
    virtual void OnWidgetVisibilityChanged(const Widget* widget,
                                           bool visible) = 0;
    virtual void OnWidgetDestroying(const Widget* widget) = 0;
  };

  ElementTrackerWidgetState(Delegate& delegate, Widget& widget);
  ElementTrackerWidgetState(const ElementTrackerWidgetState&) = delete;
  void operator=(const ElementTrackerWidgetState&) = delete;
  ~ElementTrackerWidgetState() override;

  bool visible() const { return visible_; }

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget, bool visible) override;
  void OnWidgetShowStateChanged(Widget* widget) override;
  void OnWidgetDestroying(Widget* widget) override;

  // Visible -> not visible transitions can happen prior to minimization, so
  // briefly hold off marking views as not visible when widget visibility
  // changes.
  void CommitPendingVisibilityChange();
  void CancelPendingVisibilityChange();

  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<Widget> widget_ = nullptr;
  bool minimized_ = false;
  bool visible_ = false;

  base::ScopedObservation<Widget, WidgetObserver> observation_{this};
  base::WeakPtrFactory<ElementTrackerWidgetState> commit_weak_ptr_factory_{
      this};
};

}  // namespace views::internal

#endif  // UI_VIEWS_INTERACTION_ELEMENT_TRACKER_WIDGET_STATE_H_
