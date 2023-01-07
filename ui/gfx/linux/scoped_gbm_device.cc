// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/scoped_gbm_device.h"

namespace ui {

void GbmDeviceDeleter::operator()(gbm_device* device) {
  if (device)
    gbm_device_destroy(device);
}

}  // namespace ui
