// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_LAYER_ID_H_
#define UI_GFX_OVERLAY_LAYER_ID_H_

#include <stdint.h>

#include <compare>
#include <string>
#include <utility>

#include "base/component_export.h"

namespace gfx {

// A layer ID packing that includes the namespace. Should be stable across
// frames if OS compositor subtree reuse is desired.
// See |SharedQuadState::layer_id| and |SharedQuadState::layer_namespace_id|.
class COMPONENT_EXPORT(GFX) OverlayLayerId {
 public:
  using NamespaceId = std::pair<uint32_t, uint32_t>;

  OverlayLayerId();
  // `layer_namespace_id` must be non-zero.
  OverlayLayerId(const NamespaceId& layer_namespace_id, uint32_t layer_id);
  ~OverlayLayerId();

  // The set of named layers that Viz make create that do not (or cannot) map
  // back to anything in any compositor frames.
  enum class VizInternalId : uint32_t {
    kInvalid = 0,
    kPrimaryPlane,
    kDelegatedInkTrail,
  };

  // Create an `OverlayLayerId` with a zero `layer_namespace_id`, which is
  // reserved for internal viz use.
  // `layer_id` must be non-zero.
  static OverlayLayerId MakeVizInternal(VizInternalId layer_id);

  // Create an `OverlayLayerId` with a `layer_namespace_id` of {1,1}.
  static OverlayLayerId MakeForTesting(uint32_t layer_id);

  auto operator<=>(const OverlayLayerId&) const = default;

  std::string ToString() const;

  // TODO(crbug.com/324460866): remove when we remove partial delegation.
  using SharedQuadStateLayerId = std::pair<NamespaceId, uint32_t>;
  SharedQuadStateLayerId shared_quad_state_layer_id() const {
    return {layer_namespace_id_, layer_id_};
  }

 private:
  // See `viz::SharedQuadState::layer_namespace_id`.
  // A value of 0 is reserved for Viz use.
  NamespaceId layer_namespace_id_;
  // See `viz::SharedQuadState::layer_id`.
  uint32_t layer_id_ = 0;
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_LAYER_ID_H_
