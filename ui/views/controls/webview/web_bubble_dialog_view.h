// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEB_BUBBLE_DIALOG_VIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEB_BUBBLE_DIALOG_VIEW_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace views {

class Widget;

// A BubbleDialogDelegateView that hosts WebUI and resizes to fit the hosted
// WebContents.
class WEBVIEW_EXPORT WebBubbleDialogView
    : public BubbleDialogDelegateView,
      public ui::MojoBubbleWebUIController::Embedder {
 public:
  // |CreateWebBubbleDialog()| returns a Widget instance owned by its
  // NativeWidget. When the NativeWidget is destroyed (in response to a native
  // destruction message), it deletes the Widget from its destructor.
  template <typename T>
  static Widget* CreateWebBubbleDialog(
      std::unique_ptr<WebBubbleDialogView> bubble_view,
      const GURL& url) {
    bubble_view->SetVisible(true);
    bubble_view->LoadURL<T>(url);
    bubble_view->hosted_in_bubble_ = true;
    return BubbleDialogDelegateView::CreateBubble(std::move(bubble_view));
  }

  WebBubbleDialogView(content::BrowserContext* browser_context,
                      View* anchor_view);
  WebBubbleDialogView(const WebBubbleDialogView&) = delete;
  WebBubbleDialogView& operator=(const WebBubbleDialogView&) = delete;
  ~WebBubbleDialogView() override;

  void OnWebViewSizeChanged();
  WebView* web_view() { return web_view_; }

  // BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // MojoBubbleWebUIController::Embedder:
  void ShowUI() override;

  // The type T enables WebBubbleDialogView to know what WebUIController is
  // being used for the bubble's WebUI and allows it to make sure the associated
  // WebUI is a MojoBubbleWebUIController at compile time.
  // TODO(pbos): Move LoadURL into BubbleWebView.
  template <typename T>
  void LoadURL(const GURL& url) {
    // Lie to WebContents so it starts rendering and eventually calls ShowUI().
    web_view_->GetWebContents()->WasShown();
    web_view_->LoadInitialURL(url);
    T* webui_bubble_controller = web_view_->GetWebContents()
                                     ->GetWebUI()
                                     ->GetController()
                                     ->template GetAs<T>();
    // Depends on the WebUIController object being constructed synchronously
    // when the navigation is started in LoadInitialURL().
    webui_bubble_controller->set_embedder(weak_ptr_factory_.GetWeakPtr());
  }

  // Used for tests that create the bubble instead of calling
  // CreateWebBubbleDialog().
  void set_hosted_in_bubble_for_testing() { hosted_in_bubble_ = true; }

 private:
  WebView* const web_view_;
  // TODO(pbos): Remove this by separating WebBubbleDialogView content from its
  // BubbleDialogDelegateView parent.
  bool hosted_in_bubble_ = false;
  base::WeakPtrFactory<WebBubbleDialogView> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEB_BUBBLE_DIALOG_VIEW_H_
