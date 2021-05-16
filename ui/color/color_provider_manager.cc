// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include <algorithm>

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"

#if !defined(OS_ANDROID)
#include "ui/color/color_mixers.h"
#endif

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
              "absl::optional, nothing more.");

absl::optional<GlobalManager>& GetGlobalManager() {
  static base::NoDestructor<absl::optional<GlobalManager>> manager;
  return *manager;
}

}  // namespace

ColorProviderManager::ColorProviderManager() {
  ResetColorProviderInitializerList();
}

ColorProviderManager::~ColorProviderManager() = default;

// static
ColorProviderManager& ColorProviderManager::Get() {
  absl::optional<GlobalManager>& manager = GetGlobalManager();
  if (!manager.has_value()) {
    manager.emplace();
#if !defined(OS_ANDROID)
    manager.value().AppendColorProviderInitializer(base::BindRepeating(
        [](ColorProvider* provider, ColorProviderManager::ColorMode color_mode,
           ColorProviderManager::ContrastMode contrast_mode,
           ColorProviderManager::ThemeName theme_name) {
          const bool dark_mode =
              color_mode == ColorProviderManager::ColorMode::kDark;
          const bool high_contrast =
              contrast_mode == ColorProviderManager::ContrastMode::kHigh;
          ui::AddCoreDefaultColorMixer(provider, dark_mode, high_contrast);
          ui::AddNativeCoreColorMixer(provider, dark_mode, high_contrast);
          ui::AddUiColorMixer(provider, dark_mode, high_contrast);
          ui::AddNativeUiColorMixer(provider, dark_mode, high_contrast);
          ui::AddNativePostprocessingMixer(provider);
        }));
#endif  // !defined(OS_ANDROID)
  }

  return manager.value();
}

// static
ColorProviderManager& ColorProviderManager::GetForTesting() {
  absl::optional<GlobalManager>& manager = GetGlobalManager();
  if (!manager.has_value())
    manager.emplace();
  return manager.value();
}

// static
void ColorProviderManager::ResetForTesting() {
  GetGlobalManager().reset();
}

void ColorProviderManager::ResetColorProviderInitializerList() {
  if (!color_providers_.empty())
    color_providers_.clear();
  initializer_list_ = std::make_unique<ColorProviderInitializerList>();
  initializer_subscriptions_.clear();
}

void ColorProviderManager::AppendColorProviderInitializer(
    ColorProviderInitializerList::CallbackType initializer) {
  DCHECK(initializer_list_);
  if (!color_providers_.empty())
    color_providers_.clear();

  initializer_subscriptions_.push_back(
      initializer_list_->Add(std::move(initializer)));
}

ColorProvider* ColorProviderManager::GetColorProviderFor(ColorProviderKey key) {
  auto iter = color_providers_.find(key);
  if (iter == color_providers_.end()) {
    auto provider = std::make_unique<ColorProvider>();
    DCHECK(initializer_list_);
    if (!initializer_list_->empty()) {
      DVLOG(2) << "ColorProviderManager: Initializing Color Provider"
               << " - ColorMode: " << ColorModeName(std::get<ColorMode>(key))
               << " - ContrastMode: "
               << ContrastModeName(std::get<ContrastMode>(key))
               << " - ThemeName: " << std::get<ThemeName>(key);
      initializer_list_->Notify(provider.get(), std::get<ColorMode>(key),
                                std::get<ContrastMode>(key),
                                std::get<ThemeName>(key));
    }

    iter = color_providers_.emplace(key, std::move(provider)).first;
  }
  ColorProvider* provider = iter->second.get();
  DCHECK(provider);
  return provider;
}

}  // namespace ui
