// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/untrusted_web_ui_browsertest_util.h"

#include <string_view>

#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ui {

namespace {

class TestUntrustedWebUIController : public ui::UntrustedWebUIController {
 public:
  TestUntrustedWebUIController(
      content::WebUI* web_ui,
      const std::string& host,
      const content::TestUntrustedDataSourceHeaders& headers)
      : ui::UntrustedWebUIController(web_ui) {
    content::AddUntrustedDataSource(
        web_ui->GetWebContents()->GetBrowserContext(), host, headers);
  }

  ~TestUntrustedWebUIController() override = default;
};

}  // namespace

TestUntrustedWebUIConfig::TestUntrustedWebUIConfig(std::string_view host)
    : WebUIConfig(content::kChromeUIUntrustedScheme, host) {}

TestUntrustedWebUIConfig::TestUntrustedWebUIConfig(
    std::string_view host,
    const content::TestUntrustedDataSourceHeaders& headers)
    : WebUIConfig(content::kChromeUIUntrustedScheme, host), headers_(headers) {}

TestUntrustedWebUIConfig::~TestUntrustedWebUIConfig() = default;

std::unique_ptr<content::WebUIController>
TestUntrustedWebUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                const GURL& url) {
  return std::make_unique<TestUntrustedWebUIController>(web_ui, host(),
                                                        headers_);
}

}  // namespace ui
