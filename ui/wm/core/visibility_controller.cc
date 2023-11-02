// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/visibility_controller.h"

#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/window_animations.h"

namespace wm {

namespace {

// Property set on all windows whose child windows' visibility changes are
// animated.
DEFINE_UI_CLASS_PROPERTY_KEY(bool,
                             kChildWindowVisibilityChangesAnimatedKey,
                             false)

// A window with this property set will animate upon its visibility changes.
DEFINE_UI_CLASS_PROPERTY_KEY(bool, kWindowVisibilityChangesAnimatedKey, false)

bool ShouldAnimateWindow(aura::Window* window) {
  return (window->parent() &&
          window->parent()->GetProperty(
              kChildWindowVisibilityChangesAnimatedKey)) ||
         window->GetProperty(kWindowVisibilityChangesAnimatedKey);
}

}  // namespace

VisibilityController::VisibilityController() {
}

VisibilityController::~VisibilityController() {
}

bool VisibilityController::CallAnimateOnChildWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  return AnimateOnChildWindowVisibilityChanged(window, visible);
}

void VisibilityController::UpdateLayerVisibility(aura::Window* window,
                                                 bool visible) {
  bool animated = window->GetType() != aura::client::WINDOW_TYPE_CONTROL &&
                  window->GetType() != aura::client::WINDOW_TYPE_UNKNOWN &&
                  ShouldAnimateWindow(window);
  animated = animated &&
      CallAnimateOnChildWindowVisibilityChanged(window, visible);

  // If we're already in the process of hiding don't do anything. Otherwise we
  // may end up prematurely canceling the animation.
  // This does not check opacity as when fading out a visibility change should
  // also be scheduled (to do otherwise would mean the window can not be seen,
  // opacity is 0, yet the window is marked as visible) (see CL 132903003).
  // TODO(vollick): remove this.
  if (!visible &&
      window->layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::VISIBILITY) &&
      !window->layer()->GetTargetVisibility()) {
    return;
  }

  // When a window is made visible, we always make its layer visible
  // immediately. When a window is hidden, the layer must be left visible and
  // only made not visible once the animation is complete.
  if (!animated || visible)
    window->layer()->SetVisible(visible);
}

SuspendChildWindowVisibilityAnimations::SuspendChildWindowVisibilityAnimations(
    aura::Window* window)
    : window_(window),
      original_enabled_(window->GetProperty(
          kChildWindowVisibilityChangesAnimatedKey)) {
  window_->ClearProperty(kChildWindowVisibilityChangesAnimatedKey);
}

SuspendChildWindowVisibilityAnimations::
    ~SuspendChildWindowVisibilityAnimations() {
  if (original_enabled_)
    window_->SetProperty(kChildWindowVisibilityChangesAnimatedKey, true);
  else
    window_->ClearProperty(kChildWindowVisibilityChangesAnimatedKey);
}

void SetWindowVisibilityChangesAnimated(aura::Window* window) {
  window->SetProperty(kWindowVisibilityChangesAnimatedKey, true);
}

void SetChildWindowVisibilityChangesAnimated(aura::Window* window) {
  window->SetProperty(kChildWindowVisibilityChangesAnimatedKey, true);
}

}  // namespace wm
