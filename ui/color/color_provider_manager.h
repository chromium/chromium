// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_MANAGER_H_
#define UI_COLOR_COLOR_PROVIDER_MANAGER_H_

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/system_theme.h"
#include "ui/gfx/color_utils.h"

namespace ui {

class ColorProvider;

// Manages and provides color providers.
//
// In most cases, use ColorProviderManager::Get() to obtain an instance to the
// manager and then call GetColorProviderFor() to get a ColorProvider. It is not
// necessary to construct a ColorProviderManager manually.
class COMPONENT_EXPORT(COLOR) ColorProviderManager {
 public:
  using ColorProviderInitializerList =
      base::RepeatingCallbackList<void(ColorProvider*,
                                       const ColorProviderKey&)>;

  ColorProviderManager(const ColorProviderManager&) = delete;
  ColorProviderManager& operator=(const ColorProviderManager&) = delete;

  static ColorProviderManager& Get();
  static ColorProviderManager& GetForTesting();
  static void ResetForTesting();

  // Resets the current `initializer_list_`.
  void ResetColorProviderInitializerList();

  // Clears the ColorProviders stored in `color_providers_`.
  void ResetColorProviderCache();

  // Appends `initializer` to the end of the current `initializer_list_`.
  void AppendColorProviderInitializer(
      ColorProviderInitializerList::CallbackType Initializer);

  // Returns a color provider for |key|, creating one if necessary.
  ColorProvider* GetColorProviderFor(ColorProviderKey key);

  size_t num_providers_initialized() const {
    return num_providers_initialized_;
  }

 protected:
  ColorProviderManager();
  virtual ~ColorProviderManager();

 private:
  // Holds the chain of ColorProvider initializer callbacks.
  std::unique_ptr<ColorProviderInitializerList> initializer_list_;

  // Holds the subscriptions for initializers in the `initializer_list_`.
  std::vector<base::CallbackListSubscription> initializer_subscriptions_;

  base::flat_map<ColorProviderKey, std::unique_ptr<ColorProvider>>
      color_providers_;

  // Tracks the number of ColorProviders constructed and initialized by the
  // manager for metrics purposes.
  size_t num_providers_initialized_ = 0;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_MANAGER_H_
