// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_WEBUI_WEBLAYER_INTERNALS_UI_H_
#define WEBLAYER_BROWSER_WEBUI_WEBLAYER_INTERNALS_UI_H_

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "weblayer/browser/webui/weblayer_internals.mojom.h"

namespace weblayer {

extern const char kChromeUIWebLayerHost[];

class WebLayerInternalsUI : public ui::MojoWebUIController,
                            public weblayer_internals::mojom::PageHandler {
 public:
  explicit WebLayerInternalsUI(content::WebUI* web_ui);

  ~WebLayerInternalsUI() override;

 private:
  // weblayer_internals::mojom::PageHandler:
#if defined(OS_ANDROID)
  void GetRemoteDebuggingEnabled(
      GetRemoteDebuggingEnabledCallback callback) override;
  void SetRemoteDebuggingEnabled(bool enabled) override;
#endif

  void BindPageHandler(
      mojo::PendingReceiver<weblayer_internals::mojom::PageHandler>
          pending_receiver);

  mojo::Receiver<weblayer_internals::mojom::PageHandler> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(WebLayerInternalsUI);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_WEBUI_WEBLAYER_INTERNALS_UI_H_
