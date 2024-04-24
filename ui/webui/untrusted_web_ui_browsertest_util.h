// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_
#define UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_

#include <string_view>

#include "content/public/browser/webui_config.h"
#include "content/public/test/web_ui_browsertest_util.h"

namespace content {
class WebUIController;
}

namespace ui {

class TestUntrustedWebUIConfig : public content::WebUIConfig {
 public:
  explicit TestUntrustedWebUIConfig(std::string_view host);
  TestUntrustedWebUIConfig(
      std::string_view host,
      const content::TestUntrustedDataSourceHeaders& headers);
  ~TestUntrustedWebUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

  const content::TestUntrustedDataSourceHeaders headers_;
};

}  // namespace ui

#endif  // UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_
