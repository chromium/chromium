// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_
#define UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/skia_vector_animation.h"
#include "ui/views/controls/image_view_base.h"

namespace gfx {
class SkiaVectorAnimation;
class Canvas;
}  // namespace gfx

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
  enum class State {
    kPlaying,  // The animation is currently playing.
    kStopped   // The animation is stopped and paint will raster the first
               // frame.
  };

  AnimatedImageView();
  ~AnimatedImageView() override;

  // Set the animated image that should be displayed. Setting an animated image
  // will result in stopping the current animation.
  void SetAnimatedImage(
      std::unique_ptr<gfx::SkiaVectorAnimation> animated_image);

  // Plays the animation in loop and must only be called when this view has
  // access to a widget.
  void Play();

  // Stops any animation and resets it to the start frame.
  void Stop();

 private:
  friend class AnimatedImageViewTest;

  // Overridden from View:
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;
  void NativeViewHierarchyChanged() override;
  void RemovedFromWidget() override;

  // Overridden from ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void SetCompositorFromWidget();
  void ClearCurrentCompositor();

  // Overridden from ImageViewBase:
  gfx::Size GetImageSize() const override;

  // The current state of the animation.
  State state_ = State::kStopped;

  // The compositor associated with the widget of this view.
  ui::Compositor* compositor_ = nullptr;

  // The most recent timestamp at which a paint was scheduled for this view.
  base::TimeTicks previous_timestamp_;

  // The underlying skia vector animation.
  std::unique_ptr<gfx::SkiaVectorAnimation> animated_image_;

  DISALLOW_COPY_AND_ASSIGN(AnimatedImageView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_ANIMATED_IMAGE_VIEW_H_
