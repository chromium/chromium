// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_
#define UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_

#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {

// Handles ColorProvider related communication between C++ and WebUI in the
// renderer.
class ColorChangeHandler : public content::WebContentsObserver,
                           public content::DocumentUserData<ColorChangeHandler>,
                           public color_change_listener::mojom::PageHandler {
 public:
  explicit ColorChangeHandler(content::RenderFrameHost* render_frame_host);
  ColorChangeHandler(const ColorChangeHandler&) = delete;
  ColorChangeHandler& operator=(const ColorChangeHandler&) = delete;
  ~ColorChangeHandler() override;

  void Bind(mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
                pending_receiver);

  // content::WebContentsObserver:
  void OnColorProviderChanged() override;

  // color_change_listener::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<color_change_listener::mojom::Page>
                   pending_page) override;

 private:
  friend content::DocumentUserData<ColorChangeHandler>;

  mojo::Remote<color_change_listener::mojom::Page> page_;
  mojo::Receiver<color_change_listener::mojom::PageHandler> receiver_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace ui

#endif  // UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_
