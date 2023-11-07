// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSourceObserver::ColorProviderSourceObserver(
    ColorProviderSource* source)
    : source_(source) {
  if (source) {
    color_provider_source_observation_.Observe(source);
  }
}

ColorProviderSourceObserver::~ColorProviderSourceObserver() = default;

void ColorProviderSourceObserver::OnColorProviderSourceDestroying() {
  Observe(nullptr);
}

const ui::ColorProviderSource*
ColorProviderSourceObserver::GetColorProviderSourceForTesting() const {
  return GetColorProviderSource();
}

void ColorProviderSourceObserver::Observe(ColorProviderSource* source) {
  if ((source &&
       color_provider_source_observation_.IsObservingSource(source)) ||
      (!source && source_ == nullptr)) {
    return;
  }

  color_provider_source_observation_.Reset();
  source_ = source;

  if (source_)
    color_provider_source_observation_.Observe(source);

  // Notify both when a new source is observed and when an observation is reset
  // (i.e. when Observe() is called with nullptr).
  OnColorProviderChanged();
}

const ui::ColorProviderSource*
ColorProviderSourceObserver::GetColorProviderSource() const {
  return source_;
}

}  // namespace ui
