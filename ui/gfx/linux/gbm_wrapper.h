// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_WRAPPER_H_
#define UI_GFX_LINUX_GBM_WRAPPER_H_

#include <memory>

#include "ui/gfx/linux/gbm_device.h"

namespace ui {

std::unique_ptr<ui::GbmDevice> CreateGbmDevice(int fd);

}  // namespace ui

#endif  // UI_GFX_LINUX_GBM_WRAPPER_H_
