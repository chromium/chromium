// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LINUX_DEVICE_SCALE_FACTOR_OBSERVER_H_
#define UI_LINUX_DEVICE_SCALE_FACTOR_OBSERVER_H_

namespace ui {

class DeviceScaleFactorObserver {
 public:
  virtual ~DeviceScaleFactorObserver() = default;

  virtual void OnDeviceScaleFactorChanged() = 0;
};

}  // namespace ui

#endif  // UI_LINUX_DEVICE_SCALE_FACTOR_OBSERVER_H_
