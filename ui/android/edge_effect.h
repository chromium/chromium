// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_EDGE_EFFECT_H_
#define UI_ANDROID_EDGE_EFFECT_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace cc {
class Layer;
class UIResourceLayer;
}

namespace ui {
class ResourceManager;
}

namespace ui {

// A base class for overscroll-related Android effects.
class UI_ANDROID_EXPORT EdgeEffect {
 public:
  enum State {
    STATE_IDLE = 0,
    STATE_PULL,
    STATE_ABSORB,
    STATE_RECEDE,
    STATE_PULL_DECAY
  };

  enum Edge { EDGE_TOP, EDGE_LEFT, EDGE_BOTTOM, EDGE_RIGHT, EDGE_COUNT };

  explicit EdgeEffect(ui::ResourceManager* resource_manager);
  ~EdgeEffect();

  void Pull(base::TimeTicks current_time,
            float delta_distance,
            float displacement);
  void Absorb(base::TimeTicks current_time, float velocity);
  bool Update(base::TimeTicks current_time);
  void Release(base::TimeTicks current_time);

  void Finish();
  bool IsFinished() const;
  float GetAlpha() const;

  void ApplyToLayers(Edge edge, const gfx::SizeF& viewport_size, float offset);
  void SetParent(cc::Layer* parent);

 private:
  ui::ResourceManager* const resource_manager_;

  scoped_refptr<cc::UIResourceLayer> glow_;

  float glow_alpha_;
  float glow_scale_y_;

  float glow_alpha_start_;
  float glow_alpha_finish_;
  float glow_scale_y_start_;
  float glow_scale_y_finish_;

  gfx::RectF arc_rect_;
  gfx::Size bounds_;
  float displacement_;
  float target_displacement_;

  base::TimeTicks start_time_;
  base::TimeDelta duration_;

  State state_;

  float pull_distance_;

  DISALLOW_COPY_AND_ASSIGN(EdgeEffect);
};

}  // namespace ui

#endif  // UI_ANDROID_EDGE_EFFECT_H_
