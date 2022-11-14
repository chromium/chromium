// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/examples_views_delegate_chromeos.h"

#include "ui/views/examples/examples_window.h"
#include "ui/wm/test/wm_test_helper.h"

namespace views::examples {

namespace {
constexpr gfx::Size kDefaultSize(1024, 768);
}  // namespace

ExamplesViewsDelegateChromeOS::ExamplesViewsDelegateChromeOS() = default;

ExamplesViewsDelegateChromeOS::~ExamplesViewsDelegateChromeOS() = default;

void ExamplesViewsDelegateChromeOS::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  views::TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
  if (!params->parent && !params->context) {
    DCHECK(!wm_helper_);

    wm_helper_ = std::make_unique<wm::WMTestHelper>(kDefaultSize);
    wm_helper_->host()->Show();
    observation_.Observe(wm_helper_->host());
    params->context = wm_helper_->host()->window();
  }
}

void ExamplesViewsDelegateChromeOS::OnHostCloseRequested(
    aura::WindowTreeHost* host) {
  Widget* widget = GetExamplesWidget();
  if (widget) {
    DCHECK(observation_.IsObservingSource(host));
    observation_.Reset();
    widget->Close();
  }
}

}  // namespace views::examples
