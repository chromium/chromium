// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_H_
#define UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_H_

#include <iosfwd>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/animation/ink_drop_animation_ended_reason.h"
#include "ui/views/views_export.h"

namespace ui {
class Layer;
}  // namespace ui

namespace views {
namespace test {
class InkDropHighlightTestApi;
}  // namespace test

class BasePaintedLayerDelegate;
class InkDropHighlightObserver;

// Manages fade in/out animations for a Layer that is used to provide visual
// feedback on ui::Views for highlight states (e.g. mouse hover, keyboard
// focus).
class VIEWS_EXPORT InkDropHighlight {
 public:
  enum class AnimationType { kFadeIn, kFadeOut };

  // Creates a highlight with a specified painter.
  InkDropHighlight(const gfx::PointF& center_point,
                   std::unique_ptr<BasePaintedLayerDelegate> layer_delegate);

  // Creates a highlight that paints a partially transparent roundrect with
  // color |color|.
  InkDropHighlight(const gfx::SizeF& size,
                   int corner_radius,
                   const gfx::PointF& center_point,
                   SkColor color);

  // Deprecated version of the above that takes a Size instead of SizeF.
  // TODO(estade): remove. See crbug.com/706228
  InkDropHighlight(const gfx::Size& size,
                   int corner_radius,
                   const gfx::PointF& center_point,
                   SkColor color);

  // Creates a highlight that is drawn with a solid color layer. It's shape will
  // be determined by the mask or clip applied to the parent layer. Note that
  // this still uses the default highlight opacity. Users who supply a
  // |base_color| with alpha will also want to call set_visible_opacity(1.f);.
  InkDropHighlight(const gfx::SizeF& size, SkColor base_color);

  InkDropHighlight(const InkDropHighlight&) = delete;
  InkDropHighlight& operator=(const InkDropHighlight&) = delete;

  virtual ~InkDropHighlight();

  void set_observer(InkDropHighlightObserver* observer) {
    observer_ = observer;
  }

  void set_visible_opacity(float visible_opacity) {
    visible_opacity_ = visible_opacity;
  }

  // Returns true if the highlight animation is either in the process of fading
  // in or is fully visible.
  bool IsFadingInOrVisible() const;

  // Fades in the highlight visual over the given |duration|.
  void FadeIn(const base::TimeDelta& duration);

  // Fades out the highlight visual over the given |duration|.
  void FadeOut(const base::TimeDelta& duration);

  // The root Layer that can be added in to a Layer tree.
  ui::Layer* layer() { return layer_.get(); }

  // Returns a test api to access internals of this. Default implmentations
  // should return nullptr and test specific subclasses can override to return
  // an instance.
  virtual test::InkDropHighlightTestApi* GetTestApi();

 private:
  friend class test::InkDropHighlightTestApi;

  // Animates a fade in/out as specified by |animation_type| over the given
  // |duration|.
  void AnimateFade(AnimationType animation_type,
                   const base::TimeDelta& duration);

  // Calculates the Transform to apply to |layer_|.
  gfx::Transform CalculateTransform() const;

  // The callback that will be invoked when a fade in/out animation is started.
  void AnimationStartedCallback(AnimationType animation_type);

  // The callback that will be invoked when a fade in/out animation is complete.
  void AnimationEndedCallback(AnimationType animation_type,
                              InkDropAnimationEndedReason reason);

  // The size of the highlight shape when fully faded in.
  gfx::SizeF size_;

  // The center point of the highlight shape in the parent Layer's coordinate
  // space.
  gfx::PointF center_point_;

  // The opacity for the fully visible state of the highlight.
  float visible_opacity_ = 0.128f;

  // True if the last animation to be initiated was a kFadeIn, and false
  // otherwise.
  bool last_animation_initiated_was_fade_in_ = false;

  // The LayerDelegate that paints the highlight |layer_|. Null if |layer_| is a
  // solid color layer.
  std::unique_ptr<BasePaintedLayerDelegate> layer_delegate_;

  // The visual highlight layer.
  std::unique_ptr<ui::Layer> layer_;

  std::unique_ptr<AnimationAbortHandle> animation_abort_handle_;

  raw_ptr<InkDropHighlightObserver> observer_ = nullptr;
};

// Returns a human readable string for |animation_type|.  Useful for logging.
VIEWS_EXPORT std::string ToString(
    InkDropHighlight::AnimationType animation_type);

// This is declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(InkDropHighlight::AnimationType animation_type,
             ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_HIGHLIGHT_H_
