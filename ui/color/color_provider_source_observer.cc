// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSourceObserver::ColorProviderSourceObserver() = default;

ColorProviderSourceObserver::~ColorProviderSourceObserver() = default;

void ColorProviderSourceObserver::OnColorProviderSourceDestroying() {
  source_ = nullptr;
  color_provider_source_observation_.Reset();
}

const ui::ColorProviderSource*
ColorProviderSourceObserver::GetColorProviderSourceForTesting() const {
  return GetColorProviderSource();
}

void ColorProviderSourceObserver::Observe(ColorProviderSource* source) {
  if (source && color_provider_source_observation_.IsObservingSource(source))
    return;

  color_provider_source_observation_.Reset();
  source_ = source;
  if (!source_)
    return;

  color_provider_source_observation_.Observe(source);
}

const ui::ColorProviderSource*
ColorProviderSourceObserver::GetColorProviderSource() const {
  return source_;
}

}  // namespace ui
