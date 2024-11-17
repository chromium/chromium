// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source.h"

#include "base/observer_list.h"
#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSource::ColorProviderSource() = default;

ColorProviderSource::~ColorProviderSource() {
  observers_.Notify(
      &ColorProviderSourceObserver::OnColorProviderSourceDestroying);
}

void ColorProviderSource::AddObserver(ColorProviderSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void ColorProviderSource::RemoveObserver(
    ColorProviderSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ColorProviderSource::NotifyColorProviderChanged() {
  observers_.Notify(&ColorProviderSourceObserver::OnColorProviderChanged);
}

ui::ColorProviderKey::ColorMode ColorProviderSource::GetColorMode() const {
  return GetColorProviderKey().color_mode;
}

ui::ColorProviderKey::ForcedColors ColorProviderSource::GetForcedColors()
    const {
  return GetColorProviderKey().forced_colors;
}

}  // namespace ui
