// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_type.h"

#include <string_view>

#include "base/notreached.h"

namespace ui {

std::string_view LayerTypeToString(LayerType type) {
  switch (type) {
    case LayerType::LAYER_NOT_DRAWN:
      return "not_drawn";
    case LayerType::LAYER_TEXTURED:
      return "textured";
    case LayerType::LAYER_SOLID_COLOR:
      return "solid_color";
    case LayerType::LAYER_NINE_PATCH:
      return "nine_patch";
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

}  // namespace ui
