// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#elif BUILDFLAG(IS_APPLE)
#include "ui/base/dragdrop/os_exchange_data_provider_builder_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace ui {

// static
std::unique_ptr<OSExchangeDataProvider>
OSExchangeDataProviderFactory::CreateProvider() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The instance can be nullptr in tests that do not instantiate the platform,
  // or on platforms that do not implement specific drag'n'drop.  For them,
  // falling back to the Aura provider should be fine.
  if (auto* factory = OSExchangeDataProviderFactoryOzone::Instance()) {
    auto provider = factory->CreateProvider();
    if (provider)
      return provider;
  }
  return std::make_unique<OSExchangeDataProviderNonBacked>();
#elif BUILDFLAG(IS_APPLE)
  return BuildOSExchangeDataProviderMac();
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<OSExchangeDataProviderWin>();
#elif BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/42050646): Implement OSExchangeDataProvider for Fuchsia.
  return std::make_unique<OSExchangeDataProviderNonBacked>();
#else
#error "Unknown operating system"
#endif
}

}  // namespace ui
