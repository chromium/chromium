// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DECORATION_DECORATION_DETAILS_H_
#define UI_DECORATION_DECORATION_DETAILS_H_

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui::decoration {

// Defines the requirements for a specification used by DecorationDetails.
template <typename Spec>
concept DecorationSpec = requires(const Spec& a, const Spec& b) {
  { a == b } -> std::same_as<bool>;
  { a < b } -> std::same_as<bool>;
};

// Defines the requirements for a generator used by DecorationDetails.
//
// - `GetMargins()`: Outsets from content bounds to the decoration's outer edge.
// - `GetNineboxApertureInsets()`: Insets from the ninebox image edges to its
//   stretchable center tile (total space needed for the decoration and corner
//   rounding).
// - `Draw()`: Paints the decoration around `content_rect` without leaving
//   persistent state (e.g., clips or transforms) on the canvas.
template <typename Generator, typename Spec>
concept DecorationGenerator =
    DecorationSpec<Spec> &&
    requires(gfx::Canvas* canvas,
             const Spec& spec,
             const gfx::RoundedCornersF& rounded_corners,
             const gfx::Rect& content_rect) {
      { Generator::GetMargins(spec) } -> std::same_as<gfx::Outsets>;
      {
        Generator::GetNineboxApertureInsets(spec, rounded_corners)
      } -> std::same_as<gfx::Insets>;
      {
        Generator::Draw(canvas, spec, rounded_corners, content_rect)
      } -> std::same_as<void>;
    };

template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
struct DecorationDetails;

namespace internal {

// Creates an image with decorations painted around a rounded rect with the
// given corner radii. The image is sized just large enough to paint the
// decoration with a 1px square center aperture.
template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
class NineboxImageSource : public gfx::CanvasImageSource {
 public:
  NineboxImageSource(const Spec& spec,
                     const gfx::RoundedCornersF& rounded_corners,
                     const gfx::Insets& aperture_insets,
                     const gfx::Outsets& margins)
      : gfx::CanvasImageSource(CalculateSize(aperture_insets)),
        spec_(spec),
        rounded_corners_(rounded_corners),
        margins_(margins) {}

  NineboxImageSource(const NineboxImageSource&) = delete;
  NineboxImageSource& operator=(const NineboxImageSource&) = delete;

  ~NineboxImageSource() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    // Outsetting by `-margins_` walks inwards from the image edge to the
    // content edge.
    gfx::Rect content_rect(size());
    content_rect.Outset(-margins_);
    Generator::Draw(canvas, spec_, rounded_corners_, content_rect);
  }

 private:
  static gfx::Size CalculateSize(const gfx::Insets& aperture_insets) {
    // The "content" area (the middle tile in the 3x3 grid) is a single pixel.
    gfx::Rect bounds(0, 0, 1, 1);
    bounds.Inset(-aperture_insets);
    return bounds.size();
  }

  const Spec spec_;
  const gfx::RoundedCornersF rounded_corners_;
  const gfx::Outsets margins_;
};

// Generic cache for decoration details.
template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
class DecorationCache {
 public:
  static const DecorationDetails<Spec, Generator>& Get(
      const gfx::RoundedCornersF& rounded_corners,
      const Spec& spec) {
    auto& cache = GetCache();
    Key key{rounded_corners, spec};
    auto iter = cache.find(key);
    if (iter != cache.end()) {
      return iter->second;
    }

    // Evict the details whose ninebox image does not have any owners.
    base::EraseIf(cache, [](auto& pair) {
      return pair.second.nine_patch_image.IsUniquelyOwned();
    });

    const gfx::Insets aperture_insets =
        Generator::GetNineboxApertureInsets(spec, rounded_corners);
    const gfx::Outsets margins = Generator::GetMargins(spec);

    auto source = std::make_unique<NineboxImageSource<Spec, Generator>>(
        spec, rounded_corners, aperture_insets, margins);
    const gfx::Size image_size = source->size();
    auto nine_patch_image = gfx::ImageSkia(std::move(source), image_size);
    auto [inserted_iter, success] = cache.try_emplace(
        key, spec, nine_patch_image, aperture_insets, margins);
    DCHECK(success);
    return inserted_iter->second;
  }

  static size_t GetCacheSize() { return GetCache().size(); }

 private:
  struct Key {
    gfx::RoundedCornersF rounded_corners;
    Spec spec;

    bool operator==(const Key& other) const {
      return rounded_corners == other.rounded_corners && spec == other.spec;
    }

    bool operator<(const Key& other) const {
      if (rounded_corners != other.rounded_corners) {
        return gfx::RoundedCornersF::Compare(rounded_corners,
                                             other.rounded_corners);
      }
      return spec < other.spec;
    }
  };

  using CacheMap = base::flat_map<Key, DecorationDetails<Spec, Generator>>;

  static CacheMap& GetCache() {
    static base::NoDestructor<CacheMap> cache;
    return *cache;
  }
};

}  // namespace internal

// A struct that describes a visual decoration and its depiction as an image
// suitable for ninebox tiling.
template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
struct DecorationDetails {
  DecorationDetails(const Spec& spec,
                    const gfx::ImageSkia& nine_patch_image,
                    const gfx::Insets& aperture_insets,
                    const gfx::Outsets& margins)
      : spec(spec),
        nine_patch_image(nine_patch_image),
        aperture_insets(aperture_insets),
        margins(margins) {}

  DecorationDetails(const DecorationDetails& other) = default;
  DecorationDetails& operator=(const DecorationDetails& other) = default;

