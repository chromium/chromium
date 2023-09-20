// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

#include "ui/base/accelerators/accelerator.h"

namespace ui {

WebDialogDelegate::WebDialogDelegate() = default;
WebDialogDelegate::~WebDialogDelegate() = default;

ModalType WebDialogDelegate::GetDialogModalType() const {
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

void WebDialogDelegate::GetMinimumDialogSize(gfx::Size* size) const {
  if (minimum_size_.has_value()) {
    *size = minimum_size_.value();
  } else {
    GetDialogSize(size);
  }
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

void WebDialogDelegate::OnDialogCloseFromWebUI(
    const std::string& json_retval) {
  OnDialogClosed(json_retval);
}

bool WebDialogDelegate::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  return false;
}

bool WebDialogDelegate::HandleOpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    content::WebContents** out_new_contents) {
  return false;
}

bool WebDialogDelegate::HandleShouldOverrideWebContentsCreation() {
  return false;
}

std::vector<Accelerator> WebDialogDelegate::GetAccelerators() {
  return std::vector<Accelerator>();
}

bool WebDialogDelegate::AcceleratorPressed(const Accelerator& accelerator) {
  return false;
}

bool WebDialogDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return false;
}

WebDialogDelegate::FrameKind WebDialogDelegate::GetWebDialogFrameKind() const {
  return frame_kind_;
}

}  // namespace ui
