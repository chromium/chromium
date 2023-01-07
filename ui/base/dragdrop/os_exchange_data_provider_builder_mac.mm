// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_builder_mac.h"

#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"

namespace ui {

std::unique_ptr<OSExchangeDataProvider> BuildOSExchangeDataProviderMac() {
  return OSExchangeDataProviderMac::CreateProvider();
}

}  // namespace ui
