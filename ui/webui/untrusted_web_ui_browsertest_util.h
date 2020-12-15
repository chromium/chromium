// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_
#define UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_

#include "content/public/test/web_ui_browsertest_util.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/untrusted_web_ui_controller_factory.h"
#include "ui/webui/webui_config.h"

namespace ui {

class TestUntrustedWebUIControllerFactory
    : public ui::UntrustedWebUIControllerFactory {
 public:
  TestUntrustedWebUIControllerFactory();
  ~TestUntrustedWebUIControllerFactory() override;

  void add_web_ui_config(std::unique_ptr<ui::WebUIConfig> config) {
    const std::string host = config->host();
    configs_.insert(std::make_pair(host, std::move(config)));
  }

 protected:
  const WebUIConfigMap& GetWebUIConfigMap() override;

 private:
  WebUIConfigMap configs_;
};

class TestUntrustedWebUIConfig : public ui::WebUIConfig {
 public:
  explicit TestUntrustedWebUIConfig(base::StringPiece host);
  explicit TestUntrustedWebUIConfig(
      base::StringPiece host,
      const content::TestUntrustedDataSourceCSP& content_security_policy);
  ~TestUntrustedWebUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
  const content::TestUntrustedDataSourceCSP content_security_policy_;
};

class TestUntrustedWebUIController : public ui::UntrustedWebUIController {
 public:
  explicit TestUntrustedWebUIController(
      content::WebUI* web_ui,
      const std::string& host,
      const content::TestUntrustedDataSourceCSP& content_security_policy);
  ~TestUntrustedWebUIController() override;
};

}  // namespace ui

#endif  // UI_WEBUI_UNTRUSTED_WEB_UI_BROWSERTEST_UTIL_H_
