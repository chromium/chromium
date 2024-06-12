// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/image_model_utils.h"

#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ui {

// TODO(dpenning) consider parameterizing color choices for disabled defaults.
ImageModel GetDefaultDisabledIconFromImageModel(ImageModel icon_model,
                                                ColorProvider* color_provider) {
  if (icon_model.IsEmpty()) {
    return icon_model;
  }

  if (icon_model.IsVectorIcon()) {
    if (!color_provider) {
      return icon_model;
    }
    VectorIconModel vector_model = icon_model.GetVectorIcon();
    const gfx::VectorIcon* vector_icon = vector_model.vector_icon();
    return ImageModel::FromVectorIcon(
        *vector_icon, color_provider->GetColor(kColorIconDisabled),
        vector_model.icon_size());
  }

  if (icon_model.IsImage()) {
    gfx::Image image = icon_model.GetImage();
    if (image.IsEmpty()) {
      return icon_model;
    }

    return ImageModel::FromImage(
        gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(
            *(image.ToImageSkia()), gfx::kDisabledControlAlpha)));
  }

  return icon_model;
}

}  // namespace ui
