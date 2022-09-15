// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_utils.h"

#include <utility>

#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views {

WidgetOpenTimer::WidgetOpenTimer(Callback callback)
    : callback_(std::move(callback)) {}

WidgetOpenTimer::~WidgetOpenTimer() = default;

void WidgetOpenTimer::OnWidgetDestroying(Widget* widget) {
  DCHECK(open_timer_.has_value());
  DCHECK(observed_widget_.IsObservingSource(widget));
  callback_.Run(open_timer_->Elapsed());
  open_timer_.reset();
  observed_widget_.Reset();
}

void WidgetOpenTimer::Reset(Widget* widget) {
  DCHECK(!open_timer_.has_value());
  DCHECK(!observed_widget_.IsObservingSource(widget));
  observed_widget_.Observe(widget);
  open_timer_ = base::ElapsedTimer();
}

gfx::NativeWindow GetRootWindow(const Widget* widget) {
  gfx::NativeWindow window = widget->GetNativeWindow();
#if defined(USE_AURA)
  window = window->GetRootWindow();
#endif
  return window;
}

}  // namespace views
