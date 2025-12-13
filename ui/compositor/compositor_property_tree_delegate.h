// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_
#define UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree_delegate.h"
#include "cc/trees/property_tree_layer_tree_delegate.h"
#include "ui/compositor/compositor_export.h"

namespace gfx {
class Vector2dF;
class Transform;
}  // namespace gfx

namespace cc {
class LayerTreeHost;
struct ViewportPropertyIds;
}

namespace ui {

class Compositor;

// TODO(crbug.com/389771428): This class exists to gradually move the
// Compositor from using the cc::Compositor in legacy layer tree mode
// to using it in property tree / layer list mode. This class should be
// removed once that migration is done and we can just use the
// cc::Compositor's default logic for property tree / layer list mode.
class COMPOSITOR_EXPORT CompositorPropertyTreeDelegate
    : public cc::PropertyTreeLayerTreeDelegate {
 public:
  // This class exists for testing purposes, so that tests can probe the
  // property trees once they've been updated.
  class Observer {
   public:
    // Called when UpdatePropertyTreesIfNeeded is finished.
    virtual void OnUpdateCalled(cc::LayerTreeHost* host) = 0;

   protected:
    virtual ~Observer() = default;
  };

  CompositorPropertyTreeDelegate() = default;
  CompositorPropertyTreeDelegate(const CompositorPropertyTreeDelegate&) =
      delete;
  CompositorPropertyTreeDelegate& operator=(
      const CompositorPropertyTreeDelegate&) = delete;
  ~CompositorPropertyTreeDelegate() override = default;

  void set_compositor(Compositor* compositor) { compositor_ = compositor; }

  void SetObserverForTesting(Observer*);

  // PropertyTreeDelegate overrides.
  void UpdatePropertyTreesIfNeeded() override;
  void UpdateScrollOffsetFromImpl(
      const cc::ElementId& id,
      const gfx::Vector2dF& delta,
      cc::ScrollSourceType type,
      const std::optional<cc::TargetSnapAreaElementIds>& snap_target_ids)
      override;
  void OnAnimateLayers() override;
  void RegisterViewportPropertyIds(const cc::ViewportPropertyIds& ids) override;
  void OnUnregisterElement(cc::ElementId id) override;
  bool IsElementInPropertyTrees(cc::ElementId element_id,
                                cc::ElementListType list_type) const override;
  void OnElementFilterMutated(cc::ElementId element_id,
                              cc::ElementListType list_type,
                              const cc::FilterOperations& filters) override;
  void OnElementBackdropFilterMutated(
      cc::ElementId element_id,
      cc::ElementListType list_type,
      const cc::FilterOperations& backdrop_filters) override;
  void OnElementOpacityMutated(cc::ElementId element_id,
                               cc::ElementListType list_type,
                               float opacity) override;
  void OnElementTransformMutated(cc::ElementId element_id,
                                 cc::ElementListType list_type,
                                 const gfx::Transform& transform) override;

 private:
  raw_ptr<Compositor> compositor_ = nullptr;
  raw_ptr<Observer> observer_ = nullptr;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_PROPERTY_TREE_DELEGATE_H_
