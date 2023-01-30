// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_OVERSCROLL_GLOW_H_
#define UI_ANDROID_OVERSCROLL_GLOW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/android/edge_effect.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc::slim {
class Layer;
}

namespace ui {

// Provides lazy, customized EdgeEffect creation.
class UI_ANDROID_EXPORT OverscrollGlowClient {
 public:
  virtual ~OverscrollGlowClient() {}

  // Called lazily, after the initial overscrolling event.
  virtual std::unique_ptr<EdgeEffect> CreateEdgeEffect() = 0;
};

/* |OverscrollGlow| mirrors its Android counterpart, OverscrollGlow.java.
 * Conscious tradeoffs were made to align this as closely as possible with the
 * original Android Java version.
 */
class UI_ANDROID_EXPORT OverscrollGlow {
 public:
  // |client| must be valid for the duration of the effect's lifetime.
  // The effect is enabled by default, but will remain dormant until the first
  // overscroll event.
  explicit OverscrollGlow(OverscrollGlowClient* client);

  OverscrollGlow(const OverscrollGlow&) = delete;
  OverscrollGlow& operator=(const OverscrollGlow&) = delete;

  virtual ~OverscrollGlow();

  // Called when the root content layer overscrolls.
  // |accumulated_overscroll| and |overscroll_delta| are in device pixels, while
  // |velocity| is in device pixels / second.
  // |overscroll_location| is the coordinate of the causal overscrolling event.
  // Returns true if the effect still needs animation ticks.
  // This method is made virtual for mocking.
  virtual bool OnOverscrolled(base::TimeTicks current_time,
                              gfx::Vector2dF accumulated_overscroll,
                              gfx::Vector2dF overscroll_delta,
                              gfx::Vector2dF velocity,
                              gfx::Vector2dF overscroll_location);

  // Returns true if the effect still needs animation ticks, with effect layers
  // attached to |parent_layer| if necessary.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time, cc::slim::Layer* parent_layer);

  // Update the effect according to the most recent display parameters,
  // Note: All dimensions are in device pixels.
  void OnFrameUpdated(const gfx::SizeF& viewport_size,
                      const gfx::SizeF& content_size,
                      const gfx::PointF& content_scroll_offset);

  // Reset the effect to its inactive state, clearing any active effects.
  void Reset();

  // Whether the effect is active, either being pulled or receding.
  bool IsActive() const;

  // The maximum alpha value (in the range [0,1]) of any animated edge layers.
  // If the effect is inactive, this will be 0.
  float GetVisibleAlpha() const;

 private:
  enum Axis { AXIS_X, AXIS_Y };
  enum { EDGE_COUNT = EdgeEffect::EDGE_COUNT };

  // Returns whether the effect has been properly initialized.
  bool InitializeIfNecessary();
  bool CheckNeedsAnimate();
  void UpdateLayerAttachment(cc::slim::Layer* parent);
  void Detach();
  void Pull(base::TimeTicks current_time,
            const gfx::Vector2dF& overscroll_delta,
            const gfx::Vector2dF& overscroll_location);
  void Absorb(base::TimeTicks current_time,
              const gfx::Vector2dF& velocity,
              bool x_overscroll_started,
              bool y_overscroll_started);
  void Release(base::TimeTicks current_time);

  EdgeEffect* GetOppositeEdge(int edge_index);

  raw_ptr<OverscrollGlowClient> client_;
  std::unique_ptr<EdgeEffect> edge_effects_[EDGE_COUNT];

  gfx::SizeF viewport_size_;
  float edge_offsets_[EDGE_COUNT];
  bool initialized_;
  bool allow_horizontal_overscroll_;
  bool allow_vertical_overscroll_;

  scoped_refptr<cc::slim::Layer> root_layer_;
};

}  // namespace ui

#endif  // UI_ANDROID_OVERSCROLL_GLOW_H_
