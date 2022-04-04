// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_config_map.h"

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_config.h"
#include "url/gurl.h"

namespace ui {

namespace {

// Owned by WebUIConfigMap. Used to hook up with the existing WebUI infra.
class WebUIConfigMapWebUIControllerFactory
    : public content::WebUIControllerFactory {
 public:
  explicit WebUIConfigMapWebUIControllerFactory(WebUIConfigMap& config_map)
      : config_map_(config_map) {}
  ~WebUIConfigMapWebUIControllerFactory() override = default;

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    auto* config =
        config_map_.GetConfig(browser_context, url::Origin::Create(url));
    if (!config)
      return content::WebUI::kNoWebUI;

    return reinterpret_cast<content::WebUI::TypeID>(config);
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return config_map_.GetConfig(browser_context, url::Origin::Create(url));
  }

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    auto* config =
        config_map_.GetConfig(browser_context, url::Origin::Create(url));
    if (!config)
      return nullptr;

    return config->CreateWebUIController(web_ui);
  }

 private:
  // Keeping a reference should be safe since this class is owned by
  // WebUIConfigMap.
  WebUIConfigMap& config_map_;
};

}  // namespace
// static
WebUIConfigMap& WebUIConfigMap::GetInstance() {
  static base::NoDestructor<WebUIConfigMap> instance;
  return *instance.get();
}

WebUIConfigMap::WebUIConfigMap()
    : webui_controller_factory_(
          std::make_unique<WebUIConfigMapWebUIControllerFactory>(*this)) {
  content::WebUIControllerFactory::RegisterFactory(
      webui_controller_factory_.get());
}

WebUIConfigMap::~WebUIConfigMap() = default;

void WebUIConfigMap::AddWebUIConfig(std::unique_ptr<WebUIConfig> config) {
  CHECK_EQ(config->scheme(), content::kChromeUIScheme);
  AddWebUIConfigImpl(std::move(config));
}

void WebUIConfigMap::AddUntrustedWebUIConfig(
    std::unique_ptr<WebUIConfig> config) {
  CHECK_EQ(config->scheme(), content::kChromeUIUntrustedScheme);
  AddWebUIConfigImpl(std::move(config));
}

void WebUIConfigMap::AddWebUIConfigImpl(std::unique_ptr<WebUIConfig> config) {
  GURL url(base::StrCat(
      {config->scheme(), url::kStandardSchemeSeparator, config->host()}));
  auto it = configs_map_.emplace(url::Origin::Create(url), std::move(config));
  // CHECK if a WebUIConfig with the same host was already added.
  CHECK(it.second) << url;
}

WebUIConfig* WebUIConfigMap::GetConfig(content::BrowserContext* browser_context,
                                       const url::Origin& origin) {
  auto origin_and_config = configs_map_.find(origin);
  if (origin_and_config == configs_map_.end())
    return nullptr;
  auto& config = origin_and_config->second;

  if (!config->IsWebUIEnabled(browser_context))
    return nullptr;

  return config.get();
}

}  // namespace ui
