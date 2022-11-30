// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class WaylandExchangeDataProvider final
    : public OSExchangeDataProviderNonBacked {
 public:
  WaylandExchangeDataProvider();
  WaylandExchangeDataProvider(const WaylandExchangeDataProvider&) = delete;
  WaylandExchangeDataProvider& operator=(const WaylandExchangeDataProvider&) =
      delete;
  ~WaylandExchangeDataProvider() override;

  // OSExchangeDataProvider:
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;

  // Builds up the mime types list corresponding to the data formats available
  // for this instance.
  std::vector<std::string> BuildMimeTypesList() const;

  // Extract data of |mime_type| type and append it to |buffer|. If such mime
  // type is not present, false is returned and |buffer| keeps untouched.
  // |mime_type| is assumed to be supported.
  bool ExtractData(const std::string& mime_type, std::string* buffer) const;

  // Add clipboard |data| content with |mime_type| format to |this|. |mime_type|
  // is assumed to be supported (See IsMimeTypeSupported for more).
  void AddData(PlatformClipboard::Data data, const std::string& mime_type);
};

// Tells if |mime_type| is supported for Drag and Drop operations.
bool IsMimeTypeSupported(const std::string& mime_type);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_EXCHANGE_DATA_PROVIDER_H_
