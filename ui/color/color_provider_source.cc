// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source.h"
#include "base/observer_list.h"
#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSource::ColorProviderSource() = default;

ColorProviderSource::~ColorProviderSource() {
  for (auto& observer : observers_)
    observer.OnColorProviderSourceDestroying();
}

void ColorProviderSource::AddObserver(ColorProviderSourceObserver* observer) {
  observers_.AddObserver(observer);
}

void ColorProviderSource::RemoveObserver(
    ColorProviderSourceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ColorProviderSource::NotifyColorProviderChanged() {
  for (auto& observer : observers_)
    observer.OnColorProviderChanged();
}

absl::optional<SkColor> ColorProviderSource::GetUserColor() const {
  return absl::nullopt;
}

}  // namespace ui
