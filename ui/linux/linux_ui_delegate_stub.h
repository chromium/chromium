// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_DELEGATE_STUB_H_
#define UI_LINUX_LINUX_UI_DELEGATE_STUB_H_

#include "ui/linux/linux_ui_delegate.h"

namespace ui {

class COMPONENT_EXPORT(LINUX_UI) LinuxUiDelegateStub
    : public ui::LinuxUiDelegate {
 public:
  LinuxUiDelegateStub();
  LinuxUiDelegateStub(const LinuxUiDelegateStub&) = delete;
  LinuxUiDelegateStub& operator=(const LinuxUiDelegateStub&) = delete;
  ~LinuxUiDelegateStub() override;

  // LinuxUiDelegate:
  LinuxUiBackend GetBackend() const override;
  bool ExportWindowHandle(
      gfx::AcceleratedWidget window_id,
      base::OnceCallback<void(std::string)> callback) override;
};

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_DELEGATE_STUB_H_
