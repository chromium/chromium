// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/linux_ui/status_icon_linux.h"

namespace views {

StatusIconLinux::Delegate::~Delegate() = default;

StatusIconLinux::StatusIconLinux() = default;

StatusIconLinux::~StatusIconLinux() = default;

void StatusIconLinux::RefreshPlatformContextMenu() {}

void StatusIconLinux::OnSetDelegate() {}

void StatusIconLinux::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  OnSetDelegate();
}

}  // namespace views
