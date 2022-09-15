// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_OZONE_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_OZONE_H_

#include <memory>

#include "base/component_export.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"

namespace ui {

// Builds platform specific OSExchangeDataProviders.
class COMPONENT_EXPORT(UI_BASE_DATA_EXCHANGE)
    OSExchangeDataProviderFactoryOzone {
 public:
  // Creates a Provider based on the current platform.
  virtual std::unique_ptr<OSExchangeDataProvider> CreateProvider() = 0;

  virtual ~OSExchangeDataProviderFactoryOzone() = default;

  static OSExchangeDataProviderFactoryOzone* Instance() { return instance_; }

 protected:
  static void SetInstance(OSExchangeDataProviderFactoryOzone* instance);

 private:
  static OSExchangeDataProviderFactoryOzone* instance_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_FACTORY_OZONE_H_
