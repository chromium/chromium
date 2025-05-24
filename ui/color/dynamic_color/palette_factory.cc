// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/dynamic_color/palette_factory.h"

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "third_party/material_color_utilities/src/cpp/cam/hct.h"
#include "third_party/material_color_utilities/src/cpp/palettes/core.h"
#include "third_party/material_color_utilities/src/cpp/palettes/tones.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/dynamic_color/palette.h"

namespace ui {

namespace {

using material_color_utilities::Hct;
using SchemeVariant = ColorProviderKey::SchemeVariant;

// Returns the hue angle for `source_color` modified by the closest match in
// `hues_to_rotations`.
double GetRotatedHue(double source_hue,
                     const base::flat_map<double, double>& hues_to_rotations) {
  if (hues_to_rotations.size() == 1) {
    return material_color_utilities::SanitizeDegreesDouble(
        source_hue + hues_to_rotations.begin()->second);
  }

  auto it = hues_to_rotations.lower_bound(source_hue);
  if (it == hues_to_rotations.end()) {
    return source_hue;
  }

  return material_color_utilities::SanitizeDegreesDouble(source_hue +
                                                         it->second);
}

// Returns the chroma value from `hues_to_chroma` given a `source_hue`. If the
// `source_hue` is out of range, the first entry in `hues_to_chroma` is
// returned.
double GetAdjustedChroma(
    double source_hue,
    const base::flat_map<double, double>& hues_to_chromas) {
  CHECK_GE(source_hue, -0.0);
  CHECK_LE(source_hue, 360.0);
  CHECK(!hues_to_chromas.empty());

  auto iter = hues_to_chromas.lower_bound(source_hue);
  if (iter == hues_to_chromas.end()) {
    return hues_to_chromas.begin()->second;
  }

  return iter->second;
}

class CustomPalette : public Palette {
 public:
  CustomPalette(TonalPalette&& primary,
                TonalPalette&& secondary,
                TonalPalette&& tertiary,
                TonalPalette&& neutral,
                TonalPalette&& neutral_variant,
                TonalPalette&& error)
      : primary_(primary),
        secondary_(secondary),
        tertiary_(tertiary),
        neutral_(neutral),
        neutral_variant_(neutral_variant),
        error_(error) {}

  const TonalPalette& primary() const override { return primary_; }

  const TonalPalette& secondary() const override { return secondary_; }

  const TonalPalette& tertiary() const override { return tertiary_; }

  const TonalPalette& neutral() const override { return neutral_; }

  const TonalPalette& neutral_variant() const override {
    return neutral_variant_;
  }

  const TonalPalette& error() const override { return error_; }

 private:
  const TonalPalette primary_;
  const TonalPalette secondary_;
  const TonalPalette tertiary_;
  const TonalPalette neutral_;
  const TonalPalette neutral_variant_;
  const TonalPalette error_;
};

struct Transform {
  Transform() = default;
  Transform(double hue_rotation,
            double chroma,
            std::optional<base::flat_map<double, double>> hues_to_rotations =
                std::nullopt)
      : hue_rotation(hue_rotation),
        chroma(chroma),
        hues_to_rotations(hues_to_rotations) {}
  explicit Transform(base::flat_map<double, double> hues_to_chroma)
      : hues_to_chroma(hues_to_chroma) {}

