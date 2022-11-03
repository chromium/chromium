// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/temp_palette.h"

#include <algorithm>

namespace ui {

namespace {

constexpr SkColor kPurple = 0xffca35c6U;
constexpr SkColor kTeal = 0xff3cc3b8U;

ToneMap PurpleTones() {
  ToneMap tones;

  tones.seed = kPurple;  // purple
  tones.primary[Luma::k100] = 0xffffffffU;
  tones.primary[Luma::k99] = 0xfffffbffU;
  tones.primary[Luma::k95] = 0xffffebf7U;
  tones.primary[Luma::k90] = 0xffffd7f5U;
  tones.primary[Luma::k80] = 0xffffabf3U;
  tones.primary[Luma::k70] = 0xffff76f5U;
  tones.primary[Luma::k60] = 0xffe651e0U;
  tones.primary[Luma::k50] = 0xffc732c3U;
  tones.primary[Luma::k40] = 0xffa901a8U;
  tones.primary[Luma::k30] = 0xff810080U;
  tones.primary[Luma::k20] = 0xff5b005bU;
  tones.primary[Luma::k10] = 0xff380038U;
  tones.primary[Luma::k0] = 0xff000000U;

  tones.secondary[Luma::k100] = 0xffffffffU;
  tones.secondary[Luma::k99] = 0xfffffbffU;
  tones.secondary[Luma::k95] = 0xffffebf7U;
  tones.secondary[Luma::k90] = 0xfff7daefU;
  tones.secondary[Luma::k80] = 0xffdabfd2U;
  tones.secondary[Luma::k70] = 0xffbea4b7U;
  tones.secondary[Luma::k60] = 0xffa2899cU;
  tones.secondary[Luma::k50] = 0xff877082U;
  tones.secondary[Luma::k40] = 0xff6e5869U;
  tones.secondary[Luma::k30] = 0xff554151U;
  tones.secondary[Luma::k20] = 0xff3d2b3aU;
  tones.secondary[Luma::k10] = 0xff271624U;
  tones.secondary[Luma::k0] = 0xff000000U;

  tones.tertiary[Luma::k100] = 0xffffffffU;
  tones.tertiary[Luma::k99] = 0xfffffbffU;
  tones.tertiary[Luma::k95] = 0xffffede8U;
  tones.tertiary[Luma::k90] = 0xffffdbd1U;
  tones.tertiary[Luma::k80] = 0xfff5b8a7U;
  tones.tertiary[Luma::k70] = 0xffd79e8dU;
  tones.tertiary[Luma::k60] = 0xffba8474U;
  tones.tertiary[Luma::k50] = 0xff9d6b5cU;
  tones.tertiary[Luma::k40] = 0xff815345U;
  tones.tertiary[Luma::k30] = 0xff663c2fU;
  tones.tertiary[Luma::k20] = 0xff4c261bU;
  tones.tertiary[Luma::k10] = 0xff321208U;
  tones.tertiary[Luma::k0] = 0xff000000U;

  tones.neutral1[Luma::k100] = 0xffffffffU;
  tones.neutral1[Luma::k99] = 0xfffffbffU;
  tones.neutral1[Luma::k95] = 0xfff8eef2U;
  tones.neutral1[Luma::k90] = 0xffe9e0e4U;
  tones.neutral1[Luma::k80] = 0xffcdc4c8U;
  tones.neutral1[Luma::k70] = 0xffb1a9adU;
  tones.neutral1[Luma::k60] = 0xff968f92U;
  tones.neutral1[Luma::k50] = 0xff7c7579U;
  tones.neutral1[Luma::k40] = 0xff635d60U;
  tones.neutral1[Luma::k30] = 0xff4b4548U;
  tones.neutral1[Luma::k20] = 0xff342f32U;
  tones.neutral1[Luma::k10] = 0xff1e1a1dU;
  tones.neutral1[Luma::k0] = 0xff000000U;

  tones.neutral2[Luma::k100] = 0xffffffffU;
  tones.neutral2[Luma::k99] = 0xfffffbffU;
  tones.neutral2[Luma::k95] = 0xfffcecf5U;
  tones.neutral2[Luma::k90] = 0xffeedee7U;
  tones.neutral2[Luma::k80] = 0xffd1c2cbU;
  tones.neutral2[Luma::k70] = 0xffb5a7b0U;
  tones.neutral2[Luma::k60] = 0xff9a8d95U;
  tones.neutral2[Luma::k50] = 0xff80747bU;
  tones.neutral2[Luma::k40] = 0xff665b63U;
  tones.neutral2[Luma::k30] = 0xff4e444bU;
  tones.neutral2[Luma::k20] = 0xff372e34U;
  tones.neutral2[Luma::k10] = 0xff21191fU;
  tones.neutral2[Luma::k0] = 0xff000000U;

  return tones;
}

ToneMap TealTones() {
  ToneMap tones;

  tones.seed = kTeal;  // teal
  tones.primary[Luma::k100] = 0xffffffffU;
  tones.primary[Luma::k99] = 0xfff2fffcU;
  tones.primary[Luma::k95] = 0xffb2fff6U;
  tones.primary[Luma::k90] = 0xff71f7ebU;
  tones.primary[Luma::k80] = 0xff50dbcfU;
  tones.primary[Luma::k70] = 0xff26bfb3U;
  tones.primary[Luma::k60] = 0xff00a298U;
  tones.primary[Luma::k50] = 0xff00867dU;
  tones.primary[Luma::k40] = 0xff006a63U;
  tones.primary[Luma::k30] = 0xff00504bU;
  tones.primary[Luma::k20] = 0xff003733U;
  tones.primary[Luma::k10] = 0xff00201dU;
  tones.primary[Luma::k0] = 0xff000000U;

  tones.secondary[Luma::k100] = 0xffffffffU;
  tones.secondary[Luma::k99] = 0xfff2fffcU;
  tones.secondary[Luma::k95] = 0xffdaf7f2U;
  tones.secondary[Luma::k90] = 0xffcce8e4U;
  tones.secondary[Luma::k80] = 0xffb1ccc8U;
  tones.secondary[Luma::k70] = 0xff96b1adU;
  tones.secondary[Luma::k60] = 0xff7b9692U;
  tones.secondary[Luma::k50] = 0xff627c79U;
  tones.secondary[Luma::k40] = 0xff4a6360U;
  tones.secondary[Luma::k30] = 0xff324b48U;
  tones.secondary[Luma::k20] = 0xff1c3532U;
  tones.secondary[Luma::k10] = 0xff051f1dU;
  tones.secondary[Luma::k0] = 0xff000000U;

  tones.tertiary[Luma::k100] = 0xffffffffU;
  tones.tertiary[Luma::k99] = 0xfffcfcffU;
  tones.tertiary[Luma::k95] = 0xffe8f2ffU;
  tones.tertiary[Luma::k90] = 0xffcee5ffU;
  tones.tertiary[Luma::k80] = 0xffafc9e7U;
  tones.tertiary[Luma::k70] = 0xff94aecaU;
  tones.tertiary[Luma::k60] = 0xff7993afU;
  tones.tertiary[Luma::k50] = 0xff607994U;
  tones.tertiary[Luma::k40] = 0xff47617aU;
  tones.tertiary[Luma::k30] = 0xff2f4961U;
  tones.tertiary[Luma::k20] = 0xff17324aU;
  tones.tertiary[Luma::k10] = 0xff001d33U;
  tones.tertiary[Luma::k0] = 0xff000000U;

  tones.neutral1[Luma::k100] = 0xffffffffU;
  tones.neutral1[Luma::k99] = 0xfffafdfbU;
  tones.neutral1[Luma::k95] = 0xffeff1f0U;
  tones.neutral1[Luma::k90] = 0xffe0e3e1U;
  tones.neutral1[Luma::k80] = 0xffc4c7c6U;
  tones.neutral1[Luma::k70] = 0xffa9acaaU;
  tones.neutral1[Luma::k60] = 0xff8e9190U;
  tones.neutral1[Luma::k50] = 0xff747877U;
  tones.neutral1[Luma::k40] = 0xff5b5f5eU;
  tones.neutral1[Luma::k30] = 0xff444747U;
  tones.neutral1[Luma::k20] = 0xff2d3130U;
  tones.neutral1[Luma::k10] = 0xff191c1cU;
  tones.neutral1[Luma::k0] = 0xff000000U;

  tones.neutral2[Luma::k100] = 0xffffffffU;
  tones.neutral2[Luma::k99] = 0xfff4fefcU;
  tones.neutral2[Luma::k95] = 0xffe9f3f0U;
  tones.neutral2[Luma::k90] = 0xffdae5e2U;
  tones.neutral2[Luma::k80] = 0xffbec9c6U;
  tones.neutral2[Luma::k70] = 0xffa3adabU;
  tones.neutral2[Luma::k60] = 0xff899391U;
  tones.neutral2[Luma::k50] = 0xff6f7977U;
  tones.neutral2[Luma::k40] = 0xff56605fU;
  tones.neutral2[Luma::k30] = 0xff3f4947U;
  tones.neutral2[Luma::k20] = 0xff293231U;
  tones.neutral2[Luma::k10] = 0xff141d1cU;
  tones.neutral2[Luma::k0] = 0xff000000U;

  return tones;
}

void AddErrorColors(ToneMap& tones) {
  tones.error[Luma::k100] = 0xffffffffU;
  tones.error[Luma::k99] = 0xfffffbffU;
  tones.error[Luma::k95] = 0xffffedeaU;
  tones.error[Luma::k90] = 0xffffdad6U;
  tones.error[Luma::k80] = 0xffffb4abU;
  tones.error[Luma::k70] = 0xffff897dU;
  tones.error[Luma::k60] = 0xffff5449U;
  tones.error[Luma::k50] = 0xffde3730U;
  tones.error[Luma::k40] = 0xffba1a1aU;
  tones.error[Luma::k30] = 0xff93000aU;
  tones.error[Luma::k20] = 0xff690005U;
  tones.error[Luma::k10] = 0xff410002U;
  tones.error[Luma::k0] = 0xff000000U;
}

// Extracts the hue for |color| in degrees [0-360).
SkScalar GetHue(SkColor color) {
  SkScalar hsv[3];
  SkColorToHSV(color, hsv);
  // Index 0 is hue.
  return hsv[0];
}

// Returns the difference in hue of two colors in degrees.
SkScalar HueDistance(SkColor a, SkColor b) {
  SkScalar hue_a = GetHue(a);
  SkScalar hue_b = GetHue(b);

  return std::min(360 - std::abs(hue_a - hue_b), std::abs(hue_a - hue_b));
}

}  // namespace

ToneMap::ToneMap() = default;
ToneMap::ToneMap(const ToneMap&) = default;
ToneMap::~ToneMap() = default;

const ToneMap GetTempPalette(SkColor seed_color) {
  const SkScalar teal_distance = HueDistance(seed_color, kTeal);
  const SkScalar purple_distance = HueDistance(seed_color, kPurple);
  ToneMap tones =
      (teal_distance < purple_distance) ? TealTones() : PurpleTones();
  AddErrorColors(tones);
  return tones;
}

}  // namespace ui
