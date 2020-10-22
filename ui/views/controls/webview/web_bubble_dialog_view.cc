// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_bubble_dialog_view.h"

#include <memory>

#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

bool IsEscapeEvent(const content::NativeWebKeyboardEvent& event) {
  return event.GetType() ==
             content::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
}

// The min / max size available to the WebBubbleDialogView.
// These are arbitrary sizes that match those set by ExtensionPopup.
// TODO(tluk): Determine the correct size constraints for the
// WebBubbleDialogView.
constexpr gfx::Size kMinSize(25, 25);
constexpr gfx::Size kMaxSize(800, 600);

class BubbleWebView : public WebView {
 public:
  BubbleWebView(content::BrowserContext* browser_context,
                WebBubbleDialogView* parent)
      : WebView(browser_context), parent_(parent) {
    EnableSizingFromWebContents(kMinSize, kMaxSize);
  }

  ~BubbleWebView() override = default;

  // WebView:
  void PreferredSizeChanged() override {
    WebView::PreferredSizeChanged();
    parent_->OnWebViewSizeChanged();
  }

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override {
    // Ignores context menu.
    return true;
  }
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    // Close the bubble if an escape event is detected. Handle this here to
    // prevent the renderer from capturing the event and not propagating it up.
    if (IsEscapeEvent(event) && GetWidget()) {
      GetWidget()->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      return content::KeyboardEventProcessingResult::HANDLED;
    }
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

 private:
  WebBubbleDialogView* parent_;
};

}  // namespace

WebBubbleDialogView::WebBubbleDialogView(
    content::BrowserContext* browser_context,
    View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, BubbleBorder::TOP_RIGHT),
      web_view_(AddChildView(
          std::make_unique<BubbleWebView>(browser_context, this))) {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_margins(gfx::Insets());

  SetLayoutManager(std::make_unique<FillLayout>());
  SetVisible(false);
}

WebBubbleDialogView::~WebBubbleDialogView() = default;

void WebBubbleDialogView::OnWebViewSizeChanged() {
  if (!hosted_in_bubble_) {
    PreferredSizeChanged();
    return;
  }
  SizeToContents();
}

gfx::Size WebBubbleDialogView::CalculatePreferredSize() const {
  // Constrain the size to popup min/max.
  gfx::Size preferred_size = BubbleDialogDelegateView::CalculatePreferredSize();
  preferred_size.SetToMax(kMinSize);
  preferred_size.SetToMin(kMaxSize);
  return preferred_size;
}

void WebBubbleDialogView::AddedToWidget() {
  if (!hosted_in_bubble_)
    return;
  BubbleDialogDelegateView::AddedToWidget();
  web_view_->holder()->SetCornerRadii(gfx::RoundedCornersF(GetCornerRadius()));
}

void WebBubbleDialogView::ShowUI() {
  SetVisible(true);
  DCHECK(GetWidget());
  GetWidget()->Show();
  web_view_->GetWebContents()->Focus();
}

}  // namespace views
