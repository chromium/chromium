// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_nine_patch.h"

#include "cc/layers/nine_patch_layer.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ui {

LayerNinePatch::LayerNinePatch() : Layer(LAYER_NINE_PATCH) {
  nine_patch_layer_ = cc::NinePatchLayer::Create();
  cc_layer_ = nine_patch_layer_.get();
  InitializeCcLayer();
}

LayerNinePatch::~LayerNinePatch() {
  Destroy();
}

const gfx::Rect& LayerNinePatch::border() const {
  return nine_patch_layer_->border();
}

const gfx::Rect& LayerNinePatch::aperture() const {
  return nine_patch_layer_->aperture();
}

const gfx::Rect& LayerNinePatch::occlusion() const {
  return nine_patch_layer_->occlusion();
}

bool LayerNinePatch::ShouldSchedulePaint() const {
  // LayerNinePatch draws a pre-defined image rather than requesting painted
  // content.
  return false;
}

void LayerNinePatch::UpdateNinePatchLayerImage(const gfx::ImageSkia& image) {
  nine_patch_layer_image_ = image;
  nine_patch_layer_->SetBitmap(
      image.GetRepresentation(device_scale_factor()).GetBitmap());
}

void LayerNinePatch::UpdateNinePatchLayerAperture(
    const gfx::Rect& aperture_in_dip) {
  nine_patch_layer_aperture_ = aperture_in_dip;

  // TODO(danakj): Specifying the aperture in DIPs as integers is not sufficient
  // and means the resulting aperture in pixels will not be exact.
  gfx::Rect aperture_in_pixel = gfx::ToEnclosingRect(
      gfx::ConvertRectToPixels(aperture_in_dip, device_scale_factor()));
  nine_patch_layer_->SetAperture(aperture_in_pixel);
}

void LayerNinePatch::UpdateNinePatchLayerBorder(const gfx::Rect& border) {
  nine_patch_layer_->SetBorder(border);
}

void LayerNinePatch::UpdateNinePatchOcclusion(const gfx::Rect& occlusion) {
  nine_patch_layer_->SetLayerOcclusion(occlusion);
}

void LayerNinePatch::HandleDeviceScaleFactorChange() {
  Layer::HandleDeviceScaleFactorChange();

  if (!nine_patch_layer_image_.isNull()) {
    UpdateNinePatchLayerImage(nine_patch_layer_image_);
  }

  UpdateNinePatchLayerAperture(nine_patch_layer_aperture_);
}

void LayerNinePatch::Reset() {
  nine_patch_layer_ = nullptr;
}

}  // namespace ui
