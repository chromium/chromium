// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

#include "ui/base/accelerators/accelerator.h"

namespace ui {

std::u16string WebDialogDelegate::GetAccessibleDialogTitle() const {
  return GetDialogTitle();
}

std::string WebDialogDelegate::GetDialogName() const {
  return std::string();
}

void WebDialogDelegate::GetMinimumDialogSize(gfx::Size* size) const {
  GetDialogSize(size);
}

bool WebDialogDelegate::CanMaximizeDialog() const {
  return false;
}

bool WebDialogDelegate::OnDialogCloseRequested() {
  return true;
}

bool WebDialogDelegate::ShouldCenterDialogTitleText() const {
  return false;
}

bool WebDialogDelegate::ShouldCloseDialogOnEscape() const {
  return true;
}

bool WebDialogDelegate::ShouldShowCloseButton() const {
  return true;
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
  return WebDialogDelegate::FrameKind::kNonClient;
}

}  // namespace ui
