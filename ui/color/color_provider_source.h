// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_SOURCE_H_
#define UI_COLOR_COLOR_PROVIDER_SOURCE_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"

namespace ui {

class ColorProviderSourceObserver;

// Encapsulates the theme bits that are used to uniquely identify a
// ColorProvider instance (i.e. the bits that comprise the ColorProviderKey).
// Notifies observers when the ColorProvider instance that maps to these theme
// bits changes. References are managed to avoid dangling pointers in the case
// the source or the observer are deleted.
class COMPONENT_EXPORT(COLOR) ColorProviderSource {
 public:
  ColorProviderSource();
  virtual ~ColorProviderSource();

  // Returns the ColorProvider associated with the Key returned by
  // GetColorProviderKey().
  // TODO(tluk): This shouldn't need to be virtual, most overrides simply pass
  // the key to the manager.
  virtual const ColorProvider* GetColorProvider() const = 0;

  void AddObserver(ColorProviderSourceObserver* observer);
  void RemoveObserver(ColorProviderSourceObserver* observer);

  // Should be called by the implementation whenever the ColorProvider supplied
  // in the method `GetColorProvider()` changes.
  void NotifyColorProviderChanged();

  // Gets the ColorMode currently associated with this source.
  ColorProviderKey::ColorMode GetColorMode() const;

  // Gets the ForcedColors state currently associated with this source.
  ColorProviderKey::ForcedColors GetForcedColors() const;

  // Gets the RendererColorMap corresponding to the ColorProvider for the
  // `color_mode` and `forced_colors`.
  virtual RendererColorMap GetRendererColorMap(
      ColorProviderKey::ColorMode color_mode,
      ColorProviderKey::ForcedColors forced_colors) const = 0;

  base::ObserverList<ColorProviderSourceObserver>& observers_for_testing() {
    return observers_;
  }

 protected:
  // Implementations should return the ColorProviderKey associated with
  // this source.
  virtual ColorProviderKey GetColorProviderKey() const = 0;

 private:
  base::ObserverList<ColorProviderSourceObserver> observers_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_SOURCE_H_
