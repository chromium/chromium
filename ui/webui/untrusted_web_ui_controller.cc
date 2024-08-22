// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/untrusted_web_ui_controller.h"

#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"

namespace ui {

UntrustedWebUIController::UntrustedWebUIController(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // UntrustedWebUIController should never enable bindings.
  web_ui->SetBindings(content::BindingsPolicySet());
}

UntrustedWebUIController::~UntrustedWebUIController() = default;

}  // namespace ui
