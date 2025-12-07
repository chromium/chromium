// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WEB_DIALOGS_TEST_TEST_WEB_DIALOG_DELEGATE_H_
#define UI_WEB_DIALOGS_TEST_TEST_WEB_DIALOG_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace ui {
namespace test {

class TestWebDialogDelegate : public WebDialogDelegate {
 public:
  explicit TestWebDialogDelegate(const GURL& url);

  TestWebDialogDelegate(const TestWebDialogDelegate&) = delete;
  TestWebDialogDelegate& operator=(const TestWebDialogDelegate&) = delete;

  ~TestWebDialogDelegate() override;

  void set_size(int width, int height) {
    size_.SetSize(width, height);
  }

  // Sets the test delegate to delete when closed, as recommended by
  // WebDialogDelegate::OnDialogClosed(). |observed_destroy| must be a pointer
  // to a bool, which will be set to true when the destructor is called.
  void SetDeleteOnClosedAndObserve(bool* destroy_observer);

  // Sets whether the dialog should close when we press Escape.
  void SetCloseOnEscape(bool enabled);

  // WebDialogDelegate implementation:
  ui::mojom::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCloseDialogOnEscape() const override;

 protected:
  const GURL url_;
  gfx::Size size_;
  raw_ptr<bool> did_delete_;
  bool close_on_escape_;
};

}  // namespace test
}  // namespace ui

#endif  // UI_WEB_DIALOGS_TEST_TEST_WEB_DIALOG_DELEGATE_H_
