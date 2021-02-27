// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"

namespace ui {

namespace {

class GlobalManager : public ColorProviderManager {
 public:
  GlobalManager() = default;
  GlobalManager(const GlobalManager&) = delete;
  GlobalManager& operator=(const GlobalManager&) = delete;
  ~GlobalManager() override = default;
};

static_assert(sizeof(GlobalManager) == sizeof(ColorProviderManager),
              "Global manager is intended to provide constructor visibility to "
              "base::Optional, nothing more.");

base::Optional<GlobalManager>& GetGlobalManager() {
  static base::NoDestructor<base::Optional<GlobalManager>> manager;
  return *manager;
}

}  // namespace

ColorProviderManager::ColorProviderManager() = default;
ColorProviderManager::~ColorProviderManager() = default;

// static
ColorProviderManager& ColorProviderManager::Get() {
  base::Optional<GlobalManager>& manager = GetGlobalManager();
  if (!manager.has_value())
    manager.emplace();

  return manager.value();
}

// static
void ColorProviderManager::ResetForTesting() {
  GetGlobalManager().reset();
}

void ColorProviderManager::SetColorProviderInitializer(
    ColorProviderInitializer initializer) {
  DCHECK(initializer_.is_null());
  DCHECK(color_providers_.empty());
  initializer_ = std::move(initializer);
}

ColorProvider* ColorProviderManager::GetColorProviderFor(
    ColorMode color_mode,
    ContrastMode contrast_mode) {
  auto key = ColorProviderKey(color_mode, contrast_mode);
  auto iter = color_providers_.find(key);
  if (iter == color_providers_.end()) {
    auto provider = std::make_unique<ColorProvider>();
    if (!initializer_.is_null()) {
      DVLOG(2) << "ColorProviderManager: Initializing Color Provider"
               << " - ColorMode: " << ColorModeName(color_mode)
               << " - ContrastMode: " << ContrastModeName(contrast_mode);
      initializer_.Run(provider.get(), color_mode, contrast_mode);
    }

    iter = color_providers_.emplace(key, std::move(provider)).first;
  }
  ColorProvider* provider = iter->second.get();
  DCHECK(provider);
  return provider;
}

}  // namespace ui
