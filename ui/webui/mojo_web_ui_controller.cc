// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_web_ui_controller.h"

#include "content/public/common/bindings_policy.h"

namespace ui {

EnableMojoWebUI::EnableMojoWebUI(content::WebUI* contents,
                                 bool enable_chrome_send) {
  content::BindingsPolicySet bindings(
      {content::BindingsPolicyValue::kMojoWebUi});
  if (enable_chrome_send) {
    bindings.Put(content::BindingsPolicyValue::kWebUi);
  }
  contents->SetBindings(bindings);
}

EnableMojoWebUI::~EnableMojoWebUI() = default;

MojoWebUIController::MojoWebUIController(content::WebUI* contents,
                                         bool enable_chrome_send)
    : content::WebUIController(contents),
      EnableMojoWebUI(contents, enable_chrome_send) {}

MojoWebUIController::~MojoWebUIController() = default;

}  // namespace ui
