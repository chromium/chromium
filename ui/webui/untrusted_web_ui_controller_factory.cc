// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/untrusted_web_ui_controller_factory.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ui {

UntrustedWebUIControllerFactory::UntrustedWebUIControllerFactory() = default;

UntrustedWebUIControllerFactory::~UntrustedWebUIControllerFactory() = default;

content::WebUI::TypeID UntrustedWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  auto* config = GetConfigIfWebUIEnabled(browser_context, url);
  if (!config)
    return content::WebUI::kNoWebUI;

  return reinterpret_cast<content::WebUI::TypeID>(config);
}

bool UntrustedWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetConfigIfWebUIEnabled(browser_context, url);
}

std::unique_ptr<content::WebUIController>
UntrustedWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  auto* config = GetConfigIfWebUIEnabled(browser_context, url);
  if (!config)
    return nullptr;

  return config->CreateWebUIController(web_ui, url);
}

content::WebUIConfig* UntrustedWebUIControllerFactory::GetConfigIfWebUIEnabled(
    content::BrowserContext* browser_context,
    const GURL& url) {
  // This factory doesn't support non chrome-untrusted:// WebUIs.
  if (!url.SchemeIs(content::kChromeUIUntrustedScheme))
    return nullptr;

  auto it = GetWebUIConfigMap().find(url.host_piece());
  if (it == GetWebUIConfigMap().end())
    return nullptr;

  if (!it->second->IsWebUIEnabled(browser_context))
    return nullptr;

  return it->second.get();
}

}  // namespace ui
