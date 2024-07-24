// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/skbitmap_operations.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>

#include "base/check_op.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"
#include "third_party/skia/include/effects/SkImageFilters.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

static bool IsUninitializedBitmap(const SkBitmap& bitmap) {
  return bitmap.isNull() && bitmap.colorType() == kUnknown_SkColorType &&
         bitmap.alphaType() == kUnknown_SkAlphaType;
}

// static
SkBitmap SkBitmapOperations::CreateInvertedBitmap(const SkBitmap& image) {
  if (IsUninitializedBitmap(image))
    return image;
  CHECK_EQ(image.colorType(), kN32_SkColorType);

  SkBitmap inverted;
  inverted.allocN32Pixels(image.width(), image.height());

  for (int y = 0; y < image.height(); ++y) {
    uint32_t* image_row = image.getAddr32(0, y);
    uint32_t* dst_row = inverted.getAddr32(0, y);

    for (int x = 0; x < image.width(); ++x) {
      uint32_t image_pixel = image_row[x];
      dst_row[x] = (image_pixel & 0xFF000000) |
                   (0x00FFFFFF - (image_pixel & 0x00FFFFFF));
    }
  }

  return inverted;
}

// static
SkBitmap SkBitmapOperations::CreateBlendedBitmap(const SkBitmap& first,
                                                 const SkBitmap& second,
                                                 double alpha) {
  DCHECK((alpha >= 0) && (alpha <= 1));
  CHECK_EQ(first.width(), second.width());
  CHECK_EQ(first.height(), second.height());
  CHECK_EQ(first.colorType(), kN32_SkColorType);
  CHECK_EQ(second.colorType(), kN32_SkColorType);

  // Optimize for case where we won't need to blend anything.
  static const double alpha_min = 1.0 / 255;
  static const double alpha_max = 254.0 / 255;
  if (alpha < alpha_min)
    return first;
  else if (alpha > alpha_max)
    return second;

  SkBitmap blended;
  blended.allocN32Pixels(first.width(), first.height());

  double first_alpha = 1 - alpha;

  for (int y = 0; y < first.height(); ++y) {
    uint32_t* first_row = first.getAddr32(0, y);
    uint32_t* second_row = second.getAddr32(0, y);
    uint32_t* dst_row = blended.getAddr32(0, y);

    for (int x = 0; x < first.width(); ++x) {
      uint32_t first_pixel = first_row[x];
      uint32_t second_pixel = second_row[x];

      int a = static_cast<int>((SkColorGetA(first_pixel) * first_alpha) +
                               (SkColorGetA(second_pixel) * alpha));
      int r = static_cast<int>((SkColorGetR(first_pixel) * first_alpha) +
                               (SkColorGetR(second_pixel) * alpha));
      int g = static_cast<int>((SkColorGetG(first_pixel) * first_alpha) +
                               (SkColorGetG(second_pixel) * alpha));
      int b = static_cast<int>((SkColorGetB(first_pixel) * first_alpha) +
                               (SkColorGetB(second_pixel) * alpha));

      dst_row[x] = SkColorSetARGB(a, r, g, b);
    }
  }

  return blended;
}

// static
SkBitmap SkBitmapOperations::CreateMaskedBitmap(const SkBitmap& rgb,
                                                const SkBitmap& alpha) {
  CHECK_EQ(rgb.width(), alpha.width());
  CHECK_EQ(rgb.height(), alpha.height());
  CHECK_EQ(rgb.colorType(), kN32_SkColorType);
  CHECK_EQ(alpha.colorType(), kN32_SkColorType);

  SkBitmap masked;
  masked.allocN32Pixels(rgb.width(), rgb.height());

  for (int y = 0; y < masked.height(); ++y) {
    uint32_t* rgb_row = rgb.getAddr32(0, y);
    uint32_t* alpha_row = alpha.getAddr32(0, y);
    uint32_t* dst_row = masked.getAddr32(0, y);

    for (int x = 0; x < masked.width(); ++x) {
      unsigned alpha32 = SkGetPackedA32(alpha_row[x]);
      unsigned scale = SkAlpha255To256(alpha32);
      dst_row[x] = SkAlphaMulQ(rgb_row[x], scale);
    }
  }

  return masked;
}

