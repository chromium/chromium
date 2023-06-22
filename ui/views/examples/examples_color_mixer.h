// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_MIXER_H_
#define UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_MIXER_H_

#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/views/examples/views_examples_export.h"

namespace views::examples {

VIEWS_EXAMPLES_EXPORT void AddExamplesColorMixers(
    ui::ColorProvider* color_provider,
    const ui::ColorProviderKey& key);

}  // namespace views::examples

#endif  // UI_VIEWS_EXAMPLES_EXAMPLES_COLOR_MIXER_H_
