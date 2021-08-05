// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_LAYER_ANIMATION_OBSERVER_TEST_API_H_
#define UI_COMPOSITOR_TEST_LAYER_ANIMATION_OBSERVER_TEST_API_H_

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {
namespace test {

// Test API to provide internal access to the LayerAnimationObserver class.
class LayerAnimationObserverTestApi {
 public:
  explicit LayerAnimationObserverTestApi(LayerAnimationObserver* observer);

  // Wrappers for LayerAnimationObserver.
  void AttachedToSequence(LayerAnimationSequence* sequence);
  void DetachedFromSequence(LayerAnimationSequence* sequence,
                            bool send_notification);

 private:
  // The instance to provide internal access to.
  LayerAnimationObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimationObserverTestApi);
};

}  // namespace test
}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_LAYER_ANIMATION_OBSERVER_TEST_API_H_
