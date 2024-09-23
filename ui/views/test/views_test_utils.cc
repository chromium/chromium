// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_utils.h"

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
  while (parent_view->parent())
    parent_view = parent_view->parent();
  if (parent_view->needs_layout())
    parent_view->DeprecatedLayoutImmediately();
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

}  // namespace views::test
