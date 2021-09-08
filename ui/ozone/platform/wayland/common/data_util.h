// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_DATA_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_DATA_UTIL_H_

#include <string>

#include "ui/ozone/public/platform_clipboard.h"

namespace ui {
class OSExchangeData;
}  // namespace ui

namespace wl {

// TODO(crbug.com/1247063): Move into WaylandExchangeDataProvider class.

// Tells if |mime_type| is supported for Drag and Drop operations.
bool IsMimeTypeSupported(const std::string& mime_type);

// Tells if |exchange_data| contains |mime_type| content.
bool ContainsMimeType(const ui::OSExchangeData& exchange_data,
                      const std::string& mime_type);

// Add clipboard |data| content with |mime_type| format to the |exchange_data|.
// |mime_type| is assumed to be supported (See IsMimeTypeSupported for more).
void AddToOSExchangeData(ui::PlatformClipboard::Data data,
                         const std::string& mime_type,
                         ui::OSExchangeData* exchange_data);

// Extract |exchange_data| of type |mime_type| and put it into |buffer|. If such
// mime type is not present, false is returned and |buffer| keeps untouched.
// |mime_type| is assumed to be supported (See IsMimeTypeSupported for more).
bool ExtractOSExchangeData(const ui::OSExchangeData& exchange_data,
                           const std::string& mime_type,
                           std::string* buffer);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_DATA_UTIL_H_