// static
SkBitmap SkBitmapOperations::CreateButtonBackground(SkColor color,
                                                    const SkBitmap& image,
                                                    const SkBitmap& mask) {
  CHECK_EQ(image.colorType(), kN32_SkColorType);
  CHECK_EQ(mask.colorType(), kN32_SkColorType);

  SkBitmap background;
  background.allocN32Pixels(mask.width(), mask.height());

  double bg_a = SkColorGetA(color);
  double bg_r = SkColorGetR(color) * (bg_a / 255.0);
  double bg_g = SkColorGetG(color) * (bg_a / 255.0);
  double bg_b = SkColorGetB(color) * (bg_a / 255.0);

  for (int y = 0; y < mask.height(); ++y) {
    uint32_t* dst_row = background.getAddr32(0, y);
    uint32_t* image_row = image.getAddr32(0, y % image.height());
    uint32_t* mask_row = mask.getAddr32(0, y);

    for (int x = 0; x < mask.width(); ++x) {
      uint32_t image_pixel = image_row[x % image.width()];

      double img_a = SkColorGetA(image_pixel);
      double img_r = SkColorGetR(image_pixel);
      double img_g = SkColorGetG(image_pixel);
      double img_b = SkColorGetB(image_pixel);

      double img_alpha = img_a / 255.0;
      double img_inv = 1 - img_alpha;

      double mask_a = static_cast<double>(SkColorGetA(mask_row[x])) / 255.0;

      dst_row[x] = SkColorSetARGB(
          // This is pretty weird; why not the usual SrcOver alpha?
          static_cast<int>(std::min(255.0, bg_a + img_a) * mask_a),
          static_cast<int>(((bg_r * img_inv) + (img_r * img_alpha)) * mask_a),
          static_cast<int>(((bg_g * img_inv) + (img_g * img_alpha)) * mask_a),
          static_cast<int>(((bg_b * img_inv) + (img_b * img_alpha)) * mask_a));
    }
  }

  return background;
}

