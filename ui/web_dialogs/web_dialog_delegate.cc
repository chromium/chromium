// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

#include <utility>

#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"

namespace ui {

WebDialogDelegate::WebDialogDelegate() = default;
WebDialogDelegate::~WebDialogDelegate() = default;

mojom::ModalType WebDialogDelegate::GetDialogModalType() const {
  return modal_type_;
}

std::u16string WebDialogDelegate::GetDialogTitle() const {
  return title_;
}

std::u16string WebDialogDelegate::GetAccessibleDialogTitle() const {
  return accessible_title_.value_or(GetDialogTitle());
}

std::string WebDialogDelegate::GetDialogName() const {
  return name_;
}

GURL WebDialogDelegate::GetDialogContentURL() const {
  return content_url_;
}

void WebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) {
  // Note: even though this function returns a vector of WebUIMessageHandler*,
  // those are actually owning raw pointers. See the documentation for this
  // method in the header file.
  for (auto& handler : added_message_handlers_) {
    handlers->push_back(std::move(handler).release());
  }
  added_message_handlers_.clear();
}

void WebDialogDelegate::AddWebUIMessageHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  added_message_handlers_.emplace_back(std::move(handler));
}

void WebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

void WebDialogDelegate::GetMinimumDialogSize(gfx::Size* size) const {
  if (minimum_size_.has_value()) {
    *size = minimum_size_.value();
  } else {
    GetDialogSize(size);
  }
}

std::string WebDialogDelegate::GetDialogArgs() const {
  return args_;
}

bool WebDialogDelegate::CanMaximizeDialog() const {
  return can_maximize_;
}

bool WebDialogDelegate::OnDialogCloseRequested() {
  return true;
}

bool WebDialogDelegate::ShouldCenterDialogTitleText() const {
  return center_title_text_;
}

bool WebDialogDelegate::ShouldCloseDialogOnEscape() const {
  return close_on_escape_;
}

bool WebDialogDelegate::ShouldShowCloseButton() const {
  return show_close_button_;
}

bool WebDialogDelegate::ShouldShowDialogTitle() const {
  return show_title_;
}

void WebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  if (closed_callback_) {
    std::move(closed_callback_).Run(json_retval);
  }
  if (delete_on_close_) {
    delete this;
  }
}

void WebDialogDelegate::RegisterOnDialogClosedCallback(
    OnDialogClosedCallback callback) {
  closed_callback_ = std::move(callback);
}

void WebDialogDelegate::OnDialogCloseFromWebUI(
    const std::string& json_retval) {
  OnDialogClosed(json_retval);
}

void WebDialogDelegate::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  *out_close_dialog = can_close_;
}

bool WebDialogDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // TODO: this is very confusing. HandleContextMenu() returns true if a custom
  // context menu handler is installed. Actual overrides of this method return
  // true without showing a context menu to suppress the default context menu.
  // Therefore, when allow_default_context_menu_ is false, this function should
  // return true to prevent the default context menu from showing.
  return !allow_default_context_menu_;
}

bool WebDialogDelegate::HandleOpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback,
    content::WebContents** out_new_contents) {
  return false;
}

bool WebDialogDelegate::HandleShouldOverrideWebContentsCreation() {
  return !allow_web_contents_creation_;
}

std::vector<Accelerator> WebDialogDelegate::GetAccelerators() {
  std::vector<Accelerator> result;
  for (const auto& entry : accelerators_) {
    result.push_back(entry.first);
  }
  return result;
}

bool WebDialogDelegate::AcceleratorPressed(const Accelerator& accelerator) {
  auto it = accelerators_.find(accelerator);
  if (it != accelerators_.end()) {
    return it->second.Run(*this, accelerator);
  } else {
    return false;
  }
}

void WebDialogDelegate::RegisterAccelerator(Accelerator accelerator,
                                            AcceleratorHandler handler) {
  CHECK(!accelerators_.contains(accelerator));
  accelerators_.insert(std::make_pair(accelerator, handler));
}

bool WebDialogDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return false;
}

WebDialogDelegate::FrameKind WebDialogDelegate::GetWebDialogFrameKind() const {
  return frame_kind_;
}

}  // namespace ui
