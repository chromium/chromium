// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"

#include "base/notreached.h"
#include "build/build_config.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_FUCHSIA)
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/base/ui_base_features.h"
#if defined(USE_OZONE)
#include "ui/base/dragdrop/os_exchange_data_provider_factory_ozone.h"
#endif  // defined(USE_OZONE)
#if defined(USE_X11)
#include "ui/base/dragdrop/os_exchange_data_provider_x11.h"
#endif  // defined(USE_X11)
#elif defined(OS_APPLE)
#include "ui/base/dragdrop/os_exchange_data_provider_builder_mac.h"
#elif defined(OS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace ui {

namespace {

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
std::unique_ptr<OSExchangeDataProvider> CreateProviderForLinux() {
#if defined(USE_OZONE)
  // The instance can be nullptr in tests that do not instantiate the platform,
  // or on platforms that do not implement specific drag'n'drop.  For them,
  // falling back to the Aura provider should be fine.
  if (OSExchangeDataProviderFactoryOzone::Instance()) {
    auto provider =
        OSExchangeDataProviderFactoryOzone::Instance()->CreateProvider();
    if (provider)
      return provider;
  }
#endif  // defined(USE_OZONE)
  // non-Ozone X11 is never expected to reach this.
  DCHECK(features::IsUsingOzonePlatform());
  return std::make_unique<OSExchangeDataProviderNonBacked>();
}
#endif  // defined(USE_LINUX)

}  // namespace

// static
std::unique_ptr<OSExchangeDataProvider>
OSExchangeDataProviderFactory::CreateProvider() {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (features::IsUsingOzonePlatform())
    return CreateProviderForLinux();
#if defined(USE_X11)
  return std::make_unique<OSExchangeDataProviderX11>();
#endif  // defined(USE_X11)
  NOTREACHED();
  return nullptr;
#elif defined(OS_APPLE)
  return BuildOSExchangeDataProviderMac();
#elif defined(OS_WIN)
  return std::make_unique<OSExchangeDataProviderWin>();
#elif defined(OS_FUCHSIA)
  // TODO(crbug.com/980371): Implement OSExchangeDataProvider for Fuchsia.
  return std::make_unique<OSExchangeDataProviderNonBacked>();
#else
#error "Unknown operating system"
#endif
}

}  // namespace ui