namespace {
namespace HSLShift {

// TODO(viettrungluu): Some things have yet to be optimized at all.

// Notes on and conventions used in the following code
//
// Conventions:
//  - R, G, B, A = obvious; as variables: |r|, |g|, |b|, |a| (see also below)
//  - H, S, L = obvious; as variables: |h|, |s|, |l| (see also below)
//  - variables derived from S, L shift parameters: |sdec| and |sinc| for S
//    increase and decrease factors, |ldec| and |linc| for L (see also below)
//
// To try to optimize HSL shifts, we do several things:
//  - Avoid unpremultiplying (then processing) then premultiplying. This means
//    that R, G, B values (and also L, but not H and S) should be treated as
//    having a range of 0..A (where A is alpha).
//  - Do things in integer/fixed-point. This avoids costly conversions between
//    floating-point and integer, though I should study the tradeoff more
//    carefully (presumably, at some point of processing complexity, converting
//    and processing using simpler floating-point code will begin to win in
//    performance). Also to be studied is the speed/type of floating point
//    conversions; see, e.g., <http://www.stereopsis.com/sree/fpu2006.html>.
//
// Conventions for fixed-point arithmetic
//  - Each function has a constant denominator (called |den|, which should be a
//    power of 2), appropriate for the computations done in that function.
//  - A value |x| is then typically represented by a numerator, named |x_num|,
//    so that its actual value is |x_num / den| (casting to floating-point
//    before division).
//  - To obtain |x_num| from |x|, simply multiply by |den|, i.e., |x_num = x *
//    den| (casting appropriately).
//  - When necessary, a value |x| may also be represented as a numerator over
//    the denominator squared (set |den2 = den * den|). In such a case, the
//    corresponding variable is called |x_num2| (so that its actual value is
//    |x_num^2 / den2|.
//  - The representation of the product of |x| and |y| is be called |x_y_num| if
//    |x * y == x_y_num / den|, and |xy_num2| if |x * y == x_y_num2 / den2|. In
//    the latter case, notice that one can calculate |x_y_num2 = x_num * y_num|.

// Routine used to process a line; typically specialized for specific kinds of
// HSL shifts (to optimize).
typedef void (*LineProcessor)(const color_utils::HSL&,
                              const SkPMColor*,
                              SkPMColor*,
                              int width);

enum OperationOnH { kOpHNone = 0, kOpHShift, kNumHOps };
enum OperationOnS { kOpSNone = 0, kOpSDec, kOpSInc, kNumSOps };
enum OperationOnL { kOpLNone = 0, kOpLDec, kOpLInc, kNumLOps };

// Epsilon used to judge when shift values are close enough to various critical
// values (typically 0.5, which yields a no-op for S and L shifts. 1/256 should
// be small enough, but let's play it safe>
const double epsilon = 0.0005;

// Line processor: default/universal (i.e., old-school).
void LineProcDefault(const color_utils::HSL& hsl_shift,
                     const SkPMColor* in,
                     SkPMColor* out,
                     int width) {
  for (int x = 0; x < width; x++) {
    out[x] = SkPreMultiplyColor(color_utils::HSLShift(
        SkUnPreMultiply::PMColorToColor(in[x]), hsl_shift));
  }
}

// Line processor: no-op (i.e., copy).
void LineProcCopy(const color_utils::HSL& hsl_shift,
                  const SkPMColor* in,
                  SkPMColor* out,
                  int width) {
  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s < 0 || fabs(hsl_shift.s - 0.5) < HSLShift::epsilon);
  DCHECK(hsl_shift.l < 0 || fabs(hsl_shift.l - 0.5) < HSLShift::epsilon);
  memcpy(out, in, static_cast<size_t>(width) * sizeof(out[0]));
}

// Line processor: H no-op, S no-op, L decrease.
void LineProcHnopSnopLdec(const color_utils::HSL& hsl_shift,
                          const SkPMColor* in,
                          SkPMColor* out,
                          int width) {
  const uint32_t den = 65536;

  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s < 0 || fabs(hsl_shift.s - 0.5) < HSLShift::epsilon);
  DCHECK(hsl_shift.l <= 0.5 - HSLShift::epsilon && hsl_shift.l >= 0);

  uint32_t ldec_num = static_cast<uint32_t>(hsl_shift.l * 2 * den);
  for (int x = 0; x < width; x++) {
    uint32_t a = SkGetPackedA32(in[x]);
    uint32_t r = SkGetPackedR32(in[x]);
    uint32_t g = SkGetPackedG32(in[x]);
    uint32_t b = SkGetPackedB32(in[x]);
    r = r * ldec_num / den;
    g = g * ldec_num / den;
    b = b * ldec_num / den;
    out[x] = SkPackARGB32(a, r, g, b);
  }
}

// Line processor: H no-op, S no-op, L increase.
void LineProcHnopSnopLinc(const color_utils::HSL& hsl_shift,
                          const SkPMColor* in,
                          SkPMColor* out,
                          int width) {
  const uint32_t den = 65536;

  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s < 0 || fabs(hsl_shift.s - 0.5) < HSLShift::epsilon);
  DCHECK(hsl_shift.l >= 0.5 + HSLShift::epsilon && hsl_shift.l <= 1);

  uint32_t linc_num = static_cast<uint32_t>((hsl_shift.l - 0.5) * 2 * den);
  for (int x = 0; x < width; x++) {
    uint32_t a = SkGetPackedA32(in[x]);
    uint32_t r = SkGetPackedR32(in[x]);
    uint32_t g = SkGetPackedG32(in[x]);
    uint32_t b = SkGetPackedB32(in[x]);
    r += (a - r) * linc_num / den;
    g += (a - g) * linc_num / den;
    b += (a - b) * linc_num / den;
    out[x] = SkPackARGB32(a, r, g, b);
  }
}

