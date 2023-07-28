// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_H_
#define UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_H_

#include "ui/webui/mojo_web_ui_controller.h"

namespace webui_examples {

class Browser : public ui::MojoWebUIController {
 public:
  static constexpr char kHost[] = "browser";

  Browser(content::WebUI* web_ui);
  Browser(const Browser&) = delete;
  Browser& operator=(const Browser&) = delete;
  ~Browser() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace webui_examples

#endif  // UI_WEBUI_EXAMPLES_BROWSER_UI_WEB_BROWSER_H_
