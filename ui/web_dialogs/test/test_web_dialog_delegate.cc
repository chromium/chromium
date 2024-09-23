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
    : url_(url),
      size_(400, 400),
      did_delete_(nullptr),
      close_on_escape_(true) {}

TestWebDialogDelegate::~TestWebDialogDelegate() {
  if (did_delete_) {
    CHECK(!*did_delete_);
    *did_delete_ = true;
  }
}

void TestWebDialogDelegate::SetDeleteOnClosedAndObserve(
    bool* destroy_observer) {
  CHECK(destroy_observer);
  did_delete_ = destroy_observer;
}

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

void TestWebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  if (did_delete_)
    delete this;
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