// Saturation changes modifications in RGB
//
// (Note that as a further complication, the values we deal in are
// premultiplied, so R/G/B values must be in the range 0..A. For mathematical
// purposes, one may as well use r=R/A, g=G/A, b=B/A. Without loss of
// generality, assume that R/G/B values are in the range 0..1.)
//
// Let Max = max(R,G,B), Min = min(R,G,B), and Med be the median value. Then L =
// (Max+Min)/2. If L is to remain constant, Max+Min must also remain constant.
//
// For H to remain constant, first, the (numerical) order of R/G/B (from
// smallest to largest) must remain the same. Second, all the ratios
// (R-G)/(Max-Min), (R-B)/(Max-Min), (G-B)/(Max-Min) must remain constant (of
// course, if Max = Min, then S = 0 and no saturation change is well-defined,
// since H is not well-defined).
//
// Let C_max be a colour with value Max, C_min be one with value Min, and C_med
// the remaining colour. Increasing saturation (to the maximum) is accomplished
// by increasing the value of C_max while simultaneously decreasing C_min and
// changing C_med so that the ratios are maintained; for the latter, it suffices
// to keep (C_med-C_min)/(C_max-C_min) constant (and equal to
// (Med-Min)/(Max-Min)).

// Line processor: H no-op, S decrease, L no-op.
void LineProcHnopSdecLnop(const color_utils::HSL& hsl_shift,
                          const SkPMColor* in,
                          SkPMColor* out,
                          int width) {
  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s >= 0 && hsl_shift.s <= 0.5 - HSLShift::epsilon);
  DCHECK(hsl_shift.l < 0 || fabs(hsl_shift.l - 0.5) < HSLShift::epsilon);

  const int32_t denom = 65536;
  int32_t s_numer = static_cast<int32_t>(hsl_shift.s * 2 * denom);
  for (int x = 0; x < width; x++) {
    int32_t a = static_cast<int32_t>(SkGetPackedA32(in[x]));
    int32_t r = static_cast<int32_t>(SkGetPackedR32(in[x]));
    int32_t g = static_cast<int32_t>(SkGetPackedG32(in[x]));
    int32_t b = static_cast<int32_t>(SkGetPackedB32(in[x]));

    int32_t vmax, vmin;
    if (r > g) {  // This uses 3 compares rather than 4.
      vmax = std::max(r, b);
      vmin = std::min(g, b);
    } else {
      vmax = std::max(g, b);
      vmin = std::min(r, b);
    }

    // Use denom * L to avoid rounding.
    int32_t denom_l = (vmax + vmin) * (denom / 2);
    int32_t s_numer_l = (vmax + vmin) * s_numer / 2;

    r = (denom_l + r * s_numer - s_numer_l) / denom;
    g = (denom_l + g * s_numer - s_numer_l) / denom;
    b = (denom_l + b * s_numer - s_numer_l) / denom;
    out[x] = SkPackARGB32(a, r, g, b);
  }
}

// Line processor: H no-op, S decrease, L decrease.
void LineProcHnopSdecLdec(const color_utils::HSL& hsl_shift,
                          const SkPMColor* in,
                          SkPMColor* out,
                          int width) {
  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s >= 0 && hsl_shift.s <= 0.5 - HSLShift::epsilon);
  DCHECK(hsl_shift.l >= 0 && hsl_shift.l <= 0.5 - HSLShift::epsilon);

  // Can't be too big since we need room for denom*denom and a bit for sign.
  const int32_t denom = 1024;
  int32_t l_numer = static_cast<int32_t>(hsl_shift.l * 2 * denom);
  int32_t s_numer = static_cast<int32_t>(hsl_shift.s * 2 * denom);
  for (int x = 0; x < width; x++) {
    int32_t a = static_cast<int32_t>(SkGetPackedA32(in[x]));
    int32_t r = static_cast<int32_t>(SkGetPackedR32(in[x]));
    int32_t g = static_cast<int32_t>(SkGetPackedG32(in[x]));
    int32_t b = static_cast<int32_t>(SkGetPackedB32(in[x]));

    int32_t vmax, vmin;
    if (r > g) {  // This uses 3 compares rather than 4.
      vmax = std::max(r, b);
      vmin = std::min(g, b);
    } else {
      vmax = std::max(g, b);
      vmin = std::min(r, b);
    }

    // Use denom * L to avoid rounding.
    int32_t denom_l = (vmax + vmin) * (denom / 2);
    int32_t s_numer_l = (vmax + vmin) * s_numer / 2;

    r = (denom_l + r * s_numer - s_numer_l) * l_numer / (denom * denom);
    g = (denom_l + g * s_numer - s_numer_l) * l_numer / (denom * denom);
    b = (denom_l + b * s_numer - s_numer_l) * l_numer / (denom * denom);
    out[x] = SkPackARGB32(a, r, g, b);
  }
}

