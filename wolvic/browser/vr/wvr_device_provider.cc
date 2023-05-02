// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_device_provider.h"

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "wolvic/browser/vr/wvr_device.h"

namespace wolvic {

WvrDeviceProvider::WvrDeviceProvider() {}
WvrDeviceProvider::~WvrDeviceProvider() = default;

void WvrDeviceProvider::Initialize(device::VRDeviceProviderClient* client) {
  vr_device_ = base::WrapUnique(new WvrDevice());
  if (vr_device_) {
    client->AddRuntime(vr_device_->GetId(), vr_device_->GetDeviceData(),
                       vr_device_->BindXRRuntime());
  }
  initialized_ = true;
  client->OnProviderInitialized();
}

bool WvrDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace wolvic
