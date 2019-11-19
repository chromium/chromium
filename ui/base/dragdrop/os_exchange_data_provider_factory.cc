// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"

#include "build/build_config.h"

#if defined(USE_X11)
#include "ui/base/dragdrop/os_exchange_data_provider_aurax11.h"
#elif defined(OS_LINUX)
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#elif defined(OS_MACOSX)
#include "ui/base/dragdrop/os_exchange_data_provider_builder_mac.h"
#elif defined(OS_WIN)
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#endif

namespace ui {

//static
std::unique_ptr<OSExchangeData::Provider>
OSExchangeDataProviderFactory::CreateProvider() {
#if defined(USE_X11)
  return std::make_unique<OSExchangeDataProviderAuraX11>();
#elif defined(OS_LINUX)
  return std::make_unique<OSExchangeDataProviderAura>();
#elif defined(OS_MACOSX)
  return ui::BuildOSExchangeDataProviderMac();
#elif defined(OS_WIN)
  return std::make_unique<OSExchangeDataProviderWin>();
#elif defined(OS_FUCHSIA)
  // TODO(crbug.com/980371): Implement OSExchangeDataProvider for Fuchsia.
  NOTIMPLEMENTED();
  return nullptr;
#else
#error "Unknown operating system"
#endif
}

}  // namespace ui
