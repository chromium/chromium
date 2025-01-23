// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_property_tree_delegate.h"

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/property_tree_builder.h"

namespace ui {

void CompositorPropertyTreeDelegate::UpdatePropertyTreesIfNeeded(
    cc::LayerTreeHost* host) {
  // Note: this leaves the _BuiltPropertyTrees trace event guarded by
  // cc.debug to be consistent with the default implementations in cc and
  // so we don't need to set two different logging options to get the output.
  TRACE_EVENT0("ui",
               "CompositorPropertyTreeDelegate::UpdatePropertyTreesIfNeeded");
  cc::PropertyTreeBuilder::BuildPropertyTrees(host);
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                       "CompositorPropertyTreeDelegate::"
                       "UpdatePropertyTreesIfNeeded_BuiltPropertyTrees",
                       TRACE_EVENT_SCOPE_THREAD, "property_trees",
                       host->property_trees()->AsTracedValue());
  if (observer_) {
    observer_->OnUpdateCalled(host);
  }
}

void CompositorPropertyTreeDelegate::SetObserverForTesting(Observer* observer) {
  observer_ = observer;
}

}  // namespace ui
