// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/image_view_base.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
}

namespace ui {
class Compositor;
}

namespace views {

/////////////////////////////////////////////////////////////////////////////
//
// AnimatedImageView class.
//
// An AnimatedImageView can display a skia vector animation. The animation paint
// size can be set via SetImageSize. The animation is stopped by default.
// Use this over AnimatedIconView if you want to play a skottie animation file.
//
/////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT AnimatedImageView : public ImageViewBase,
                                       public ui::CompositorAnimationObserver {
 public:
  METADATA_HEADER(AnimatedImageView);

  enum class State {
    kPlaying,  // The animation is currently playing.
    kStopped   // The animation is stopped and paint will raster the first
               // frame.
  };

  AnimatedImageView();
  AnimatedImageView(const AnimatedImageView&) = delete;
  AnimatedImageView& operator=(const AnimatedImageView&) = delete;
  ~AnimatedImageView() override;

  // Set the animated image that should be displayed. Setting an animated image
  // will result in stopping the current animation.
  void SetAnimatedImage(std::unique_ptr<lottie::Animation> animated_image);

  // Plays the animation and must only be called when this view has
  // access to a widget.
  //
  // If a null |playback_config| is provided, the default one is used.
  void Play(absl::optional<lottie::Animation::PlaybackConfig> playback_config =
                absl::nullopt);

  // Stops any animation and resets it to the start frame.
  void Stop();

  // May return null if SetAnimatedImage() has not been called.
  lottie::Animation* animated_image() { return animated_image_.get(); }

  // Sets additional translation that will be applied to all future rendered
  // animation frames. The term "additional" is used because the provided
  // translation is applied on top of the translation that ImageViewBase already
  // applies to align the animation appropriately within the view's boundaries.
  //
  // Note this is not the same as translating the entire View. This only
  // translates the animation within the existing content bounds of the View. By
  // default, there is no additional translation.
  void SetAdditionalTranslation(gfx::Vector2d additional_translation) {
    additional_translation_ = std::move(additional_translation);
  }

 private:
  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;
  void NativeViewHierarchyChanged() override;
  void RemovedFromWidget() override;

  // Overridden from ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // Overridden from ImageViewBase:
  gfx::Size GetImageSize() const override;

  void SetCompositorFromWidget();
  void ClearCurrentCompositor();

  // The current state of the animation.
  State state_ = State::kStopped;

  // The compositor associated with the widget of this view.
  raw_ptr<ui::Compositor> compositor_ = nullptr;

  // The most recent timestamp at which a paint was scheduled for this view.
  base::TimeTicks previous_timestamp_;

  // The underlying lottie animation.
  std::unique_ptr<lottie::Animation> animated_image_;

  gfx::Vector2d additional_translation_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, AnimatedImageView, ImageViewBase)
VIEW_BUILDER_PROPERTY(std::unique_ptr<lottie::Animation>, AnimatedImage)
VIEW_BUILDER_PROPERTY(gfx::Vector2d, AdditionalTranslation)
END_VIEW_BUILDER

}  // namespace views

DEFINE_VIEW_BUILDER(VIEWS_EXPORT, AnimatedImageView)

#endif  // UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_
