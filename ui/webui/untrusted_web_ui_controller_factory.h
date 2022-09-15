// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_
#define UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_

#include "content/public/browser/web_ui_controller_factory.h"

class GURL;

namespace content {
class BrowserContext;
class WebUIController;
class WebUIConfig;
}  // namespace content

namespace ui {

// Factory class for WebUIControllers for chrome-untrusted:// URLs.
//
// To add a new WebUIController, subclass ui::WebUIConfig and add it to
// `CreateConfigs()` in the .cc.
class UntrustedWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  UntrustedWebUIControllerFactory();
  ~UntrustedWebUIControllerFactory() override;
  UntrustedWebUIControllerFactory(const UntrustedWebUIControllerFactory&) =
      delete;
  UntrustedWebUIControllerFactory& operator=(
      const UntrustedWebUIControllerFactory&) = delete;

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) final;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) final;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) final;

 protected:
  // Map of hosts to their corresponding WebUIConfigs.
  using WebUIConfigMap =
      base::flat_map<std::string, std::unique_ptr<content::WebUIConfig>>;
  virtual const WebUIConfigMap& GetWebUIConfigMap() = 0;

 private:
  // Returns the WebUIConfig for |url| if it's registered and the WebUI is
  // enabled. (WebUIs can be disabled based on the profile or feature flags.)
  content::WebUIConfig* GetConfigIfWebUIEnabled(
      content::BrowserContext* browser_context,
      const GURL& url);
};

}  // namespace ui

#endif  // UI_WEBUI_UNTRUSTED_WEB_UI_CONTROLLER_FACTORY_H_
