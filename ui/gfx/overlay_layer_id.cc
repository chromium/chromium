// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_layer_id.h"

#include <sstream>
#include <variant>

#include "base/check_op.h"
#include "base/numerics/byte_conversions.h"

namespace gfx {

OverlayLayerId::OverlayLayerId() : impl_(VizInternalId::kOsCompositorRoot) {}

OverlayLayerId::OverlayLayerId(const NamespaceId& layer_namespace_id,
                               uint32_t layer_id,
                               uint32_t sqs_z_order)
    : impl_(RendererLayer{
          .layer_namespace_id = layer_namespace_id,
          .layer_id = layer_id,
          .sqs_z_order = sqs_z_order,
      }) {
  // See: `viz::FrameSinkId::is_valid()`.
  CHECK(layer_namespace_id.first != 0 || layer_namespace_id.second != 0);
}

OverlayLayerId::~OverlayLayerId() = default;

// static
OverlayLayerId OverlayLayerId::MakeVizInternal(VizInternalId layer_id) {
  OverlayLayerId overlay_layer_id;
  overlay_layer_id.impl_ = {layer_id};
  return overlay_layer_id;
}

// static
OverlayLayerId OverlayLayerId::MakeVizInternalRenderPass(
    RenderPassId render_pass_id) {
  OverlayLayerId overlay_layer_id;
  overlay_layer_id.impl_ = std::array<uint8_t, sizeof(RenderPassId)>(
      base::U64ToNativeEndian(render_pass_id.value()));
  return overlay_layer_id;
}

// static
OverlayLayerId OverlayLayerId::MakeForTesting(uint32_t layer_id) {
  return OverlayLayerId({1, 1}, layer_id);
}

OverlayLayerId OverlayLayerId::MakeForChildOfSharedQuadStateLayer(
    uint32_t non_zero_z_order) const {
  CHECK_NE(0u, non_zero_z_order);
  CHECK_EQ(0u, this->z_order_);
  OverlayLayerId child_id = *this;
  child_id.z_order_ = non_zero_z_order;
  return child_id;
}

std::optional<OverlayLayerId::SharedQuadStateLayerId>
OverlayLayerId::shared_quad_state_layer_id() const {
  if (const auto* renderer_layer = std::get_if<RendererLayer>(&impl_)) {
    return std::make_optional<OverlayLayerId::SharedQuadStateLayerId>(
        renderer_layer->layer_namespace_id, renderer_layer->layer_id,
        renderer_layer->sqs_z_order);
  }
  return std::nullopt;
}

std::string OverlayLayerId::ToString() const {
  std::stringstream out;

  std::visit(
      [&](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, VizInternalId>) {
          switch (arg) {
            case VizInternalId::kOsCompositorRoot:
              out << "OsCompositorRoot";
              break;
            case VizInternalId::kDelegatedInkTrail:
              out << "DelegatedInkTrail";
              break;
            case VizInternalId::kBackgroundColorLayer:
              out << "kBackgroundColorLayer";
              break;
          }
        }

        if constexpr (std::is_same_v<
                          T, std::array<uint8_t, sizeof(RenderPassId)>>) {
          out << "RenderPass(" << base::U64FromNativeEndian(arg) << ")";
        }

        if constexpr (std::is_same_v<T, RendererLayer>) {
          out << arg.layer_namespace_id.first << ":"
              << arg.layer_namespace_id.second << ":" << arg.layer_id << "."
              << arg.sqs_z_order;
        }
      },
      impl_);

  if (z_order_) {
    out << "(" << z_order_ << ")";
  }

  return out.str();
}

}  // namespace gfx
