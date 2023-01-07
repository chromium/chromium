// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_BUILDER_MAC_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_BUILDER_MAC_H_

#include <memory>

#include "ui/base/dragdrop/os_exchange_data_provider.h"

namespace ui {

// We can't include os_exchange_data_provider_mac.h from arbitrary C++ files
// because it depends on the Objective-C headers.
std::unique_ptr<OSExchangeDataProvider> BuildOSExchangeDataProviderMac();

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_BUILDER_MAC_H_
