// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_MANAGER_H_
#define UI_COLOR_COLOR_PROVIDER_MANAGER_H_

#include <memory>
#include <tuple>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"

namespace ui {

class ColorProvider;

// Manages and provides color providers.
//
// In most cases, use ColorProviderManager::Get() to obtain an instance to the
// manager and then call GetColorProviderFor() to get a ColorProvider. It is not
// necessary to construct a ColorProviderManager manually.
class COMPONENT_EXPORT(COLOR) ColorProviderManager {
 public:
  enum class ColorMode {
    kLight,
    kDark,
  };
  enum class ContrastMode {
    kNormal,
    kHigh,
  };
  using ColorProviderKey = std::tuple<ColorMode, ContrastMode>;
  using ColorProviderInitializer =
      base::RepeatingCallback<void(ColorProvider*, ColorMode, ContrastMode)>;

  ColorProviderManager(const ColorProviderManager&) = delete;
  ColorProviderManager& operator=(const ColorProviderManager&) = delete;

  static ColorProviderManager& Get();
  static ColorProviderManager& GetForTesting();
  static void ResetForTesting();

  // Sets the initializer for all ColorProviders returned from
  // GetColorProviderFor().
  void SetColorProviderInitializer(ColorProviderInitializer initializer);

  // Returns a color provider for |key|, creating one if necessary.
  ColorProvider* GetColorProviderFor(ColorProviderKey key);

 protected:
  ColorProviderManager();
  virtual ~ColorProviderManager();

 private:
  ColorProviderInitializer initializer_;
  base::flat_map<ColorProviderKey, std::unique_ptr<ColorProvider>>
      color_providers_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_MANAGER_H_
