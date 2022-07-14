// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_LINUX_UI_FACTORY_H_
#define UI_LINUX_LINUX_UI_FACTORY_H_

#include <memory>

namespace ui {

class LinuxUi;

// Returns a new LinuxUI based on a Linux toolkit.  May return nullptr if the
// preferred toolkits are unavailable.
std::unique_ptr<LinuxUi> CreateLinuxUi();

}  // namespace ui

#endif  // UI_LINUX_LINUX_UI_FACTORY_H_
