// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_widget_state.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/views/widget/widget.h"

namespace views::internal {

ElementTrackerWidgetState::ElementTrackerWidgetState(Delegate& delegate,
                                                     Widget& widget)
    : delegate_(&delegate), widget_(&widget) {
  observation_.Observe(widget_);
  visible_ = widget_->IsVisible();
  minimized_ = widget_->IsMinimized();
}

ElementTrackerWidgetState::~ElementTrackerWidgetState() = default;

void ElementTrackerWidgetState::OnWidgetVisibilityChanged(Widget* widget,
                                                          bool visible) {
  CancelPendingVisibilityChange();
  if (visible == visible_) {
    return;
  }

#if !BUILDFLAG(IS_WIN)
  // On all platforms but Windows, minimization comes with an automatic hide.
  if (!visible && widget == widget->GetPrimaryWindowWidget()) {
    // This happens *before* the minimized signal is sent, so delay processing
    // until the message queue is cleared.
    if (!minimized_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ElementTrackerWidgetState::CommitPendingVisibilityChange,
              commit_weak_ptr_factory_.GetWeakPtr()));
    }
    // If the window is minimized, transitions to not-visible need to be
    // ignored, since the window will be artificially not-visible in this state.
    return;
  }
#endif

  visible_ = visible;
  delegate_->OnWidgetVisibilityChanged(widget, visible);
}

void ElementTrackerWidgetState::OnWidgetShowStateChanged(Widget* widget) {
  CancelPendingVisibilityChange();
  minimized_ = widget->IsMinimized();
}

void ElementTrackerWidgetState::OnWidgetDestroying(Widget* widget) {
  CancelPendingVisibilityChange();
  Delegate* delegate = delegate_;
  delegate_ = nullptr;
  widget_ = nullptr;
  observation_.Reset();
  visible_ = false;

  // This has to be last since the current object could be destroyed during
  // the call.
  delegate->OnWidgetDestroying(widget);
}

void ElementTrackerWidgetState::CommitPendingVisibilityChange() {
  if (visible_) {
    visible_ = false;
    delegate_->OnWidgetVisibilityChanged(widget_, false);
  }
}

void ElementTrackerWidgetState::CancelPendingVisibilityChange() {
  commit_weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace views::internal
