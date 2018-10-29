// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_
#define UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/geometry/insets.h"

namespace ui {
class Layer;
class LayerOwner;
}

namespace aura {

class Window;

// Used by WindowPortMus when it is embedding a client. Responsible for setting
// up layers containing content from the client, parenting them to the window's
// layer, and updating them when the client submits new surfaces.
class AURA_EXPORT ClientSurfaceEmbedder {
 public:
  // TODO(fsamuel): Insets might differ when the window is maximized. We should
  // deal with that case as well.
  ClientSurfaceEmbedder(Window* window,
                        bool inject_gutter,
                        const gfx::Insets& client_area_insets);
  ~ClientSurfaceEmbedder();

  // Updates the clip layer and primary SurfaceId of the surface layer based
  // on the provided |surface_id|.
  void SetSurfaceId(const viz::SurfaceId& surface_id);

  bool HasPrimarySurfaceId() const;

  // Sets the fallback SurfaceInfo of the surface layer. The clip layer is not
  // updated.
  void SetFallbackSurfaceInfo(const viz::SurfaceInfo& surface_info);

  void SetClientAreaInsets(const gfx::Insets& client_area_insets);
  const gfx::Insets& client_area_insets() const { return client_area_insets_; }

  // Update the surface layer size and the right and bottom gutter layers for
  // the current window size.
  void UpdateSizeAndGutters();

  ui::Layer* RightGutterForTesting();

  ui::Layer* BottomGutterForTesting();

  const viz::SurfaceId& GetSurfaceIdForTesting() const;

 private:
  // The window which embeds the client.
  Window* window_;

  // Contains the client's content. This (and other Layers) are wrapped in a
  // LayerOwner so that animations clone the layer.
  std::unique_ptr<ui::LayerOwner> surface_layer_owner_;

  // Information describing the currently set fallback surface.
  viz::SurfaceInfo fallback_surface_info_;

  // Used for showing a gutter when the content is not available.
  std::unique_ptr<ui::LayerOwner> right_gutter_owner_;
  std::unique_ptr<ui::LayerOwner> bottom_gutter_owner_;

  bool inject_gutter_;
  gfx::Insets client_area_insets_;

  DISALLOW_COPY_AND_ASSIGN(ClientSurfaceEmbedder);
};

}  // namespace aura

#endif  // UI_AURA_MUS_CLIENT_SURFACE_EMBEDDER_H_
