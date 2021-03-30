// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_shape_updater.h"

#include "ui/gfx/skia_util.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace {

std::unique_ptr<ui::Layer::ShapeRects> ConvertToShapeRects(const SkPath& path) {
  // Converts to ShapeRects from SkPath.
  auto shape_rects = std::make_unique<ui::Layer::ShapeRects>();
  SkRegion clip_region;
  clip_region.setRect(path.getBounds().round());
  SkRegion region;
  region.setPath(path, clip_region);
  for (SkRegion::Iterator it(region); !it.done(); it.next())
    shape_rects->push_back(gfx::SkIRectToRect(it.rect()));
  return shape_rects;
}

}  // namespace

namespace views {

// static
WindowShapeUpdater* WindowShapeUpdater::CreateWindowShapeUpdater(
    DesktopWindowTreeHostPlatform* tree_host,
    DesktopNativeWidgetAura* native_widget_aura) {
  return new WindowShapeUpdater(tree_host, native_widget_aura);
}

WindowShapeUpdater::WindowShapeUpdater(
    DesktopWindowTreeHostPlatform* tree_host,
    DesktopNativeWidgetAura* native_widget_aura)
    : tree_host_(tree_host), native_widget_aura_(native_widget_aura) {
  tree_host_->GetContentWindow()->AddObserver(this);
  UpdateWindowShapeFromWindowMask(tree_host_->GetContentWindow());
}

void WindowShapeUpdater::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateWindowShapeFromWindowMask(window);
}

void WindowShapeUpdater::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  delete this;
}

void WindowShapeUpdater::UpdateWindowShapeFromWindowMask(aura::Window* window) {
  // If |ui::Layer::alpha_shape_| is set explicitly from SetShape,
  // we don't need to set default window mask from non_client_view.
  if (tree_host_->is_shape_explicitly_set())
    return;
  // WindowTransparency should be updated as well when the window shape is
  // changed. When a window mask exists, transparent should be true to prevent
  // compositor from filling the entire screen in AppendQuadsToFillScreen.
  // Otherwise, transparent should be false.
  native_widget_aura_->UpdateWindowTransparency();
  SkPath path = tree_host_->GetWindowMaskForWindowShapeInPixels();
  if (path.isEmpty()) {
    window->layer()->SetAlphaShape(nullptr);
    return;
  }
  SkPath path_in_dips;
  path.transform(SkMatrix(tree_host_->GetInverseRootTransform().matrix()),
                 &path_in_dips);

  // SetAlphaShape to the layer of |content_window_|
  window->layer()->SetAlphaShape(ConvertToShapeRects(path_in_dips));
}

}  // namespace views
