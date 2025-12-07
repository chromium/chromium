// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_UTILS_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_UTILS_H_

#include "ui/ozone/public/native_pixmap_usage.h"

namespace gfx {
enum class BufferUsage;
}

namespace ui {

NativePixmapUsageSet COMPONENT_EXPORT(NATIVE_PIXMAP_USAGE)
    BufferUsageToNativePixmapUsage(gfx::BufferUsage usage);

std::string COMPONENT_EXPORT(NATIVE_PIXMAP_USAGE)
    NativePixmapUsageToString(NativePixmapUsageSet usage);

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_USAGE_UTILS_H_
