// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_key.h"

#include <utility>

namespace ui {

ColorProviderKey::InitializerSupplier::InitializerSupplier() = default;

ColorProviderKey::InitializerSupplier::~InitializerSupplier() = default;

ColorProviderKey::ThemeInitializerSupplier::ThemeInitializerSupplier(
    ThemeType theme_type)
    : theme_type_(theme_type) {}

ColorProviderKey::ColorProviderKey() = default;

ColorProviderKey::ColorProviderKey(const ColorProviderKey&) = default;

ColorProviderKey& ColorProviderKey::operator=(const ColorProviderKey&) =
    default;

ColorProviderKey::ColorProviderKey(ColorProviderKey&&) = default;

ColorProviderKey& ColorProviderKey::operator=(ColorProviderKey&&) = default;

ColorProviderKey::~ColorProviderKey() = default;

}  // namespace ui
