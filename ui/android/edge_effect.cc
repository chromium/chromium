// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/edge_effect.h"

#include "base/notreached.h"
#include "cc/slim/layer.h"
#include "cc/slim/ui_resource_layer.h"
#include "ui/android/animation_utils.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/android/window_android_compositor.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ui {
namespace {
// Time it will take the effect to fully recede in ms
const int kRecedeTimeMs = 600;

// Time it will take before a pulled glow begins receding in ms
const int kPullTimeMs = 167;

// Time it will take for a pulled glow to decay to partial strength before
// release
const int kPullDecayTimeMs = 2000;

const float kMaxAlpha = 0.5f;

const float kPullGlowBegin = 0.f;

// Min/max velocity that will be absorbed
const float kMinVelocity = 100.f;
const float kMaxVelocity = 10000.f;

const float kEpsilon = 0.001f;

const float kSin = 0.5f;    // sin(PI / 6)
const float kCos = 0.866f;  // cos(PI / 6);

// How much dragging should effect the height of the glow image.
// Number determined by user testing.
const float kPullDistanceAlphaGlowFactor = 0.8f;

const int kVelocityGlowFactor = 6;

const ui::SystemUIResourceType kResourceId = ui::OVERSCROLL_GLOW;

// Computes the transform for an edge effect given the |edge|, |viewport_size|
// and edge |offset|. This assumes the the effect transform anchor is at the
// centered edge of the effect.
gfx::Transform ComputeTransform(EdgeEffect::Edge edge,
                                const gfx::SizeF& viewport_size,
                                float offset) {
  // Transforms assume the edge layers are anchored to their *top center point*.
  switch (edge) {
    case EdgeEffect::EDGE_TOP:
      return gfx::Transform::MakeTranslation(0, offset);
    case EdgeEffect::EDGE_LEFT:
      return gfx::Transform::MakeTranslation(
                 -viewport_size.height() / 2.f + offset,
                 viewport_size.height() / 2.f) *
             gfx::Transform::Make270degRotation();
    case EdgeEffect::EDGE_BOTTOM:
      return gfx::Transform::MakeTranslation(0,
                                             viewport_size.height() + offset) *
             gfx::Transform::Make180degRotation();
    case EdgeEffect::EDGE_RIGHT:
      return gfx::Transform::MakeTranslation(
                 -viewport_size.height() / 2.f + viewport_size.width() + offset,
                 viewport_size.height() / 2.f) *
             gfx::Transform::Make90degRotation();
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid edge: " << edge;
      return gfx::Transform();
  };
}

// Computes the maximum effect size relative to the screen |edge|. For
// top/bottom edges, this is simply |viewport_size|, while for left/right
// edges this is |viewport_size| with coordinates swapped.
gfx::SizeF ComputeOrientedSize(EdgeEffect::Edge edge,
                               const gfx::SizeF& viewport_size) {
  switch (edge) {
    case EdgeEffect::EDGE_TOP:
    case EdgeEffect::EDGE_BOTTOM:
      return viewport_size;
    case EdgeEffect::EDGE_LEFT:
    case EdgeEffect::EDGE_RIGHT:
      return gfx::SizeF(viewport_size.height(), viewport_size.width());
    default:
      NOTREACHED_IN_MIGRATION() << "Invalid edge: " << edge;
      return gfx::SizeF();
  };
}

}  // namespace

EdgeEffect::EdgeEffect(ui::ResourceManager* resource_manager)
    : resource_manager_(resource_manager),
      glow_(cc::slim::UIResourceLayer::Create()),
      glow_alpha_(0),
      glow_scale_y_(0),
      glow_alpha_start_(0),
      glow_alpha_finish_(0),
      glow_scale_y_start_(0),
      glow_scale_y_finish_(0),
      displacement_(0.5f),
      target_displacement_(0.5f),
      state_(STATE_IDLE),
      pull_distance_(0) {
  // Prevent the provided layers from drawing until the effect is activated.
  glow_->SetIsDrawable(false);
}

EdgeEffect::~EdgeEffect() {
  glow_->RemoveFromParent();
}

bool EdgeEffect::IsFinished() const {
  return state_ == STATE_IDLE;
}