// Line processor: H no-op, S decrease, L increase.
void LineProcHnopSdecLinc(const color_utils::HSL& hsl_shift,
                          const SkPMColor* in,
                          SkPMColor* out,
                          int width) {
  DCHECK(hsl_shift.h < 0);
  DCHECK(hsl_shift.s >= 0 && hsl_shift.s <= 0.5 - HSLShift::epsilon);
  DCHECK(hsl_shift.l >= 0.5 + HSLShift::epsilon && hsl_shift.l <= 1);

  // Can't be too big since we need room for denom*denom and a bit for sign.
  const int32_t denom = 1024;
  int32_t l_numer = static_cast<int32_t>((hsl_shift.l - 0.5) * 2 * denom);
  int32_t s_numer = static_cast<int32_t>(hsl_shift.s * 2 * denom);
  for (int x = 0; x < width; x++) {
    int32_t a = static_cast<int32_t>(SkGetPackedA32(in[x]));
    int32_t r = static_cast<int32_t>(SkGetPackedR32(in[x]));
    int32_t g = static_cast<int32_t>(SkGetPackedG32(in[x]));
    int32_t b = static_cast<int32_t>(SkGetPackedB32(in[x]));

    int32_t vmax, vmin;
    if (r > g) {  // This uses 3 compares rather than 4.
      vmax = std::max(r, b);
      vmin = std::min(g, b);
    } else {
      vmax = std::max(g, b);
      vmin = std::min(r, b);
    }

    // Use denom * L to avoid rounding.
    int32_t denom_l = (vmax + vmin) * (denom / 2);
    int32_t s_numer_l = (vmax + vmin) * s_numer / 2;

    r = denom_l + r * s_numer - s_numer_l;
    g = denom_l + g * s_numer - s_numer_l;
    b = denom_l + b * s_numer - s_numer_l;

    r = (r * denom + (a * denom - r) * l_numer) / (denom * denom);
    g = (g * denom + (a * denom - g) * l_numer) / (denom * denom);
    b = (b * denom + (a * denom - b) * l_numer) / (denom * denom);
    out[x] = SkPackARGB32(a, r, g, b);
  }
}

const LineProcessor kLineProcessors[kNumHOps][kNumSOps][kNumLOps] = {
  { // H: kOpHNone
    { // S: kOpSNone
      LineProcCopy,         // L: kOpLNone
      LineProcHnopSnopLdec, // L: kOpLDec
      LineProcHnopSnopLinc  // L: kOpLInc
    },
    { // S: kOpSDec
      LineProcHnopSdecLnop, // L: kOpLNone
      LineProcHnopSdecLdec, // L: kOpLDec
      LineProcHnopSdecLinc  // L: kOpLInc
    },
    { // S: kOpSInc
      LineProcDefault, // L: kOpLNone
      LineProcDefault, // L: kOpLDec
      LineProcDefault  // L: kOpLInc
    }
  },
  { // H: kOpHShift
    { // S: kOpSNone
      LineProcDefault, // L: kOpLNone
      LineProcDefault, // L: kOpLDec
      LineProcDefault  // L: kOpLInc
    },
    { // S: kOpSDec
      LineProcDefault, // L: kOpLNone
      LineProcDefault, // L: kOpLDec
      LineProcDefault  // L: kOpLInc
    },
    { // S: kOpSInc
      LineProcDefault, // L: kOpLNone
      LineProcDefault, // L: kOpLDec
      LineProcDefault  // L: kOpLInc
    }
  }
};

}  // namespace HSLShift
}  // namespace

