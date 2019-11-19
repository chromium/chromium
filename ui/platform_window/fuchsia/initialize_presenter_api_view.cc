// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace ui {
namespace fuchsia {

void InitializeViewTokenAndPresentView(
    ui::PlatformWindowInitProperties* window_properties_out) {
  DCHECK(window_properties_out);

  // Generate ViewToken and ViewHolderToken for the new view.
  ::fuchsia::ui::views::ViewHolderToken view_holder_token;
  std::tie(window_properties_out->view_token, view_holder_token) =
      scenic::NewViewTokenPair();

  // Create a ViewRefPair so the view can be registered to the SemanticsManager.
  window_properties_out->view_ref_pair = scenic::ViewRefPair::New();

  // Request Presenter to show the view full-screen.
  auto presenter = base::fuchsia::ComponentContextForCurrentProcess()
                       ->svc()
                       ->Connect<::fuchsia::ui::policy::Presenter>();

  presenter->PresentView(std::move(view_holder_token), nullptr);
}

}  // namespace fuchsia
}  // namespace ui
