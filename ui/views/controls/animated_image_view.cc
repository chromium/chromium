// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/animated_image_view.h"

#include <utility>

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace {

bool AreAnimatedImagesEqual(const lottie::Animation& animation_1,
                            const lottie::Animation& animation_2) {
  // In rare cases this may return false, even if the animated images are backed
  // by the same resource file.
  return animation_1.skottie() == animation_2.skottie();
}

}  // namespace

AnimatedImageView::AnimatedImageView() = default;

AnimatedImageView::~AnimatedImageView() = default;

void AnimatedImageView::SetAnimatedImage(
    std::unique_ptr<lottie::Animation> animated_image) {
  if (animated_image_ &&
      AreAnimatedImagesEqual(*animated_image, *animated_image_)) {
    Stop();
    return;
  }

  gfx::Size preferred_size(GetPreferredSize({}));
  animated_image_ = std::move(animated_image);

  // Stop the animation to reset it.
  Stop();

  if (preferred_size != GetPreferredSize({})) {
    PreferredSizeChanged();
  }
  SchedulePaint();
}

void AnimatedImageView::Play(
    std::optional<lottie::Animation::PlaybackConfig> playback_config) {
  DCHECK(animated_image_);
  if (state_ == State::kPlaying)
    return;

  state_ = State::kPlaying;

  if (!playback_config) {
    playback_config =
        lottie::Animation::PlaybackConfig::CreateDefault(*animated_image_);
  }
  set_check_active_duration(playback_config->style !=
                            lottie::Animation::Style::kLoop);

  if (GetWidget()) {
    DoPlay(std::move(*playback_config));
  } else {
    // Playback will start in `AddedToWidget`.
    playback_config_ = std::make_unique<lottie::Animation::PlaybackConfig>(
        std::move(*playback_config));
  }
}

void AnimatedImageView::Stop() {
  if (state_ == State::kStopped)
    return;

  DCHECK(animated_image_);
  ClearCurrentCompositor();

  animated_image_->Stop();
  state_ = State::kStopped;
}

gfx::Size AnimatedImageView::GetImageSize() const {
  return image_size_.value_or(
      animated_image_ ? animated_image_->GetOriginalSize() : gfx::Size());
}

void AnimatedImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (!animated_image_)
    return;
  canvas->Save();

  gfx::Vector2d translation = GetImageBounds().origin().OffsetFromOrigin();
  translation.Add(additional_translation_);
  canvas->Translate(std::move(translation));

  if (!previous_timestamp_.is_null() && state_ != State::kStopped) {
    animated_image_->Paint(canvas, previous_timestamp_, GetImageSize());
  } else {
    // OnPaint may be called before clock tick was received; in that case just
    // paint the first frame.
    animated_image_->PaintFrame(canvas, 0, GetImageSize());
  }

  canvas->Restore();
}

void AnimatedImageView::NativeViewHierarchyChanged() {
  ui::Compositor* compositor = GetWidget()->GetCompositor();
  DCHECK(compositor);
  if (compositor_ != compositor) {
    ClearCurrentCompositor();

    // Restore the Play() state with the new compositor.
    if (state_ == State::kPlaying)
      SetCompositorFromWidget();
  }
}

void AnimatedImageView::AddedToWidget() {
  if (state_ == State::kPlaying && playback_config_) {
    DoPlay(std::move(*playback_config_));
    playback_config_.reset();
  }
}

void AnimatedImageView::RemovedFromWidget() {
  if (compositor_) {
    Stop();
    ClearCurrentCompositor();
  }
}

void AnimatedImageView::OnAnimationStep(base::TimeTicks timestamp) {
  TRACE_EVENT1("views", "AnimatedImageView::OnAnimationStep", "timestamp",
               timestamp);
  previous_timestamp_ = timestamp;
  SchedulePaint();
}

void AnimatedImageView::OnCompositingShuttingDown(ui::Compositor* compositor) {
  if (compositor_ == compositor) {
    Stop();
    ClearCurrentCompositor();
  }
}

void AnimatedImageView::DoPlay(
    lottie::Animation::PlaybackConfig playback_config) {
  SetCompositorFromWidget();
  animated_image_->Start(std::move(playback_config));
}

void AnimatedImageView::SetCompositorFromWidget() {
  DCHECK(!compositor_);
  auto* widget = GetWidget();
  DCHECK(widget);
  compositor_ = widget->GetCompositor();
  DCHECK(!compositor_->HasAnimationObserver(this));
  compositor_->AddAnimationObserver(this);
}

void AnimatedImageView::ClearCurrentCompositor() {
  if (compositor_) {
    DCHECK(compositor_->HasAnimationObserver(this));
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

BEGIN_METADATA(AnimatedImageView)
END_METADATA

}  // namespace views