void EdgeEffect::Finish() {
  glow_->SetIsDrawable(false);
  pull_distance_ = 0;
  state_ = STATE_IDLE;
}

void EdgeEffect::Pull(base::TimeTicks current_time,
                      float delta_distance,
                      float displacement) {
  target_displacement_ = displacement;
  if (state_ == STATE_PULL_DECAY && current_time - start_time_ < duration_) {
    return;
  }
  if (state_ != STATE_PULL) {
    glow_scale_y_ = std::max(kPullGlowBegin, glow_scale_y_);
  }
  state_ = STATE_PULL;

  start_time_ = current_time;
  duration_ = base::Milliseconds(kPullTimeMs);

  float abs_delta_distance = std::abs(delta_distance);
  pull_distance_ += delta_distance;

  glow_alpha_ = glow_alpha_start_ = std::min(
      kMaxAlpha,
      glow_alpha_ + (abs_delta_distance * kPullDistanceAlphaGlowFactor));

  if (pull_distance_ == 0) {
    glow_scale_y_ = glow_scale_y_start_ = 0;
  } else {
    float scale = 1.f -
                  1.f / std::sqrt(std::abs(pull_distance_) * bounds_.height()) -
                  0.3f;
    glow_scale_y_ = glow_scale_y_start_ = std::max(0.f, scale) / 0.7f;
  }

  glow_alpha_finish_ = glow_alpha_;
  glow_scale_y_finish_ = glow_scale_y_;
}

void EdgeEffect::Release(base::TimeTicks current_time) {
  pull_distance_ = 0;

  if (state_ != STATE_PULL && state_ != STATE_PULL_DECAY)
    return;

  state_ = STATE_RECEDE;
  glow_alpha_start_ = glow_alpha_;
  glow_scale_y_start_ = glow_scale_y_;

  glow_alpha_finish_ = 0.f;
  glow_scale_y_finish_ = 0.f;

  start_time_ = current_time;
  duration_ = base::Milliseconds(kRecedeTimeMs);
}

void EdgeEffect::Absorb(base::TimeTicks current_time, float velocity) {
  state_ = STATE_ABSORB;

  velocity = Clamp(std::abs(velocity), kMinVelocity, kMaxVelocity);

  start_time_ = current_time;
  // This should never be less than 1 millisecond.
  duration_ = base::Milliseconds(0.15f + (velocity * 0.02f));

  // The glow depends more on the velocity, and therefore starts out
  // nearly invisible.
  glow_alpha_start_ = 0.3f;
  glow_scale_y_start_ = std::max(glow_scale_y_, 0.f);

  // Growth for the size of the glow should be quadratic to properly respond
  // to a user's scrolling speed. The faster the scrolling speed, the more
  // intense the effect should be for both the size and the saturation.
  glow_scale_y_finish_ =
      std::min(0.025f + (velocity * (velocity / 100) * 0.00015f) / 2.f, 1.f);
  // Alpha should change for the glow as well as size.
  glow_alpha_finish_ = Clamp(
      glow_alpha_start_, velocity * kVelocityGlowFactor * .00001f, kMaxAlpha);
  target_displacement_ = 0.5;
}

bool EdgeEffect::Update(base::TimeTicks current_time) {
  if (IsFinished())
    return false;

  const double t = std::min((current_time - start_time_) / duration_, 1.0);
  const float interp = static_cast<float>(Damp(t, 1.0));

  glow_alpha_ = Lerp(glow_alpha_start_, glow_alpha_finish_, interp);
  glow_scale_y_ = Lerp(glow_scale_y_start_, glow_scale_y_finish_, interp);
  displacement_ = (displacement_ + target_displacement_) / 2.f;

  if (t >= 1.f - kEpsilon) {
    switch (state_) {
      case STATE_ABSORB:
        state_ = STATE_RECEDE;
        start_time_ = current_time;
        duration_ = base::Milliseconds(kRecedeTimeMs);

        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        glow_alpha_finish_ = 0.0f;
        glow_scale_y_finish_ = 0.0f;
        break;
      case STATE_PULL:
        state_ = STATE_PULL_DECAY;
        start_time_ = current_time;
        duration_ = base::Milliseconds(kPullDecayTimeMs);

        glow_alpha_start_ = glow_alpha_;
        glow_scale_y_start_ = glow_scale_y_;

        // After pull, the glow should fade to nothing.
        glow_alpha_finish_ = 0.0f;
        glow_scale_y_finish_ = 0.0f;
        break;
      case STATE_PULL_DECAY:
        state_ = STATE_RECEDE;
        break;
      case STATE_RECEDE:
        Finish();
        break;
      default:
        break;
    }
  }

  bool one_last_frame = false;
  if (state_ == STATE_RECEDE && glow_scale_y_ <= 0) {
    Finish();
    one_last_frame = true;
  }

  return !IsFinished() || one_last_frame;
}

