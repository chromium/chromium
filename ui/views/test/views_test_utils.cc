// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_utils.h"

#include <utility>

#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace views::test {

void RunScheduledLayout(Widget* widget) {
  DCHECK(widget);
  widget->LayoutRootViewIfNecessary();
}

void RunScheduledLayout(View* view) {
  DCHECK(view);
  Widget* widget = view->GetWidget();
  if (widget) {
    RunScheduledLayout(widget);
    return;
  }
  View* parent_view = view;
  while (parent_view->parent()) {
    parent_view = parent_view->parent();
  }
  if (parent_view->needs_layout()) {
    parent_view->DeprecatedLayoutImmediately();
  }
}

bool IsOzoneBubblesUsingPlatformWidgets() {
#if BUILDFLAG(IS_OZONE)
  return base::FeatureList::IsEnabled(
             features::kOzoneBubblesUsePlatformWidgets) &&
         ui::OzonePlatform::GetInstance()
             ->GetPlatformRuntimeProperties()
             .supports_subwindows_as_accelerated_widgets;
#else
  return false;
#endif
}

PropertyWaiter::PropertyWaiter(base::RepeatingCallback<bool()> callback,
                               bool expected_value,
                               base::TimeDelta timeout)
    : timeout_(timeout),
      callback_(std::move(callback)),
      expected_value_(expected_value) {}
PropertyWaiter::~PropertyWaiter() = default;

bool PropertyWaiter::Wait() {
  if (callback_.Run() == expected_value_) {
    success_ = true;
    return success_;
  }
  start_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, base::TimeDelta(), this, &PropertyWaiter::Check);
  run_loop_.Run();
  return success_;
}

void PropertyWaiter::Check() {
  DCHECK(!success_);
  success_ = callback_.Run() == expected_value_;
  if (success_ || base::TimeTicks::Now() - start_time_ > timeout_) {
    timer_.Stop();
    run_loop_.Quit();
  }
}

}  // namespace views::test
