// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/linux/linux_ui_delegate.h"

#include "base/functional/callback.h"
#include "base/notreached.h"

namespace ui {

// static
LinuxUiDelegate* LinuxUiDelegate::instance_ = nullptr;

// static
LinuxUiDelegate* LinuxUiDelegate::GetInstance() {
  return instance_;
}

LinuxUiDelegate::LinuxUiDelegate() {
  DCHECK(!instance_);
  instance_ = this;
}

LinuxUiDelegate::~LinuxUiDelegate() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

bool LinuxUiDelegate::ExportWindowHandle(
    uint32_t parent_widget,
    base::OnceCallback<void(const std::string&)> callback) {
  // This function should not be called when using a platform that doesn't
  // implement it.
  NOTREACHED_IN_MIGRATION();
  return false;
}

void LinuxUiDelegate::SetTransientWindowForParent(
    gfx::AcceleratedWidget parent,
    gfx::AcceleratedWidget transient) {
  // This function should not be called when using a platform that doesn't
  // implement it.
  NOTREACHED_IN_MIGRATION();
}

}  // namespace ui
