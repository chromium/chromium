// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LINUX_UI_LINUX_UI_FACTORY_H_
#define UI_VIEWS_LINUX_UI_LINUX_UI_FACTORY_H_

#include <memory>

namespace views {
class LinuxUI;
}

// Returns a new LinuxUI based on a Linux toolkit.  May return nullptr if the
// preferred toolkits are unavailable.
std::unique_ptr<views::LinuxUI> CreateLinuxUi();

#endif  // UI_VIEWS_LINUX_UI_LINUX_UI_FACTORY_H_