// static
SkBitmap SkBitmapOperations::CreateHSLShiftedBitmap(
    const SkBitmap& bitmap,
    const color_utils::HSL& hsl_shift) {
  if (IsUninitializedBitmap(bitmap))
    return bitmap;
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  // Default to NOPs.
  HSLShift::OperationOnH H_op = HSLShift::kOpHNone;
  HSLShift::OperationOnS S_op = HSLShift::kOpSNone;
  HSLShift::OperationOnL L_op = HSLShift::kOpLNone;

  if (hsl_shift.h >= 0 && hsl_shift.h <= 1)
    H_op = HSLShift::kOpHShift;

  // Saturation shift: 0 -> fully desaturate, 0.5 -> NOP, 1 -> fully saturate.
  if (hsl_shift.s >= 0 && hsl_shift.s <= (0.5 - HSLShift::epsilon))
    S_op = HSLShift::kOpSDec;
  else if (hsl_shift.s >= (0.5 + HSLShift::epsilon))
    S_op = HSLShift::kOpSInc;

  // Lightness shift: 0 -> black, 0.5 -> NOP, 1 -> white.
  if (hsl_shift.l >= 0 && hsl_shift.l <= (0.5 - HSLShift::epsilon))
    L_op = HSLShift::kOpLDec;
  else if (hsl_shift.l >= (0.5 + HSLShift::epsilon))
    L_op = HSLShift::kOpLInc;

  HSLShift::LineProcessor line_proc =
      HSLShift::kLineProcessors[H_op][S_op][L_op];

  DCHECK(bitmap.empty() == false);
  DCHECK(bitmap.colorType() == kN32_SkColorType);

  SkBitmap shifted;
  shifted.allocN32Pixels(bitmap.width(), bitmap.height());

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < bitmap.height(); ++y) {
    SkPMColor* pixels = bitmap.getAddr32(0, y);
    SkPMColor* tinted_pixels = shifted.getAddr32(0, y);

    (*line_proc)(hsl_shift, pixels, tinted_pixels, bitmap.width());
  }

  return shifted;
}

// static
SkBitmap SkBitmapOperations::CreateTiledBitmap(const SkBitmap& source,
                                               int src_x, int src_y,
                                               int dst_w, int dst_h) {
  CHECK_EQ(source.colorType(), kN32_SkColorType);

  SkBitmap cropped;
  cropped.allocN32Pixels(dst_w, dst_h);

  // Loop through the pixels of the original bitmap.
  for (int y = 0; y < dst_h; ++y) {
    int y_pix = (src_y + y) % source.height();
    while (y_pix < 0)
      y_pix += source.height();

    uint32_t* source_row = source.getAddr32(0, y_pix);
    uint32_t* dst_row = cropped.getAddr32(0, y);

    for (int x = 0; x < dst_w; ++x) {
      int x_pix = (src_x + x) % source.width();
      while (x_pix < 0)
        x_pix += source.width();

      dst_row[x] = source_row[x_pix];
    }
  }

  return cropped;
}

// static
SkBitmap SkBitmapOperations::DownsampleByTwoUntilSize(const SkBitmap& bitmap,
                                                      int min_w, int min_h) {
  if ((bitmap.width() <= min_w) || (bitmap.height() <= min_h) ||
      (min_w < 0) || (min_h < 0))
    return bitmap;

  // Since bitmaps are refcounted, this copy will be fast.
  SkBitmap current = bitmap;
  while ((current.width() >= min_w * 2) && (current.height() >= min_h * 2) &&
         (current.width() > 1) && (current.height() > 1))
    current = DownsampleByTwo(current);
  return current;
}

