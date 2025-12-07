// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_KEY_H_
#define UI_VIEWS_ANIMATION_ANIMATION_KEY_H_

#include <tuple>

#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_animation_element.h"

namespace ui {
class Layer;
}

namespace views {

struct AnimationKey {
  raw_ptr<ui::Layer> target;
  ui::LayerAnimationElement::AnimatableProperty property;

  auto operator<=>(const AnimationKey& key) const = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_KEY_H_
