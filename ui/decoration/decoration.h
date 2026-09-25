// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_DECORATION_H_
#define UI_DECORATION_DECORATION_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_nine_patch.h"
#include "ui/compositor/layer_owner.h"
#include "ui/decoration/decoration_source.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

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
  //
  // `debug_name` describes what the decoration represents (e.g. "Shadow" or
  // "HighlightBorder") and is used to give the decoration's layers
  // context-specific debug names such as "Decoration:Shadow". When empty, the
  // layers are named generically "Decoration".
  static std::unique_ptr<Decoration> Create(
      std::unique_ptr<DecorationSource> source,
      std::string_view debug_name = {});

  explicit Decoration(std::unique_ptr<DecorationSource> source,
                      std::string_view debug_name = {});

  Decoration(const Decoration&) = delete;
  Decoration& operator=(const Decoration&) = delete;

  ~Decoration() override;

  // The debug name given to the decoration layer, e.g. "Decoration:Shadow".
  const std::string& name() const { return name_; }

  DecorationSource* source() { return source_.get(); }
  const DecorationSource* source() const { return source_.get(); }

  // Returns the source as a `T`, or nullptr if it isn't one.
  template <typename T>
  T* GetSourceAs() {
    return source()->AsA<T>();
  }
  template <typename T>
  const T* GetSourceAs() const {
    return source()->AsA<T>();
  }

  // Moves and resizes the decoration layer to frame |content_bounds|.
  // This should be used to adjust the decoration's size and position (rather
  // than applying transformations to the `layer()` of this Decoration).
  void SetContentBounds(const gfx::Rect& content_bounds);
  const gfx::Rect& content_bounds() const { return content_bounds_; }

  // Sets the radii of the corners of the content this decoration frames.
  void SetRoundedCorners(const gfx::RoundedCornersF& rounded_corners);
  const gfx::RoundedCornersF& rounded_corners() const {
    return rounded_corners_;
  }

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

  // Debug name for the decoration layer, e.g. "Decoration:Shadow".
  const std::string name_;

  // Bounds of the content that the decoration encloses, and the radii of that
  // content's corners.
  gfx::Rect content_bounds_;
  gfx::RoundedCornersF rounded_corners_;

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
