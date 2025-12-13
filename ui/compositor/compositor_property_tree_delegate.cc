// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_property_tree_delegate.h"

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "cc/input/scroll_snap_data.h"
#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/mutator_host_client.h"
#include "cc/trees/property_tree_builder.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

void CompositorPropertyTreeDelegate::UpdatePropertyTreesIfNeeded() {
  // Note that this code is identical to the base method, except that
  // we update the method names in the trace events to be more accurate.
  // TODO(crbug.com/389771428): Implement this w/ layer lists.

  // Note: this leaves the _BuiltPropertyTrees trace event guarded by
  // cc.debug to be consistent with the default implementations in cc and
  // so we don't need to set two different logging options to get the output.
  TRACE_EVENT0("ui",
               "CompositorPropertyTreeDelegate::UpdatePropertyTreesIfNeeded");
  cc::PropertyTreeBuilder::BuildPropertyTrees(host());

  DCHECK(compositor_);
  compositor_->CheckPropertyTrees();

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "CompositorPropertyTreeDelegate::"
                       "UpdatePropertyTreesIfNeeded_BuiltPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host()->property_trees()->AsTracedValue());
  if (observer_) {
    observer_->OnUpdateCalled(host());
  }
}

void CompositorPropertyTreeDelegate::UpdateScrollOffsetFromImpl(
    const cc::ElementId& id,
    const gfx::Vector2dF& delta,
    cc::ScrollSourceType type,
    const std::optional<cc::TargetSnapAreaElementIds>& snap_target_ids) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::UpdateScrollOffsetFromImpl(
      id, delta, type, snap_target_ids);
}

void CompositorPropertyTreeDelegate::OnAnimateLayers() {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnAnimateLayers();
}

void CompositorPropertyTreeDelegate::RegisterViewportPropertyIds(
    const cc::ViewportPropertyIds& ids) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now, just
  // call the base class implementation to ensure that we don't get out of date.
  cc::PropertyTreeLayerTreeDelegate::RegisterViewportPropertyIds(ids);
}

void CompositorPropertyTreeDelegate::OnUnregisterElement(
    cc::ElementId element_id) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnUnregisterElement(element_id);
}

bool CompositorPropertyTreeDelegate::IsElementInPropertyTrees(
    cc::ElementId element_id,
    cc::ElementListType list_type) const {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  return cc::PropertyTreeLayerTreeDelegate::IsElementInPropertyTrees(element_id,
                                                                     list_type);
}

void CompositorPropertyTreeDelegate::OnElementFilterMutated(
    cc::ElementId element_id,
    cc::ElementListType list_type,
    const cc::FilterOperations& filters) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnElementFilterMutated(element_id,
                                                            list_type, filters);
}

void CompositorPropertyTreeDelegate::OnElementBackdropFilterMutated(
    cc::ElementId element_id,
    cc::ElementListType list_type,
    const cc::FilterOperations& backdrop_filters) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnElementBackdropFilterMutated(
      element_id, list_type, backdrop_filters);
}

void CompositorPropertyTreeDelegate::OnElementOpacityMutated(
    cc::ElementId element_id,
    cc::ElementListType list_type,
    float opacity) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnElementOpacityMutated(
      element_id, list_type, opacity);
}

void CompositorPropertyTreeDelegate::OnElementTransformMutated(
    cc::ElementId element_id,
    cc::ElementListType list_type,
    const gfx::Transform& transform) {
  // TODO(crbug.com/389771428): Implement this w/ layer lists. For now,
  // just call the base class implementation to ensure that we don't get
  // out of date.
  cc::PropertyTreeLayerTreeDelegate::OnElementTransformMutated(
      element_id, list_type, transform);
}

void CompositorPropertyTreeDelegate::SetObserverForTesting(Observer* observer) {
  observer_ = observer;
}

}  // namespace ui
