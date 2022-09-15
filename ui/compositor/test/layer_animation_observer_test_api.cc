// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/layer_animation_observer_test_api.h"

namespace ui {
namespace test {

LayerAnimationObserverTestApi::LayerAnimationObserverTestApi(
    LayerAnimationObserver* observer)
    : observer_(observer) {}

void LayerAnimationObserverTestApi::AttachedToSequence(
    LayerAnimationSequence* sequence) {
  observer_->AttachedToSequence(sequence);
}

void LayerAnimationObserverTestApi::DetachedFromSequence(
    LayerAnimationSequence* sequence,
    bool send_notification) {
  observer_->DetachedFromSequence(sequence, send_notification);
}

}  // namespace test
}  // namespace ui