// static
SkBitmap SkBitmapOperations::DownsampleByTwo(const SkBitmap& bitmap) {
  if (IsUninitializedBitmap(bitmap))
    return bitmap;
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  // Handle the nop case.
  if ((bitmap.width() <= 1) || (bitmap.height() <= 1))
    return bitmap;

  SkBitmap result;
  result.allocN32Pixels((bitmap.width() + 1) / 2, (bitmap.height() + 1) / 2);

  const int resultLastX = result.width() - 1;
  const int srcLastX = bitmap.width() - 1;

  for (int dest_y = 0; dest_y < result.height(); ++dest_y) {
    const int src_y = dest_y << 1;
    const SkPMColor* SK_RESTRICT cur_src0 = bitmap.getAddr32(0, src_y);
    const SkPMColor* SK_RESTRICT cur_src1 = cur_src0;
    if (src_y + 1 < bitmap.height())
      cur_src1 = bitmap.getAddr32(0, src_y + 1);

    SkPMColor* SK_RESTRICT cur_dst = result.getAddr32(0, dest_y);

    for (int dest_x = 0; dest_x <= resultLastX; ++dest_x) {
      // This code is based on downsampleby2_proc32 in SkBitmap.cpp. It is very
      // clever in that it does two channels at once: alpha and green ("ag")
      // and red and blue ("rb"). Each channel gets averaged across 4 pixels
      // to get the result.
      int bump_x = (dest_x << 1) < srcLastX;
      SkPMColor tmp, ag, rb;

      // Top left pixel of the 2x2 block.
      tmp = cur_src0[0];
      ag = (tmp >> 8) & 0xFF00FF;
      rb = tmp & 0xFF00FF;

      // Top right pixel of the 2x2 block.
      tmp = cur_src0[bump_x];
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;

      // Bottom left pixel of the 2x2 block.
      tmp = cur_src1[0];
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;

      // Bottom right pixel of the 2x2 block.
      tmp = cur_src1[bump_x];
      ag += (tmp >> 8) & 0xFF00FF;
      rb += tmp & 0xFF00FF;

      // Put the channels back together, dividing each by 4 to get the average.
      // |ag| has the alpha and green channels shifted right by 8 bits from
      // there they should end up, so shifting left by 6 gives them in the
      // correct position divided by 4.
      *cur_dst++ = ((rb >> 2) & 0xFF00FF) | ((ag << 6) & 0xFF00FF00);

      cur_src0 += 2;
      cur_src1 += 2;
    }
  }

  return result;
}

// static
SkBitmap SkBitmapOperations::UnPreMultiply(const SkBitmap& bitmap) {
  if (IsUninitializedBitmap(bitmap))
    return bitmap;
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  if (bitmap.alphaType() != kPremul_SkAlphaType)
    return bitmap;

  const SkImageInfo& opaque_info =
      bitmap.info().makeAlphaType(kUnpremul_SkAlphaType);
  SkBitmap opaque_bitmap;
  opaque_bitmap.allocPixels(opaque_info);

  for (int y = 0; y < opaque_bitmap.height(); y++) {
    for (int x = 0; x < opaque_bitmap.width(); x++) {
      uint32_t src_pixel = *bitmap.getAddr32(x, y);
      uint32_t* dst_pixel = opaque_bitmap.getAddr32(x, y);
      SkColor unmultiplied = SkUnPreMultiply::PMColorToColor(src_pixel);
      *dst_pixel = unmultiplied;
    }
  }

  return opaque_bitmap;
}

// static
SkBitmap SkBitmapOperations::CreateTransposedBitmap(const SkBitmap& image) {
  if (IsUninitializedBitmap(image))
    return image;
  CHECK_EQ(image.colorType(), kN32_SkColorType);

  SkBitmap transposed;
  transposed.allocN32Pixels(image.height(), image.width());

  for (int y = 0; y < image.height(); ++y) {
    uint32_t* image_row = image.getAddr32(0, y);
    for (int x = 0; x < image.width(); ++x) {
      uint32_t* dst = transposed.getAddr32(y, x);
      *dst = image_row[x];
    }
  }

  return transposed;
}