  double hue_rotation = 0.0;
  double chroma = 0.0;
  std::optional<base::flat_map<double, double>> hues_to_rotations =
      std::nullopt;
  std::optional<base::flat_map<double, double>> hues_to_chroma = std::nullopt;
};

Transform Chroma(double chroma) {
  Transform transform;
  transform.chroma = chroma;
  return transform;
}

// Specifies the `Transform`s relative to the source color that generates a
// Palette.
struct Config {
  Transform primary;
  Transform secondary;
  Transform tertiary;
  Transform neutral;
  Transform neutral_variant;
};

// Returns a `TonalPalette` constructed from `hue` transformed by `transform`.
TonalPalette MakePalette(double hue, const Transform& transform) {
  CHECK_LE(hue, 360.0);
  double chroma = transform.chroma;
  if (transform.hues_to_chroma) {
    chroma = GetAdjustedChroma(hue, *transform.hues_to_chroma);
  }
  if (transform.hues_to_rotations) {
    hue = GetRotatedHue(hue, *transform.hues_to_rotations);
  } else {
    hue = material_color_utilities::SanitizeDegreesDouble(
        hue + transform.hue_rotation);
  }
  return TonalPalette(hue, chroma);
}

std::unique_ptr<Palette> FromConfig(SkColor seed_color, const Config& config) {
  // TODO(skau): Make this const when get_hue() is marked const.
  Hct source_hct(seed_color);
  double hue = source_hct.get_hue();

  return std::make_unique<CustomPalette>(
      MakePalette(hue, config.primary), MakePalette(hue, config.secondary),
      MakePalette(hue, config.tertiary), MakePalette(hue, config.neutral),
      MakePalette(hue, config.neutral_variant), TonalPalette(25.0, 84.0));
}

// Returns a `base::flat_map` from two arrays of equal size.
template <class S, class T, size_t N>
base::flat_map<S, T> Zip(const std::array<S, N>& keys,
                         const std::array<T, N>& values) {
  CHECK_EQ(keys.size(), values.size());
  std::vector<std::pair<S, T>> zipped;
  zipped.reserve(keys.size());
  for (size_t i = 0; i < keys.size(); i++) {
    zipped.push_back(std::make_pair(keys[i], values[i]));
  }
  return base::flat_map<S, T>(base::sorted_unique_t(), std::move(zipped));
}

}  // namespace

std::unique_ptr<Palette> GeneratePalette(SkColor seed_color,
                                         SchemeVariant variant) {
  Config config;
  switch (variant) {
    case SchemeVariant::kTonalSpot:
      config = {Chroma(40.0), Chroma(16.0), Transform{60.0, 24.0}, Chroma(6.0),
                Chroma(8.0)};
      break;
    case SchemeVariant::kVibrant: {
      const auto hues =
          std::to_array<double>({0, 41, 61, 101, 131, 181, 251, 301, 360});
      const auto secondary_rotations =
          std::to_array<double>({18, 15, 10, 12, 15, 18, 15, 12, 12});
      const base::flat_map<double, double> secondary_hues_to_rotations =
          Zip(hues, secondary_rotations);

      const auto tertiary_rotations =
          std::to_array<double>({35, 30, 20, 25, 30, 35, 30, 25, 25});
      const base::flat_map<double, double> tertiary_hues_to_rotations =
          Zip(hues, tertiary_rotations);

      config = {Chroma(200.0),
                Transform(0.0, 24.0, secondary_hues_to_rotations),
                Transform(0.0, 32.0, tertiary_hues_to_rotations), Chroma(8.0),
                Chroma(12.0)};
      break;
    }
    case SchemeVariant::kNeutral: {
      const auto hues = std::to_array<double>({0, 260, 315, 360});
      const auto chromas = std::to_array<double>({12.0, 12.0, 20.0, 12.0});
      base::flat_map<double, double> chroma_transforms = Zip(hues, chromas);
      config = {Transform(std::move(chroma_transforms)), Chroma(8.0),
                Chroma(16.0), Chroma(2.0), Chroma(2.0)};
      break;
    }
    case SchemeVariant::kExpressive: {
      const auto hues =
          std::to_array<double>({0, 21, 51, 121, 151, 191, 271, 321, 360});
      const auto secondary_rotations =
          std::to_array<double>({45, 95, 45, 20, 45, 90, 45, 45, 45});
      const base::flat_map<double, double> secondary_hues_to_rotations =
          Zip(hues, secondary_rotations);

      const auto tertiary_rotations =
          std::to_array<double>({120, 120, 20, 45, 20, 15, 20, 120, 120});
      const base::flat_map<double, double> tertiary_hues_to_rotations =
          Zip(hues, tertiary_rotations);
      config = {Transform(-90, 40.0),
                Transform(0.0, 24.0, secondary_hues_to_rotations),
                Transform(0.0, 32.0, tertiary_hues_to_rotations), Chroma(8.0),
                Chroma(12.0)};
      break;
    }
  }

  return FromConfig(seed_color, config);
}

}  // namespace ui
