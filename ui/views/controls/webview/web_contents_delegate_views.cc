// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/webview/web_contents_delegate_views.h"

#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace views::internal {

namespace {

// Helper that allows for the delegate to be fetched from the `WebContents`.
class WebContentsDelegateViewsBaseHelper
    : public content::WebContentsUserData<WebContentsDelegateViewsBaseHelper> {
 public:
  ~WebContentsDelegateViewsBaseHelper() override = default;

  void set_delegate(base::WeakPtr<WebContentsDelegateViewsBase> delegate) {
    delegate_ = delegate;
  }

  WebContentsDelegateViewsBase* delegate() { return delegate_.get(); }

 private:
  explicit WebContentsDelegateViewsBaseHelper(content::WebContents* contents)
      : WebContentsUserData(*contents) {}

  friend content::WebContentsUserData<WebContentsDelegateViewsBaseHelper>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  base::WeakPtr<WebContentsDelegateViewsBase> delegate_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsDelegateViewsBaseHelper);

}  // namespace

WebContentsDelegateViewsBase::WebContentsDelegateViewsBase() = default;
WebContentsDelegateViewsBase::~WebContentsDelegateViewsBase() = default;

void WebContentsDelegateViewsBase::SetReferenceView(
    views::View* reference_view) {
  if (reference_view) {
    reference_view_.emplace(reference_view);
  } else {
    reference_view_.reset();
  }
}

void WebContentsDelegateViewsBase::SetOnEscCallback(
    WebContentsOnEscapeCallback on_esc_callback) {
  on_esc_callback_ = on_esc_callback;
}

// Convenience method which sets the on-ESC callback to close the widget
// associated with the reference view (which might be `this` if `this` is a
// view).
void WebContentsDelegateViewsBase::SetOnEscCloseWidget() {
  CHECK(GetReferenceView())
      << "Must set a valid reference view before enabling close-on-ESC.";
  SetOnEscCallback(base::BindRepeating(
      [](WebContentsDelegateViewsBase* delegate) {
        if (auto* const view = delegate->GetReferenceView()) {
          if (auto* const widget = view->GetWidget()) {
            widget->CloseWithReason(Widget::ClosedReason::kEscKeyPressed);
            return true;
          }
        }
        return false;
      },
      base::Unretained(this)));
}

void WebContentsDelegateViewsBase::SetAcceleratorHandling(
    WebContentsAcceleratorHandling accelerator_handling) {
  CHECK(accelerator_handling == WebContentsAcceleratorHandling::kDisabled ||
        GetReferenceView())
      << "Must set a valid reference view before enabling accelerator "
         "handling.";
  accelerator_handling_ = accelerator_handling;
}

bool WebContentsDelegateViewsBase::MaybeProcessEscape(
    const input::NativeWebKeyboardEvent& event) {
  if (on_esc_callback_) {
    if (event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
        event.windows_key_code == ui::VKEY_ESCAPE) {
      if (on_esc_callback_.Run()) {
        return true;
      }
    }
  }
  return false;
}

bool WebContentsDelegateViewsBase::MaybeProcessKeypress(
    const input::NativeWebKeyboardEvent& event) {
  if (accelerator_handling_ != WebContentsAcceleratorHandling::kDisabled) {
    if (auto* const view = GetReferenceView()) {
      if (auto* focus_manager = view->GetFocusManager()) {
        return event_handler_.HandleKeyboardEvent(event, focus_manager);
      }
    }
  }
  return false;
}

void WebContentsDelegateViewsBase::OnAttached(content::WebContents* contents) {
  auto* const helper =
      WebContentsDelegateViewsBaseHelper::GetOrCreateForWebContents(contents);
  DCHECK_EQ(nullptr, helper->delegate());
  helper->set_delegate(weak_ptr_factory_.GetWeakPtr());
}

void WebContentsDelegateViewsBase::OnDetaching(content::WebContents* contents) {
  auto* const helper =
      WebContentsDelegateViewsBaseHelper::GetOrCreateForWebContents(contents);
  DCHECK_EQ(this, helper->delegate());
  helper->set_delegate(nullptr);
}

// static
WebContentsAcceleratorHandling
WebContentsDelegateViewsBase::GetAcceleratorHandlingFor(
    content::WebContents& contents) {
  if (auto* const delegate = FromWebContents(contents)) {
    return delegate->accelerator_handling_;
  }
  return WebContentsAcceleratorHandling::kDisabled;
}

// static
WebContentsDelegateViewsBase* WebContentsDelegateViewsBase::FromWebContents(
    content::WebContents& contents) {
  if (auto* const helper =
          WebContentsDelegateViewsBaseHelper::FromWebContents(&contents)) {
    return helper->delegate();
  }
  return nullptr;
}

}  // namespace views::internal
