// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_LAYER_ID_H_
#define UI_GFX_OVERLAY_LAYER_ID_H_

#include <stdint.h>

#include <compare>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/component_export.h"
#include "base/types/id_type.h"

namespace viz {
class AggregatedRenderPass;
}

namespace gfx {

// An identifier for an overlay layer that is unique within a frame. Should be
// stable across frames if OS compositor subtree reuse is desired.
class COMPONENT_EXPORT(GFX) OverlayLayerId {
 public:
  using NamespaceId = std::pair<uint32_t, uint32_t>;
  // Copy of AggregatedRenderPassId, since we cannot depend on viz from here.
  using RenderPassId = base::IdTypeU64<viz::AggregatedRenderPass>;

  // The default constructed instance represents the implicit root node of the
  // OS compositor tree.
  OverlayLayerId();
  // `layer_namespace_id` must be non-zero.
  OverlayLayerId(const NamespaceId& layer_namespace_id, uint32_t layer_id);
  ~OverlayLayerId();

  // The set of named layers that Viz make create that do not (or cannot) map
  // back to anything in any compositor frames.
  enum class VizInternalId : uint32_t {
    kOsCompositorRoot,
    kDelegatedInkTrail,
  };

  // Create a named `OverlayLayerId` that is namespaced for internal viz use.
  static OverlayLayerId MakeVizInternal(VizInternalId layer_id);

  // Create an `OverlayLayerId` that is namespaced for internal viz use and
  // identifies a render pass ID.
  static OverlayLayerId MakeVizInternalRenderPass(RenderPassId render_pass_id);

  // Create an `OverlayLayerId` with a `layer_namespace_id` of {1,1}.
  static OverlayLayerId MakeForTesting(uint32_t layer_id);

  auto operator<=>(const OverlayLayerId&) const = default;

  std::string ToString() const;

  // TODO(crbug.com/324460866): remove when we remove partial delegation.
  using SharedQuadStateLayerId = std::tuple<NamespaceId, uint32_t>;
  std::optional<SharedQuadStateLayerId> shared_quad_state_layer_id() const;

 private:
  // Represents a layer ID that is constructed from a `cc::Layer` and the frame
  // sink it came from.
  struct RendererLayer {
    // See `viz::SharedQuadState::layer_namespace_id`.
    // A value of 0 is reserved for Viz use.
    NamespaceId layer_namespace_id;
    // See `viz::SharedQuadState::layer_id`.
    uint32_t layer_id = 0;

    auto operator<=>(const RendererLayer&) const = default;
  };

  // Note we store the `RenderPassId` as a byte array to not force `impl_` (and
  // its discriminant) to be aligned to 8 bytes.
  std::variant<VizInternalId,
               std::array<uint8_t, sizeof(RenderPassId)>,
               RendererLayer>
      impl_;
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_LAYER_ID_H_
