// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/mojo_bubble_web_ui_controller.h"

#include "content/public/browser/web_ui.h"

namespace ui {

MojoBubbleWebUIController::MojoBubbleWebUIController(content::WebUI* contents,
                                                     bool enable_chrome_send)
    : MojoWebUIController(contents, enable_chrome_send) {}

MojoBubbleWebUIController::~MojoBubbleWebUIController() = default;

}  // namespace ui
