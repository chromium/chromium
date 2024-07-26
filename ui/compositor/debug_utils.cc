// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/compositor/debug_utils.h"

#include <stddef.h>

#include <iomanip>
#include <optional>
#include <ostream>
#include <string>

#include "base/logging.h"
#include "base/numerics/angle_conversions.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/interpolated_transform.h"

namespace ui {

namespace {

void PrintLayerHierarchyImp(const Layer* layer,
                            int indent,
                            const gfx::Point& mouse_location,
                            std::ostringstream* out,
                            DebugLayerChildCallback child_cb) {
  std::string indent_str(indent, ' ');

  gfx::Point transformed_mouse_location = layer->transform()
                                              .InverseMapPoint(mouse_location)
                                              .value_or(mouse_location);
  const bool mouse_inside_layer_bounds =
      layer->bounds().Contains(transformed_mouse_location);
  const gfx::Point mouse_location_in_layer =
      transformed_mouse_location - layer->bounds().origin().OffsetFromOrigin();

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

  const cc::Layer* cc_layer = layer->cc_layer_for_testing();
  if (cc_layer) {
    // Property trees must be updated in order to get valid render surface
    // reasons.
    if (cc_layer->layer_tree_host() &&
        !cc_layer->layer_tree_host()->property_trees()->needs_rebuild()) {
      cc::RenderSurfaceReason render_surface =
          cc_layer->GetRenderSurfaceReason();
      if (render_surface != cc::RenderSurfaceReason::kNone) {
        *out << " render-surface-reason: "
             << cc::RenderSurfaceReasonToString(render_surface);
      }
    }
  }

  const auto clip_rect = layer->clip_rect();
  if (!clip_rect.IsEmpty()) {
    *out << " clip_rect:" << clip_rect.ToString();
  }

  if (!layer->rounded_corner_radii().IsEmpty()) {
    *out << "\n" << property_indent_str;
    *out << "rounded-corners-radii: "
         << layer->rounded_corner_radii().ToString();
  }

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

  if (!layer->transform().IsIdentity()) {
    if (std::optional<gfx::DecomposedTransform> decomp =
            layer->transform().Decompose()) {
      *out << '\n' << property_indent_str;
      *out << "translation: " << std::fixed << decomp->translate[0];
      *out << ", " << decomp->translate[1];

      *out << '\n' << property_indent_str;
      *out << "rotation: ";
      *out << base::RadToDeg(std::acos(decomp->quaternion.w()) * 2);

      *out << '\n' << property_indent_str;
      *out << "scale: " << decomp->scale[0];
      *out << ", " << decomp->scale[1];
    }
  }

  *out << '\n';

  std::vector<raw_ptr<ui::Layer, VectorExperimental>> children =
      child_cb ? child_cb.Run(layer) : layer->children();
  for (ui::Layer* child : children) {
    PrintLayerHierarchyImp(child, indent + 3, mouse_location_in_layer, out,
                           child_cb);
  }
}

}  // namespace

void PrintLayerHierarchy(const Layer* layer, const gfx::Point& mouse_location) {
  std::ostringstream out;
  PrintLayerHierarchy(layer, mouse_location, &out);
  // Error so logs can be collected from end-users.
  LOG(ERROR) << out.str();
}

void PrintLayerHierarchy(const Layer* layer,
                         const gfx::Point& mouse_location,
                         std::ostringstream* out,
                         DebugLayerChildCallback child_cb) {
  *out << "Layer hierarchy:\n";
  PrintLayerHierarchyImp(layer, 0, mouse_location, out, child_cb);
}

}  // namespace ui
