// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_type.h"

#include "base/notreached.h"

namespace ui {

base::StringPiece LayerTypeToString(LayerType type) {
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
  NOTREACHED();
  return {};
}

}  // namespace ui
