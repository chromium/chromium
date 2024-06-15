// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test_api.h"

#include "base/notimplemented.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
#include <utility>

#include "base/test/run_until.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/extensions/wayland_extension.h"
#endif

namespace views {

void DisableActivationChangeHandlingForTests() {
  Widget::SetDisableActivationChangeHandling(
      Widget::DisableActivationChangeHandlingType::kIgnore);
}

AsyncWidgetRequestWaiter::AsyncWidgetRequestWaiter(Widget& widget)
    : widget_(widget) {
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    // Wait for a Wayland roundtrip to ensure all side effects have been
    // processed.
    auto* host = aura::WindowTreeHostPlatform::GetHostForWindow(
        widget_->GetNativeWindow());
    auto* wayland_extension = ui::GetWaylandExtension(*host->platform_window());
    wayland_extension->SetLatchImmediately(false);
  }
#endif
}

AsyncWidgetRequestWaiter::~AsyncWidgetRequestWaiter() {
  CHECK(waited_)
      << "AsyncWidgetRequestWaiter has no effect unless `Wait` is called.";
}

void AsyncWidgetRequestWaiter::Wait() {
  CHECK(!waited_) << "`Wait` may only be called once.";
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  if (ui::OzonePlatform::GetPlatformNameForTest() == "wayland") {
    // Wait for a Wayland roundtrip to ensure all side effects have been
    // processed.
    auto* host = aura::WindowTreeHostPlatform::GetHostForWindow(
        widget_->GetNativeWindow());
    auto* wayland_extension = ui::GetWaylandExtension(*host->platform_window());
    wayland_extension->RoundTripQueue();

    // Wait for all in flight requests to be latched.
    CHECK(base::test::RunUntil([&]() {
      return !wayland_extension->HasInFlightRequestsForState() &&
             wayland_extension->GetVizSequenceIdForAppliedState() ==
                 wayland_extension->GetVizSequenceIdForLatchedState();
    })) << "Has in flight requests: "
        << wayland_extension->HasInFlightRequestsForState()
        << ", applied sequence ID: "
        << wayland_extension->GetVizSequenceIdForAppliedState()
        << ", latched sequence ID:"
        << wayland_extension->GetVizSequenceIdForLatchedState();

    // Wait for all Wayland messages sent as a result of requests being latched
    // to be processed on the server side.
    wayland_extension->RoundTripQueue();
    wayland_extension->SetLatchImmediately(true);
  } else {
    NOTIMPLEMENTED_LOG_ONCE();
  }
#else
  NOTIMPLEMENTED_LOG_ONCE();
#endif
  waited_ = true;
}

}  // namespace views
