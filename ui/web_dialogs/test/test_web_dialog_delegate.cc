// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/test/test_web_dialog_delegate.h"

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace ui {
namespace test {

TestWebDialogDelegate::TestWebDialogDelegate(const GURL& url)
    : url_(url), size_(400, 400), close_on_escape_(true) {
  set_delete_on_close(false);
}

TestWebDialogDelegate::~TestWebDialogDelegate() = default;

void TestWebDialogDelegate::SetCloseOnEscape(bool enabled) {
  close_on_escape_ = enabled;
}

ui::mojom::ModalType TestWebDialogDelegate::GetDialogModalType() const {
  return ui::mojom::ModalType::kWindow;
}

std::u16string TestWebDialogDelegate::GetDialogTitle() const {
  return u"Test";
}

GURL TestWebDialogDelegate::GetDialogContentURL() const {
  return url_;
}

void TestWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  *size = size_;
}

std::string TestWebDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void TestWebDialogDelegate::OnCloseContents(WebContents* source,
                                            bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool TestWebDialogDelegate::ShouldShowDialogTitle() const {
  return true;
}

bool TestWebDialogDelegate::ShouldCloseDialogOnEscape() const {
  return close_on_escape_;
}

}  // namespace test
}  // namespace ui
