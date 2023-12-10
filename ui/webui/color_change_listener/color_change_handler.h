// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_
#define UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_

#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {

// Handles ColorProvider related communication between C++ and WebUI in the
// renderer.
class ColorChangeHandler : public content::WebContentsObserver,
                           public color_change_listener::mojom::PageHandler {
 public:
  ColorChangeHandler(
      content::WebContents* web_contents,
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_page_handler);
  ColorChangeHandler(const ColorChangeHandler&) = delete;
  ColorChangeHandler& operator=(const ColorChangeHandler&) = delete;
  ~ColorChangeHandler() override;

  // content::WebContentsObserver:
  void OnColorProviderChanged() override;

  // color_change_listener::mojom::PageHandler:
  void SetPage(mojo::PendingRemote<color_change_listener::mojom::Page>
                   pending_page) override;

 private:
  mojo::Remote<color_change_listener::mojom::Page> page_;
  mojo::Receiver<color_change_listener::mojom::PageHandler> page_handler_;
};

}  // namespace ui

#endif  // UI_WEBUI_COLOR_CHANGE_LISTENER_COLOR_CHANGE_HANDLER_H_
