// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_ACCELERATED_WIDGET_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_ACCELERATED_WIDGET_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/gfx/mojom/accelerated_widget.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::AcceleratedWidgetDataView,
                    gfx::AcceleratedWidget> {
  static uint64_t widget(gfx::AcceleratedWidget widget) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return reinterpret_cast<uint64_t>(widget);
#elif BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC)
    return static_cast<uint64_t>(widget);
#else
    NOTREACHED_IN_MIGRATION();
    return 0;
#endif
  }

  static bool Read(gfx::mojom::AcceleratedWidgetDataView data,
                   gfx::AcceleratedWidget* out) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    *out = reinterpret_cast<gfx::AcceleratedWidget>(data.widget());
    return true;
#elif BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_MAC)
    *out = static_cast<gfx::AcceleratedWidget>(data.widget());
    return true;
#else
    NOTREACHED_IN_MIGRATION();
    return false;
#endif
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_ACCELERATED_WIDGET_MOJOM_TRAITS_H_
