// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_H_

#include <memory>

#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ui_base_export.h"

namespace ui {

// Builds platform specific OSExchangeDataProviders.
class UI_BASE_EXPORT OSExchangeDataProviderFactory {
 public:
  // Creates a Provider based on the current platform.
  static std::unique_ptr<OSExchangeData::Provider> CreateProvider();
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_H_
