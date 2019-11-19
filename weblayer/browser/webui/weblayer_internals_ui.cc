// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/webui/weblayer_internals_ui.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "weblayer/browser/devtools_server_android.h"
#include "weblayer/grit/weblayer_resources.h"

namespace weblayer {

const char kChromeUIWebLayerHost[] = "weblayer";

WebLayerInternalsUI::WebLayerInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIWebLayerHost);
  source->AddResourcePath("weblayer_internals.js", IDR_WEBLAYER_INTERNALS_JS);
  source->AddResourcePath("weblayer_internals.mojom-lite.js",
                          IDR_WEBLAYER_INTERNALS_MOJO_JS);
  source->SetDefaultResource(IDR_WEBLAYER_INTERNALS_HTML);
  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                source);
  AddHandlerToRegistry(base::BindRepeating(
      &WebLayerInternalsUI::BindPageHandler, base::Unretained(this)));
}

WebLayerInternalsUI::~WebLayerInternalsUI() {}

#if defined(OS_ANDROID)
void WebLayerInternalsUI::GetRemoteDebuggingEnabled(
    GetRemoteDebuggingEnabledCallback callback) {
  std::move(callback).Run(DevToolsServerAndroid::GetRemoteDebuggingEnabled());
}

void WebLayerInternalsUI::SetRemoteDebuggingEnabled(bool enabled) {
  DevToolsServerAndroid::SetRemoteDebuggingEnabled(enabled);
}
#endif

void WebLayerInternalsUI::BindPageHandler(
    mojo::PendingReceiver<weblayer_internals::mojom::PageHandler>
        pending_receiver) {
  if (receiver_.is_bound())
    receiver_.reset();

  receiver_.Bind(std::move(pending_receiver));
}

}  // namespace weblayer
