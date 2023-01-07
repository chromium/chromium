// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_THREADED_ANIMATION_DELEGATE_H_
#define UI_COMPOSITOR_LAYER_THREADED_ANIMATION_DELEGATE_H_

#include <memory>

#include "cc/animation/keyframe_model.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

// Attach CC keyframe_models using this interface.
class COMPOSITOR_EXPORT LayerThreadedAnimationDelegate {
 public:
  virtual void AddThreadedAnimation(
      std::unique_ptr<cc::KeyframeModel> keyframe_model) = 0;
  virtual void RemoveThreadedAnimation(int keyframe_model_id) = 0;

 protected:
  virtual ~LayerThreadedAnimationDelegate() {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_THREADED_ANIMATION_DELEGATE_H_
