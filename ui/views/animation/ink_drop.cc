// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop.h"

#include "ui/views/animation/ink_drop_observer.h"

namespace views {

InkDrop::~InkDrop() = default;

void InkDrop::AddObserver(InkDropObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void InkDrop::RemoveObserver(InkDropObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

InkDrop::InkDrop() = default;

void InkDrop::NotifyInkDropAnimationStarted() {
  for (InkDropObserver& observer : observers_)
    observer.InkDropAnimationStarted();
}

void InkDrop::NotifyInkDropRippleAnimationEnded(InkDropState ink_drop_state) {
  for (InkDropObserver& observer : observers_)
    observer.InkDropRippleAnimationEnded(ink_drop_state);
}

InkDropContainerView::InkDropContainerView() = default;

void InkDropContainerView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  SetPaintToLayer();
  SetVisible(true);
  layer()->SetFillsBoundsOpaquely(false);
  layer()->Add(ink_drop_layer);
}

void InkDropContainerView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  layer()->Remove(ink_drop_layer);
  SetVisible(false);
  DestroyLayer();
}

bool InkDropContainerView::CanProcessEventsWithinSubtree() const {
  // Ensure the container View is found as the EventTarget instead of this.
  return false;
}

}  // namespace views
