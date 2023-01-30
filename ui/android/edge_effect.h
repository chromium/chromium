// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_EDGE_EFFECT_H_
#define UI_ANDROID_EDGE_EFFECT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc::slim {
class Layer;
class UIResourceLayer;
}  // namespace cc::slim

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

  EdgeEffect(const EdgeEffect&) = delete;
  EdgeEffect& operator=(const EdgeEffect&) = delete;

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
  void SetParent(cc::slim::Layer* parent);

 private:
  const raw_ptr<ui::ResourceManager, DanglingUntriaged> resource_manager_;

  scoped_refptr<cc::slim::UIResourceLayer> glow_;

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
};

}  // namespace ui

#endif  // UI_ANDROID_EDGE_EFFECT_H_
