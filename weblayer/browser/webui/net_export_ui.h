// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBUI_NET_EXPORT_UI_H_
#define WEBLAYER_BROWSER_WEBUI_NET_EXPORT_UI_H_

#include "build/build_config.h"
#include "content/public/browser/web_ui_controller.h"

namespace weblayer {

extern const char kChromeUINetExportHost[];

class NetExportUI : public content::WebUIController {
 public:
  explicit NetExportUI(content::WebUI* web_ui);

  NetExportUI(const NetExportUI&) = delete;
  NetExportUI& operator=(const NetExportUI&) = delete;

  ~NetExportUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBUI_NET_EXPORT_UI_H_
