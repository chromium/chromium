// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_DECORATION_H_
#define UI_DECORATION_DECORATION_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor/layer_owner.h"
#include "ui/decoration/decoration_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"

namespace ui::decoration {

// Manages the compositor layers that frame a surface with a decoration
// (a shadow, a highlight border, or both) on a single NinePatchLayer, and
// animates appearance changes.
//
// Decoration is the only implementation of the nine-patch layer plumbing; what
// the decoration actually looks like is entirely delegated to the
// DecorationSource.
class Decoration final : public ui::ImplicitAnimationObserver,
                         public ui::LayerOwner {
 public:
  // Creates an initialized decoration drawn by `source`.
  static std::unique_ptr<Decoration> Create(
      std::unique_ptr<DecorationSource> source);

  explicit Decoration(std::unique_ptr<DecorationSource> source);

  Decoration(const Decoration&) = delete;
  Decoration& operator=(const Decoration&) = delete;

  ~Decoration() override;

  DecorationSource* source() { return source_.get(); }
  const DecorationSource* source() const { return source_.get(); }

  // Moves and resizes the decoration layer to frame |content_bounds|.
  // This should be used to adjust the decoration's size and position (rather
  // than applying transformations to the `layer()` of this Decoration).
  void SetContentBounds(const gfx::RRectF& content_bounds);
  const gfx::RRectF& content_bounds() const { return content_bounds_; }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  ui::LayerNinePatch* decoration_layer_for_testing() {
    return decoration_layer();
  }
  ui::LayerNinePatch* fading_layer_for_testing() { return fading_layer(); }

 private:
  // A layer owner that correctly updates the nine patch layer details
  // when it gets recreated.
  class DecorationLayerOwner : public ui::LayerOwner {
   public:
    explicit DecorationLayerOwner(Decoration* owner,
                                  std::unique_ptr<ui::Layer> layer = nullptr);

    DecorationLayerOwner(const DecorationLayerOwner&) = delete;
    DecorationLayerOwner& operator=(const DecorationLayerOwner&) = delete;

    ~DecorationLayerOwner() override;

    // ui::LayerOwner:
    std::unique_ptr<ui::Layer> RecreateLayer() override;

   private:
    const raw_ptr<Decoration> owner_decoration_;
  };

  void RecreateDecorationLayer();
  void UpdateAppearance(
      std::optional<base::TimeDelta> cross_fade_duration = std::nullopt);
  void UpdateAppearanceImmediately();
  void CrossFadeToNewAppearance(base::TimeDelta duration);

  ui::LayerNinePatch* decoration_layer() {
    ui::Layer* layer = decoration_layer_owner_.layer();
    return layer ? layer->AsNinePatch() : nullptr;
  }

  ui::LayerNinePatch* fading_layer() {
    ui::Layer* layer = fading_layer_owner_.layer();
    return layer ? layer->AsNinePatch() : nullptr;
  }

  // Draws this decoration. Never null.
  const std::unique_ptr<DecorationSource> source_;

  // Bounds of the content that the decoration encloses, carrying its corner
  // radii clamped to fit.
  gfx::RRectF content_bounds_;

  // Currently active appearance set on `decoration_layer()`.
  std::optional<DecorationSource::Appearance> active_appearance_;

  // The owner of the actual decoration layer corresponding to a
  // cc::NinePatchLayer.
  DecorationLayerOwner decoration_layer_owner_;

  // When the appearance changes, the old decoration cross-fades with the new
  // one. When non-null, this owns an old |decoration_layer()| that's being
  // animated out.
  ui::LayerOwner fading_layer_owner_;

  // The layer bounds since content bounds were last set.
  gfx::Rect last_layer_bounds_;
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_DECORATION_H_
