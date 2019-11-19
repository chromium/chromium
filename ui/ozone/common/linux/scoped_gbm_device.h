// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_LINUX_SCOPED_GBM_DEVICE_H_
#define UI_OZONE_COMMON_LINUX_SCOPED_GBM_DEVICE_H_

#include <gbm.h>

#include <memory>

namespace ui {

struct GbmDeviceDeleter {
  void operator()(gbm_device* device);
};

using ScopedGbmDevice = std::unique_ptr<gbm_device, GbmDeviceDeleter>;

}  // namespace ui

#endif  // UI_OZONE_COMMON_LINUX_SCOPED_GBM_DEVICE_H_
