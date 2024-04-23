// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_DEVICE_PROVIDER_H_
#define WOLVIC_BROWSER_VR_WVR_DEVICE_PROVIDER_H_

#include <memory>

#include "device/vr/public/cpp/vr_device_provider.h"

namespace wolvic {

class WvrDevice;

class WvrDeviceProvider : public device::VRDeviceProvider {
 public:
  WvrDeviceProvider();

  WvrDeviceProvider(const WvrDeviceProvider&) = delete;
  WvrDeviceProvider& operator=(const WvrDeviceProvider&) = delete;

  ~WvrDeviceProvider() override;

  void Initialize(device::VRDeviceProviderClient* client,
                  content::WebContents* initializing_web_contents) override;
  bool Initialized() override;

 private:
  std::unique_ptr<WvrDevice> vr_device_;
  bool initialized_ = false;
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_DEVICE_PROVIDER_H_
