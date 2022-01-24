// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_MANAGER_H_
#define UI_COLOR_COLOR_PROVIDER_MANAGER_H_

#include <memory>
#include <tuple>

#include "base/callback.h"
#include "base/callback_list.h"
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
  struct Key;

  enum class ColorMode {
    kLight,
    kDark,
  };
  enum class ContrastMode {
    kNormal,
    kHigh,
  };
  enum class SystemTheme {
    kDefault,
    kCustom,
  };

  // Threadsafe not because ColorProviderManager requires it but because a
  // concrete subclass does.
  class InitializerSupplier
      : public base::RefCountedThreadSafe<InitializerSupplier> {
   public:
    // Adds any mixers necessary to represent this supplier.
    virtual void AddColorMixers(ColorProvider* provider,
                                const Key& key) const = 0;

   protected:
    virtual ~InitializerSupplier() = default;

   private:
    friend class base::RefCountedThreadSafe<InitializerSupplier>;
  };

  struct COMPONENT_EXPORT(COLOR) Key {
    Key();  // For test convenience.
    Key(ColorMode color_mode,
        ContrastMode contrast_mode,
        SystemTheme system_theme,
        scoped_refptr<InitializerSupplier> custom_theme);
    Key(const Key&);
    Key& operator=(const Key&);
    ~Key();
    ColorMode color_mode;
    ContrastMode contrast_mode;
    SystemTheme system_theme;
    scoped_refptr<InitializerSupplier> custom_theme;

    bool operator<(const Key& other) const {
      return std::make_tuple(color_mode, contrast_mode, system_theme,
                             custom_theme) <
             std::make_tuple(other.color_mode, other.contrast_mode,
                             other.system_theme, other.custom_theme);
    }
  };

  using ColorProviderInitializerList =
      base::RepeatingCallbackList<void(ColorProvider*, const Key&)>;

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
  ColorProvider* GetColorProviderFor(Key key);

 protected:
  ColorProviderManager();
  virtual ~ColorProviderManager();

 private:
  // Holds the chain of ColorProvider initializer callbacks.
  std::unique_ptr<ColorProviderInitializerList> initializer_list_;

  // Holds the subscriptions for initializers in the `initializer_list_`.
  std::vector<base::CallbackListSubscription> initializer_subscriptions_;

  base::flat_map<Key, std::unique_ptr<ColorProvider>> color_providers_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_MANAGER_H_
