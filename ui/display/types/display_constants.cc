// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_constants.h"

#include "base/notreached.h"

namespace display {

std::string DisplayConnectionTypeString(DisplayConnectionType type) {
  switch (type) {
    case DISPLAY_CONNECTION_TYPE_NONE:
      return "none";
    case DISPLAY_CONNECTION_TYPE_UNKNOWN:
      return "unknown";
    case DISPLAY_CONNECTION_TYPE_INTERNAL:
      return "internal";
    case DISPLAY_CONNECTION_TYPE_VGA:
      return "vga";
    case DISPLAY_CONNECTION_TYPE_HDMI:
      return "hdmi";
    case DISPLAY_CONNECTION_TYPE_DVI:
      return "dvi";
    case DISPLAY_CONNECTION_TYPE_DISPLAYPORT:
      return "dp";
    case DISPLAY_CONNECTION_TYPE_NETWORK:
      return "network";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace display
