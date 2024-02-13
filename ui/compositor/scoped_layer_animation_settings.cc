// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/scoped_layer_animation_settings.h"

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_observer.h"

namespace ui {

namespace {

const int kScopedLayerAnimationDefaultTransitionDurationMs = 200;

template <typename Trait>
class ScopedLayerAnimationObserver : public ui::ImplicitAnimationObserver,
                                     public ui::LayerObserver {
 public:
  ScopedLayerAnimationObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    Trait::AddRequest(layer_);
  }

  ScopedLayerAnimationObserver(const ScopedLayerAnimationObserver&) = delete;
  ScopedLayerAnimationObserver& operator=(const ScopedLayerAnimationObserver&) =
      delete;

  ~ScopedLayerAnimationObserver() override {
    if (layer_)
      layer_->RemoveObserver(this);
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override {
    // If animation finishes before |layer_| is destoyed, we will remove the
    // request applied on the layer and remove |this| from the |layer_|
    // observer list when deleting |this|.
    if (layer_) {
      Trait::RemoveRequest(layer_);
      layer_->GetAnimator()->RemoveAndDestroyOwnedObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    // If the animation is still going past layer destruction then we want the
    // layer to keep the request until the animation has finished. We will defer
    // deleting |this| until the animation finishes.
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  raw_ptr<ui::Layer> layer_;
};

struct RenderSurfaceCachingTrait {
  static void AddRequest(ui::Layer* layer) {
    layer->AddCacheRenderSurfaceRequest();
  }
  static void RemoveRequest(ui::Layer* layer) {
    layer->RemoveCacheRenderSurfaceRequest();
  }
};
using ScopedRenderSurfaceCaching =
    ScopedLayerAnimationObserver<RenderSurfaceCachingTrait>;

struct DeferredPaintingTrait {
  static void AddRequest(ui::Layer* layer) { layer->AddDeferredPaintRequest(); }
  static void RemoveRequest(ui::Layer* layer) {
    layer->RemoveDeferredPaintRequest();
  }
};
using ScopedDeferredPainting =
    ScopedLayerAnimationObserver<DeferredPaintingTrait>;

struct TrilinearFilteringTrait {
  static void AddRequest(ui::Layer* layer) {
    layer->AddTrilinearFilteringRequest();
  }
  static void RemoveRequest(ui::Layer* layer) {
    layer->RemoveTrilinearFilteringRequest();
  }
};
using ScopedTrilinearFiltering =
    ScopedLayerAnimationObserver<TrilinearFilteringTrait>;

void AddObserverToSettings(
    ui::ScopedLayerAnimationSettings* settings,
    std::unique_ptr<ui::ImplicitAnimationObserver> observer) {
  settings->AddObserver(observer.get());
  settings->GetAnimator()->AddOwnedObserver(std::move(observer));
}

void AddScopedDeferredPaintingObserverRecursive(
    ui::Layer* layer,
    ui::ScopedLayerAnimationSettings* settings) {
  auto observer = std::make_unique<ScopedDeferredPainting>(layer);
  AddObserverToSettings(settings, std::move(observer));
  for (ui::Layer* child : layer->children()) {
    AddScopedDeferredPaintingObserverRecursive(child, settings);
  }
}

}  // namespace

// ScopedLayerAnimationSettings ------------------------------------------------
ScopedLayerAnimationSettings::ScopedLayerAnimationSettings(
    scoped_refptr<LayerAnimator> animator)
    : animator_(animator),
      old_is_transition_duration_locked_(
          animator->is_transition_duration_locked_),
      old_transition_duration_(animator->GetTransitionDuration()),
      old_tween_type_(animator->tween_type()),
      old_preemption_strategy_(animator->preemption_strategy()) {
  SetTransitionDuration(
      base::Milliseconds(kScopedLayerAnimationDefaultTransitionDurationMs));
}

ScopedLayerAnimationSettings::~ScopedLayerAnimationSettings() {
  animator_->is_transition_duration_locked_ =
      old_is_transition_duration_locked_;
  animator_->SetTransitionDuration(old_transition_duration_);
  animator_->set_tween_type(old_tween_type_);
  animator_->set_preemption_strategy(old_preemption_strategy_);

  for (ImplicitAnimationObserver* observer : observers_) {
    // Directly remove |observer| from |LayerAnimator::observers_| rather than
    // calling LayerAnimator::RemoveObserver(), to avoid removing it from the
    // observer list of LayerAnimationSequences that have already been
    // scheduled.
    animator_->observers_.RemoveObserver(observer);
    observer->SetActive(true);
  }
}

void ScopedLayerAnimationSettings::AddObserver(
    ImplicitAnimationObserver* observer) {
  observers_.insert(observer);
  animator_->AddObserver(observer);
}

void ScopedLayerAnimationSettings::SetTransitionDuration(
    base::TimeDelta duration) {
  animator_->SetTransitionDuration(duration);
}

base::TimeDelta ScopedLayerAnimationSettings::GetTransitionDuration() const {
  return animator_->GetTransitionDuration();
}

void ScopedLayerAnimationSettings::LockTransitionDuration() {
  animator_->is_transition_duration_locked_ = true;
}

void ScopedLayerAnimationSettings::SetTweenType(gfx::Tween::Type tween_type) {
  animator_->set_tween_type(tween_type);
}

gfx::Tween::Type ScopedLayerAnimationSettings::GetTweenType() const {
  return animator_->tween_type();
}

void ScopedLayerAnimationSettings::SetPreemptionStrategy(
    LayerAnimator::PreemptionStrategy strategy) {
  animator_->set_preemption_strategy(strategy);
}

LayerAnimator::PreemptionStrategy
ScopedLayerAnimationSettings::GetPreemptionStrategy() const {
  return animator_->preemption_strategy();
}

void ScopedLayerAnimationSettings::CacheRenderSurface() {
  auto observer = std::make_unique<ScopedRenderSurfaceCaching>(
      animator_->delegate()->GetLayer());
  AddObserverToSettings(this, std::move(observer));
}

void ScopedLayerAnimationSettings::DeferPaint() {
  AddScopedDeferredPaintingObserverRecursive(animator_->delegate()->GetLayer(),
                                             this);
}

void ScopedLayerAnimationSettings::TrilinearFiltering() {
  auto observer = std::make_unique<ScopedTrilinearFiltering>(
      animator_->delegate()->GetLayer());
  AddObserverToSettings(this, std::move(observer));
}

}  // namespace ui