  DecorationDetails(DecorationDetails&& other) = default;
  DecorationDetails& operator=(DecorationDetails&& other) = default;

  ~DecorationDetails() = default;

  bool operator==(const DecorationDetails& other) const {
    return spec == other.spec && aperture_insets == other.aperture_insets &&
           margins == other.margins &&
           nine_patch_image.BackedBySameObjectAs(other.nine_patch_image);
  }

  // Returns a cached DecorationDetails for given corner radius and decoration
  // specification. Creates the DecorationDetails first if necessary.
  static const DecorationDetails<Spec, Generator>& Get(
      const gfx::RoundedCornersF& rounded_corners,
      const Spec& spec) {
    return internal::DecorationCache<Spec, Generator>::Get(rounded_corners,
                                                           spec);
  }

  static size_t GetDetailsCacheSizeForTest() {
    return internal::DecorationCache<Spec, Generator>::GetCacheSize();
  }

  // Visual specification of the decoration.
  Spec spec;
  // Cached ninebox image based on |spec|.
  gfx::ImageSkia nine_patch_image;
  // Insets for the stretchable center aperture grid.
  gfx::Insets aperture_insets;
  // Outset margins for positioning the decoration around content bounds.
  gfx::Outsets margins;
};

// Visual specification for several decorations rasterized into a single
// ninebox image. Each specification is optional so a composite can omit any
// subset of its parts while still sharing a single cache type.
template <DecorationSpec... Specs>
class CompositeValue {
 public:
  static_assert(sizeof...(Specs) >= 2, "A composite needs at least two specs.");

  CompositeValue() = default;

  explicit CompositeValue(std::optional<Specs>... specs)
      : specs_(std::move(specs)...) {}

  CompositeValue(const CompositeValue&) = default;
  CompositeValue& operator=(const CompositeValue&) = default;
  CompositeValue(CompositeValue&&) = default;
  CompositeValue& operator=(CompositeValue&&) = default;

  ~CompositeValue() = default;

  // Returns true when no part has a specification to draw.
  bool IsEmpty() const { return (!Get<Specs>().has_value() && ...); }

  // Returns the optional specification for `Spec`.
  template <DecorationSpec Spec>
  const std::optional<Spec>& Get() const {
    return std::get<std::optional<Spec>>(specs_);
  }

  friend bool operator==(const CompositeValue&,
                         const CompositeValue&) = default;

  friend bool operator<(const CompositeValue& lhs, const CompositeValue& rhs) {
    return lhs.specs_ < rhs.specs_;
  }

 private:
  std::tuple<std::optional<Specs>...> specs_;
};

template <typename... Details>
struct CompositeGenerator;

// Measures and rasterizes several decorations into a single ninebox image
// around a shared content rect.
template <typename... Specs, typename... Generators>
  requires(DecorationGenerator<Generators, Specs> && ...)
struct CompositeGenerator<DecorationDetails<Specs, Generators>...> {
  using Value = CompositeValue<Specs...>;

  static gfx::Outsets GetMargins(const Value& value) {
    DCHECK(!value.IsEmpty());
    gfx::Outsets margins;
    (AccumulateMargins<Specs, Generators>(value, margins), ...);
    return margins;
  }

  static gfx::Insets GetNineboxApertureInsets(
      const Value& value,
      const gfx::RoundedCornersF& rounded_corners) {
    DCHECK(!value.IsEmpty());
    gfx::Insets inward_insets;
    (AccumulateInwardAperture<Specs, Generators>(value, rounded_corners,
                                                 inward_insets),
     ...);
    // `ToInsets()` negates components, so `-GetMargins(value).ToInsets()` is
    // the positive outward reach as `gfx::Insets`.
    return inward_insets - GetMargins(value).ToInsets();
  }

  static void Draw(gfx::Canvas* canvas,
                   const Value& value,
                   const gfx::RoundedCornersF& rounded_corners,
                   const gfx::Rect& content_rect) {
    DCHECK(!value.IsEmpty());
    // Painter's order: parts are drawn in the order they are listed, so a part
    // listed later sits on top of one listed earlier.
    (DrawPart<Specs, Generators>(canvas, value, rounded_corners, content_rect),
     ...);
  }

 private:
  template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
  static void AccumulateMargins(const Value& value, gfx::Outsets& margins) {
    if (const auto& spec = value.template Get<Spec>()) {
      margins.SetToMax(Generator::GetMargins(*spec));
    }
  }

  template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
  static void AccumulateInwardAperture(
      const Value& value,
      const gfx::RoundedCornersF& rounded_corners,
      gfx::Insets& inward_insets) {
    if (const auto& spec = value.template Get<Spec>()) {
      // Each part's aperture is measured from its own outer edge
      // (`GetMargins()` outside the content). Since `ToInsets()` negates
      // components, adding `GetMargins().ToInsets()` subtracts the part's
      // outward reach, leaving how far its corner patch reaches inside the
      // shared content rect.
      inward_insets.SetToMax(
          Generator::GetNineboxApertureInsets(*spec, rounded_corners) +
          Generator::GetMargins(*spec).ToInsets());
    }
  }

  template <DecorationSpec Spec, DecorationGenerator<Spec> Generator>
  static void DrawPart(gfx::Canvas* canvas,
                       const Value& value,
                       const gfx::RoundedCornersF& rounded_corners,
                       const gfx::Rect& content_rect) {
    if (const auto& spec = value.template Get<Spec>()) {
      Generator::Draw(canvas, *spec, rounded_corners, content_rect);
    }
  }
};

}  // namespace ui::decoration

#endif  // UI_DECORATION_DECORATION_DETAILS_H_
