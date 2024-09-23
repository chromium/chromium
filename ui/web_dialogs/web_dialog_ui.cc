// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_ui.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

using content::RenderFrameHost;
using content::WebUIMessageHandler;

namespace ui {

namespace {

const char kWebDialogDelegateUserDataKey[] = "WebDialogDelegateUserData";

class WebDialogDelegateUserData : public base::SupportsUserData::Data {
 public:
  explicit WebDialogDelegateUserData(WebDialogDelegate* delegate)
      : delegate_(delegate) {}
  ~WebDialogDelegateUserData() override {}
  WebDialogDelegate* delegate() { return delegate_; }

 private:
  raw_ptr<WebDialogDelegate> delegate_;  // unowned
};

}  // namespace

// static
void WebDialogUIBase::SetDelegate(content::WebContents* web_contents,
                                  WebDialogDelegate* delegate) {
  web_contents->SetUserData(
      &kWebDialogDelegateUserDataKey,
      std::make_unique<WebDialogDelegateUserData>(delegate));
}

WebDialogUIBase::WebDialogUIBase(content::WebUI* web_ui) : web_ui_(web_ui) {}

// Don't unregister our user data. During the teardown of the WebContents, this
// will be deleted, but the WebContents will already be destroyed.
//
// This object is owned indirectly by the WebContents. WebUIs can change, so
// it's scary if this WebUI is changed out and replaced with something else,
// since the user data will still point to the old delegate. But the delegate is
// itself the owner of the WebContents for a dialog so will be in scope, and the
// HTML dialogs won't swap WebUIs anyway since they don't navigate.
WebDialogUIBase::~WebDialogUIBase() = default;

void WebDialogUIBase::CloseDialog(const base::Value::List& args) {
  OnDialogClosed(args);
}

WebDialogDelegate* WebDialogUIBase::GetDelegate(
    content::WebContents* web_contents) {
  WebDialogDelegateUserData* user_data =
      static_cast<WebDialogDelegateUserData*>(
          web_contents->GetUserData(&kWebDialogDelegateUserDataKey));

  return user_data ? user_data->delegate() : NULL;
}

void WebDialogUIBase::HandleRenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  web_ui_->RegisterMessageCallback(
      "dialogClose", base::BindRepeating(&WebDialogUIBase::OnDialogClosed,
                                         base::Unretained(this)));

  // Pass the arguments to the renderer supplied by the delegate.
  std::string dialog_args;
  std::vector<WebUIMessageHandler*> handlers;
  WebDialogDelegate* delegate = GetDelegate(web_ui_->GetWebContents());
  if (delegate) {
    dialog_args = delegate->GetDialogArgs();
    delegate->GetWebUIMessageHandlers(&handlers);
  }

  if (web_ui_->GetBindings().Has(content::BindingsPolicyValue::kWebUi)) {
    render_frame_host->SetWebUIProperty("dialogArguments", dialog_args);
  }
  for (WebUIMessageHandler* handler : handlers) {
    web_ui_->AddMessageHandler(base::WrapUnique(handler));
  }

  if (delegate) {
    delegate->OnDialogShown(web_ui_);
  }
}

void WebDialogUIBase::OnDialogClosed(const base::Value::List& args) {
  WebDialogDelegate* delegate = GetDelegate(web_ui_->GetWebContents());
  if (delegate) {
    std::string json_retval;
    if (!args.empty()) {
      if (args[0].is_string())
        json_retval = args[0].GetString();
      else
        NOTREACHED_IN_MIGRATION() << "Could not read JSON argument";
    }

    delegate->OnDialogCloseFromWebUI(json_retval);
  }
}

WebDialogUI::WebDialogUI(content::WebUI* web_ui)
    : WebDialogUIBase(web_ui), content::WebUIController(web_ui) {}

WebDialogUI::~WebDialogUI() = default;

void WebDialogUI::WebUIRenderFrameCreated(RenderFrameHost* render_frame_host) {
  HandleRenderFrameCreated(render_frame_host);
}

// Note: chrome.send() must always be enabled for dialogs, since dialogs rely on
// chrome.send() to notify their handlers that the dialog should be closed. See
// the "dialogClose" message handler above in
// WebDialogUIBase::HandleRenderFrameCreated().
MojoWebDialogUI::MojoWebDialogUI(content::WebUI* web_ui)
    : WebDialogUIBase(web_ui),
      MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {}

MojoWebDialogUI::~MojoWebDialogUI() = default;

void MojoWebDialogUI::WebUIRenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  content::WebUIController::WebUIRenderFrameCreated(render_frame_host);
  HandleRenderFrameCreated(render_frame_host);
}

}  // namespace ui