float EdgeEffect::GetAlpha() const {
  return IsFinished() ? 0.f : glow_alpha_;
}

void EdgeEffect::ApplyToLayers(Edge edge,
                               const gfx::SizeF& viewport_size,
                               float offset) {
  if (IsFinished())
    return;

  // An empty viewport, while meaningless, is also relatively harmless, and will
  // simply prevent any drawing of the layers.
  if (viewport_size.IsEmpty()) {
    glow_->SetIsDrawable(false);
    return;
  }

  gfx::SizeF size = ComputeOrientedSize(edge, viewport_size);
  const float r = size.width() * 0.75f / kSin;
  const float y = kCos * r;
  const float h = r - y;
  const float o_r = size.height() * 0.75f / kSin;
  const float o_y = kCos * o_r;
  const float o_h = o_r - o_y;
  const float base_glow_scale = h > 0.f ? std::min(o_h / h, 1.f) : 1.f;
  bounds_ = gfx::Size(size.width(), (int)std::min(size.height(), h));
  gfx::Size image_bounds(
      r, std::min(1.f, glow_scale_y_) * base_glow_scale * bounds_.height());

  // Compute the displaced image rect. This includes both the horizontal
  // offset from the |displacement_| factor, as well as the vertical edge offset
  // provided by the method call.
  const float displacement = Clamp(displacement_, 0.f, 1.f) - 0.5f;
  const float displacement_offset_x = bounds_.width() * displacement * 0.5f;
  const float image_offset_x = (bounds_.width() - image_bounds.width()) * 0.5f;
  gfx::RectF image_rect = gfx::RectF(gfx::SizeF(image_bounds));
  image_rect.Offset(image_offset_x - displacement_offset_x, -std::abs(offset));

  // Clip the image rect against the viewport. If either rect is empty there's
  // no need to draw anything further.
  gfx::RectF clipped_rect(size.width(), size.height());
  clipped_rect.Intersect(image_rect);
  if (clipped_rect.IsEmpty() || image_rect.IsEmpty()) {
    glow_->SetIsDrawable(false);
    return;
  }

  // Compute the logical UV coordinates of the clipped rect relative to the
  // displaced image rect.
  gfx::PointF clipped_top_left = clipped_rect.origin();
  gfx::PointF clipped_bottom_right = clipped_rect.bottom_right();
  gfx::PointF uv_top_left(
      (clipped_top_left.x() - image_rect.x()) / image_rect.width(),
      (clipped_top_left.y() - image_rect.y()) / image_rect.height());
  gfx::PointF uv_bottom_right(
      (clipped_bottom_right.x() - image_rect.x()) / image_rect.width(),
      (clipped_bottom_right.y() - image_rect.y()) / image_rect.height());
  glow_->SetUV(uv_top_left, uv_bottom_right);

  // There's no need to use the provided |offset| when computing the transform;
  // the offset is built in to the computed UV coordinates.
  glow_->SetTransform(ComputeTransform(edge, viewport_size, 0));

  glow_->SetIsDrawable(true);
  glow_->SetUIResourceId(resource_manager_->GetUIResourceId(
      ui::ANDROID_RESOURCE_TYPE_SYSTEM, kResourceId));
  glow_->SetTransformOrigin(gfx::PointF(bounds_.width() * 0.5f, 0));
  glow_->SetBounds(gfx::ToRoundedSize(clipped_rect.size()));
  glow_->SetContentsOpaque(false);
  glow_->SetOpacity(Clamp(glow_alpha_, 0.f, 1.f));
}

void EdgeEffect::SetParent(cc::slim::Layer* parent) {
  if (glow_->parent() != parent)
    parent->AddChild(glow_);
}

}  // namespace ui
