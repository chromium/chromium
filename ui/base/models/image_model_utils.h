// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MODELS_IMAGE_MODEL_UTILS_H_
#define UI_BASE_MODELS_IMAGE_MODEL_UTILS_H_

#include "base/component_export.h"
#include "ui/base/models/image_model.h"

namespace ui {
class ColorProvider;

COMPONENT_EXPORT(UI_BASE)
ImageModel GetDefaultDisabledIconFromImageModel(ImageModel icon,
                                                ColorProvider* = nullptr);

}  // namespace ui

#endif  // UI_BASE_MODELS_IMAGE_MODEL_UTILS_H_
