// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_source_observer.h"

namespace ui {

ColorProviderSourceObserver::ColorProviderSourceObserver() = default;

ColorProviderSourceObserver::~ColorProviderSourceObserver() = default;

void ColorProviderSourceObserver::OnColorProviderSourceDestroying() {
  color_provider_source_observation_.Reset();
}

void ColorProviderSourceObserver::Observe(ColorProviderSource* source) {
  if (color_provider_source_observation_.IsObservingSource(source))
    return;

  color_provider_source_observation_.Reset();
  color_provider_source_observation_.Observe(source);
}

}  // namespace ui
