// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_manager.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "ui/color/color_metrics.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"

#if !BUILDFLAG(IS_ANDROID)
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
              "std::optional, nothing more.");

std::optional<GlobalManager>& GetGlobalManager() {
  static base::NoDestructor<std::optional<GlobalManager>> manager;
  return *manager;
}

}  // namespace

ColorProviderManager::ColorProviderManager() {
  ResetColorProviderInitializerList();
}

ColorProviderManager::~ColorProviderManager() = default;

// static
ColorProviderManager& ColorProviderManager::Get() {
  std::optional<GlobalManager>& manager = GetGlobalManager();
  if (!manager.has_value()) {
    manager.emplace();
#if !BUILDFLAG(IS_ANDROID)
    manager.value().AppendColorProviderInitializer(
        base::BindRepeating(AddColorMixers));
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  return manager.value();
}

// static
ColorProviderManager& ColorProviderManager::GetForTesting() {
  std::optional<GlobalManager>& manager = GetGlobalManager();
  if (!manager.has_value())
    manager.emplace();
  return manager.value();
}

// static
void ColorProviderManager::ResetForTesting() {
  GetGlobalManager().reset();
}

void ColorProviderManager::ResetColorProviderInitializerList() {
  ResetColorProviderCache();
  initializer_list_ = std::make_unique<ColorProviderInitializerList>();
  initializer_subscriptions_.clear();
}

void ColorProviderManager::ResetColorProviderCache() {
  if (!color_providers_.empty())
    color_providers_.clear();
}

void ColorProviderManager::AppendColorProviderInitializer(
    ColorProviderInitializerList::CallbackType initializer) {
  DCHECK(initializer_list_);
  ResetColorProviderCache();

  initializer_subscriptions_.push_back(
      initializer_list_->Add(std::move(initializer)));
}

ColorProvider* ColorProviderManager::GetColorProviderFor(ColorProviderKey key) {
  auto iter = color_providers_.find(key);
  if (iter == color_providers_.end()) {
    base::ElapsedTimer timer;

    auto provider = std::make_unique<ColorProvider>();
    DCHECK(initializer_list_);
    if (!initializer_list_->empty())
      initializer_list_->Notify(provider.get(), key);

    RecordTimeSpentInitializingColorProvider(timer.Elapsed());
    ++num_providers_initialized_;

    iter = color_providers_.emplace(key, std::move(provider)).first;
    RecordColorProviderCacheSize(static_cast<int>(color_providers_.size()));
  }
  ColorProvider* provider = iter->second.get();
  DCHECK(provider);
  return provider;
}

}  // namespace ui
