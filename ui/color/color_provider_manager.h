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
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
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
  struct Key;

  enum class ColorMode {
    kLight,
    kDark,
  };
  enum class ContrastMode {
    kNormal,
    kHigh,
  };
  enum class ElevationMode {
    kLow,
    kHigh,
  };
  enum class SystemTheme {
    // Classic theme, used in the default or users' chosen theme.
    kDefault,
    // Custom theme that follow the system style,
    // currently used only when GTK theme is on.
    kCustom,
  };
  enum class FrameType {
    // Chrome renders the browser frame.
    kChromium,
    // Native system renders the browser frame. Currently GTK only.
    kNative,
  };

  class COMPONENT_EXPORT(COLOR) InitializerSupplier {
   public:
    InitializerSupplier();
    // Adds any mixers necessary to represent this supplier.
    virtual void AddColorMixers(ColorProvider* provider,
                                const Key& key) const = 0;

    base::WeakPtr<InitializerSupplier> get_weak_ref() {
      return weak_factory_.GetWeakPtr();
    }

   protected:
    virtual ~InitializerSupplier();

   private:
    base::WeakPtrFactory<InitializerSupplier> weak_factory_{this};
  };

  // Threadsafe not because ColorProviderManager requires it but because a
  // concrete subclass does.
  class COMPONENT_EXPORT(COLOR) ThemeInitializerSupplier
      : public InitializerSupplier,
        public base::RefCountedThreadSafe<ThemeInitializerSupplier> {
   public:
    virtual bool GetColor(int id, SkColor* color) const = 0;
    virtual bool GetTint(int id, color_utils::HSL* hsl) const = 0;
    virtual bool GetDisplayProperty(int id, int* result) const = 0;
    virtual bool HasCustomImage(int id) const = 0;

   protected:
    ~ThemeInitializerSupplier() override = default;

   private:
    friend class base::RefCountedThreadSafe<ThemeInitializerSupplier>;
  };

  struct COMPONENT_EXPORT(COLOR) Key {
    Key();  // For test convenience.
    Key(ColorMode color_mode,
        ContrastMode contrast_mode,
        SystemTheme system_theme,
        FrameType frame_type,
        scoped_refptr<ThemeInitializerSupplier> custom_theme);
    Key(const Key&);
    Key& operator=(const Key&);
    ~Key();
    ColorMode color_mode;
    ContrastMode contrast_mode;
    ElevationMode elevation_mode;
    SystemTheme system_theme;
    FrameType frame_type;
    scoped_refptr<ThemeInitializerSupplier> custom_theme;
    base::WeakPtr<InitializerSupplier> app_controller;

    bool operator<(const Key& other) const {
      const auto lhs =
          std::make_tuple(color_mode, contrast_mode, elevation_mode,
                          system_theme, frame_type, custom_theme);
      const auto rhs = std::make_tuple(other.color_mode, other.contrast_mode,
                                       other.elevation_mode, other.system_theme,
                                       other.frame_type, other.custom_theme);
      if (lhs == rhs)
        return app_controller.get() < other.app_controller.get();
      return lhs < rhs;
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