// static
SkBitmap SkBitmapOperations::CreateColorMask(const SkBitmap& bitmap,
                                             SkColor c) {
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  SkBitmap color_mask;
  color_mask.allocN32Pixels(bitmap.width(), bitmap.height());
  color_mask.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(color_mask, SkSurfaceProps{});

  SkPaint paint;
  paint.setColorFilter(SkColorFilters::Blend(c, SkBlendMode::kSrcIn));
  canvas.drawImage(bitmap.asImage(), 0, 0, SkSamplingOptions(), &paint);
  return color_mask;
}

// static
SkBitmap SkBitmapOperations::CreateDropShadow(
    const SkBitmap& bitmap,
    const gfx::ShadowValues& shadows) {
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  // Shadow margin insets are negative values because they grow outside.
  // Negate them here as grow direction is not important and only pixel value
  // is of interest here.
  gfx::Insets shadow_margin = -gfx::ShadowValue::GetMargin(shadows);

  SkBitmap image_with_shadow;
  image_with_shadow.allocN32Pixels(bitmap.width() + shadow_margin.width(),
                                   bitmap.height() + shadow_margin.height());
  image_with_shadow.eraseARGB(0, 0, 0, 0);

  SkCanvas canvas(image_with_shadow, SkSurfaceProps{});
  canvas.translate(SkIntToScalar(shadow_margin.left()),
                   SkIntToScalar(shadow_margin.top()));

  SkPaint paint;
  for (size_t i = 0; i < shadows.size(); ++i) {
    const gfx::ShadowValue& shadow = shadows[i];
    SkBitmap shadow_image = SkBitmapOperations::CreateColorMask(bitmap,
                                                                shadow.color());

    // The blur is halved to produce a shadow that correctly fits within the
    // |shadow_margin|.
    SkScalar sigma = SkDoubleToScalar(shadow.blur() / 2);
    paint.setImageFilter(SkImageFilters::Blur(sigma, sigma, nullptr));

    canvas.saveLayer(0, &paint);
    canvas.drawImage(shadow_image.asImage(), SkIntToScalar(shadow.x()),
                     SkIntToScalar(shadow.y()));
    canvas.restore();
  }

  canvas.drawImage(bitmap.asImage(), 0, 0);
  return image_with_shadow;
}

// static
SkBitmap SkBitmapOperations::Rotate(const SkBitmap& source,
                                    RotationAmount rotation) {
  if (IsUninitializedBitmap(source))
    return source;
  CHECK_EQ(source.colorType(), kN32_SkColorType);
  // SkCanvas::drawBitmap() fails silently with unpremultiplied SkBitmap.
  DCHECK_NE(source.info().alphaType(), kUnpremul_SkAlphaType);

  SkBitmap result;
  SkScalar angle = SkFloatToScalar(0.0f);

  switch (rotation) {
   case ROTATION_90_CW:
     angle = SkFloatToScalar(90.0f);
     result.allocN32Pixels(source.height(), source.width());
     break;
   case ROTATION_180_CW:
     angle = SkFloatToScalar(180.0f);
     result.allocN32Pixels(source.width(), source.height());
     break;
   case ROTATION_270_CW:
     angle = SkFloatToScalar(270.0f);
     result.allocN32Pixels(source.height(), source.width());
     break;
  }

  SkCanvas canvas(result, SkSurfaceProps{});
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  canvas.translate(SkFloatToScalar(result.width() * 0.5f),
                   SkFloatToScalar(result.height() * 0.5f));
  canvas.rotate(angle);
  canvas.translate(-SkFloatToScalar(source.width() * 0.5f),
                   -SkFloatToScalar(source.height() * 0.5f));
  canvas.drawImage(source.asImage(), 0, 0);

  return result;
}
