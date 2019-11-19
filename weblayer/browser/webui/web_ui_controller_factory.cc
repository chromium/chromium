// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webui/web_ui_controller_factory.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"
#include "weblayer/browser/webui/weblayer_internals_ui.h"

namespace weblayer {
namespace {

const content::WebUI::TypeID kWebLayerID = &kWebLayerID;

// A function for creating a new WebUI. The caller owns the return value, which
// may be nullptr (for example, if the URL refers to an non-existent extension).
typedef content::WebUIController* (
    *WebUIFactoryFunctionPointer)(content::WebUI* web_ui, const GURL& url);

// Template for defining WebUIFactoryFunctionPointer.
template <class T>
content::WebUIController* NewWebUI(content::WebUI* web_ui, const GURL& url) {
  return new T(web_ui);
}

WebUIFactoryFunctionPointer GetWebUIFactoryFunctionPointer(const GURL& url) {
  if (url.host() == kChromeUIWebLayerHost) {
    return &NewWebUI<WebLayerInternalsUI>;
  }

  return nullptr;
}

content::WebUI::TypeID GetWebUITypeID(const GURL& url) {
  if (url.host() == kChromeUIWebLayerHost) {
    return kWebLayerID;
  }

  return content::WebUI::kNoWebUI;
}

}  // namespace

// static
WebUIControllerFactory* WebUIControllerFactory::GetInstance() {
  static base::NoDestructor<WebUIControllerFactory> instance;
  return instance.get();
}

WebUIControllerFactory::WebUIControllerFactory() {}

WebUIControllerFactory::~WebUIControllerFactory() {}

content::WebUI::TypeID WebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUITypeID(url);
}

bool WebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
}

bool WebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return UseWebUIForURL(browser_context, url);
}

std::unique_ptr<content::WebUIController>
WebUIControllerFactory::CreateWebUIControllerForURL(content::WebUI* web_ui,
                                                    const GURL& url) {
  WebUIFactoryFunctionPointer function = GetWebUIFactoryFunctionPointer(url);
  if (!function)
    return nullptr;

  return base::WrapUnique((*function)(web_ui, url));
}

}  // namespace weblayer
