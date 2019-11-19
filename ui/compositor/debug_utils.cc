// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/debug_utils.h"

#include <stddef.h>

#include <iomanip>
#include <ostream>
#include <string>

#include "base/logging.h"
#include "base/numerics/math_constants.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform.h"

namespace ui {

namespace {

void PrintLayerHierarchyImp(const Layer* layer,
                            int indent,
                            gfx::Point mouse_location,
                            std::ostringstream* out) {
  std::string indent_str(indent, ' ');

  layer->transform().TransformPointReverse(&mouse_location);
  bool mouse_inside_layer_bounds = layer->bounds().Contains(mouse_location);
  mouse_location.Offset(-layer->bounds().x(), -layer->bounds().y());

  *out << indent_str;
  if (mouse_inside_layer_bounds)
    *out << '*';
  else
    *out << ' ';

  *out << layer->name() << ' ' << layer;

  switch (layer->type()) {
    case ui::LAYER_NOT_DRAWN:
      *out << " not_drawn";
      break;
    case ui::LAYER_TEXTURED:
      *out << " textured";
      if (layer->fills_bounds_opaquely())
        *out << " opaque";
      break;
    case ui::LAYER_SOLID_COLOR:
      *out << " solid";
      break;
    case ui::LAYER_NINE_PATCH:
      *out << " nine_patch";
      break;
  }

  if (!layer->visible())
    *out << " !visible";

  std::string property_indent_str(indent+3, ' ');
  *out << '\n' << property_indent_str;
  *out << "bounds: " << layer->bounds().x() << ',' << layer->bounds().y();
  *out << ' ' << layer->bounds().width() << 'x' << layer->bounds().height();
  if (!layer->GetSubpixelOffset().IsZero())
    *out << " " << layer->GetSubpixelOffset().ToString();

  const ui::Layer* mask = const_cast<ui::Layer*>(layer)->layer_mask_layer();

  if (mask) {
    *out << '\n' << property_indent_str;
    *out << "mask layer: " << std::setprecision(2) << mask->bounds().ToString()
         << mask->GetSubpixelOffset().ToString();
  }

  if (layer->opacity() != 1.0f) {
    *out << '\n' << property_indent_str;
    *out << "opacity: " << std::setprecision(2) << layer->opacity();
  }

  gfx::DecomposedTransform decomp;
  if (!layer->transform().IsIdentity() &&
      gfx::DecomposeTransform(&decomp, layer->transform())) {
    *out << '\n' << property_indent_str;
    *out << "translation: " << std::fixed << decomp.translate[0];
    *out << ", " << decomp.translate[1];

    *out << '\n' << property_indent_str;
    *out << "rotation: ";
    *out << std::acos(decomp.quaternion.w()) * 360.0 / base::kPiDouble;

    *out << '\n' << property_indent_str;
    *out << "scale: " << decomp.scale[0];
    *out << ", " << decomp.scale[1];
  }

  *out << '\n';

  for (size_t i = 0, count = layer->children().size(); i < count; ++i) {
    PrintLayerHierarchyImp(
        layer->children()[i], indent + 3, mouse_location, out);
  }
}

}  // namespace

void PrintLayerHierarchy(const Layer* layer, const gfx::Point& mouse_location) {
  std::ostringstream out;
  out << "Layer hierarchy:\n";
  PrintLayerHierarchyImp(layer, 0, mouse_location, &out);
  // Error so logs can be collected from end-users.
  LOG(ERROR) << out.str();
}

}  // namespace ui
