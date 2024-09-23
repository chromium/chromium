// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_DISPLAY_COLOR_SPACES_H_
#define UI_GFX_DISPLAY_COLOR_SPACES_H_

#include <optional>
#include <string>
#include <vector>

#include "skia/ext/skcolorspace_primaries.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_space_export.h"
#include "ui/gfx/hdr_static_metadata.h"

namespace mojo {
template <class T, class U>
struct StructTraits;
}  // namespace mojo

namespace gfx {

namespace mojom {
class DisplayColorSpacesDataView;
}  // namespace mojom

// The values are set so std::max() can be used to find the widest.
enum class ContentColorUsage : uint8_t {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a color usage replace it with a dummy value; when
  // adding a color usage, do so at the bottom (and update kMaxValue).
  kSRGB = 0,
  kWideColorGamut = 1,
  kHDR = 2,
  kMaxValue = kHDR,
};

// This structure is used by a display::Display to specify the color space that
// should be used to display content of various types. This lives in here, as
// opposed to in ui/display because it is used directly by components/viz.
class COLOR_SPACE_EXPORT DisplayColorSpaces {
 public:
  static constexpr size_t kConfigCount = 6;

  // Initialize as sRGB-only.
  DisplayColorSpaces();
  DisplayColorSpaces(const DisplayColorSpaces& display_color_space);
  DisplayColorSpaces& operator=(const DisplayColorSpaces& display_color_space);

  // Initialize as |color_space| for all settings. If |color_space| is the
  // default (invalid) color space, then initialize to sRGB. The BufferFormat
  // will be set to a default value (BGRA_8888 or RGBA_8888) depending on
  // build configuration.
  explicit DisplayColorSpaces(const ColorSpace& color_space);

  // Initialize as |color_space| and |buffer_format| for all settings. If
  // |color_space| is the default (invalid) color space, then initialize to
  // sRGB.
  DisplayColorSpaces(const ColorSpace& color_space, BufferFormat buffer_format);

  // Set the color space and buffer format for the final output surface when the
  // specified content is being displayed.
  void SetOutputColorSpaceAndBufferFormat(ContentColorUsage color_usage,
                                          bool needs_alpha,
                                          const gfx::ColorSpace& color_space,
                                          gfx::BufferFormat buffer_format);

  // Set the buffer format for all color usages to |buffer_format_no_alpha| when
  // alpha is not needed and |buffer_format_with_alpha| when alpha is needed.
  void SetOutputBufferFormats(gfx::BufferFormat buffer_format_no_alpha,
                              gfx::BufferFormat buffer_format_with_alpha);

  // Retrieve parameters for a specific usage and alpha.
  ColorSpace GetOutputColorSpace(ContentColorUsage color_usage,
                                 bool needs_alpha) const;
  BufferFormat GetOutputBufferFormat(ContentColorUsage color_usage,
                                     bool needs_alpha) const;

  // Set the maximum SDR luminance, in nits. This is a non-default value only
  // on Windows.
  void SetSDRMaxLuminanceNits(float sdr_max_luminance_nits) {
    sdr_max_luminance_nits_ = sdr_max_luminance_nits;
  }
  float GetSDRMaxLuminanceNits() const { return sdr_max_luminance_nits_; }

  // Set the maximum luminance that HDR content can display. This is represented
  // as a multiple of the SDR white luminance (so a display that is incapable of
  // HDR would have a value of 1.0).
  void SetHDRMaxLuminanceRelative(float hdr_max_luminance_relative) {
    hdr_max_luminance_relative_ = hdr_max_luminance_relative;
  }
  float GetHDRMaxLuminanceRelative() const {
    return hdr_max_luminance_relative_;
  }

  // TODO(crbug.com/40144904): These helper functions exist temporarily
  // to handle the transition of display::ScreenInfo off of ColorSpace. All
  // calls to these functions are to be eliminated.
  ColorSpace GetScreenInfoColorSpace() const;

  // Return the color space that should be used for rasterization.
  // TODO: This will eventually need to take a ContentColorUsage.
  gfx::ColorSpace GetRasterColorSpace() const;

  // Return the color space in which compositing (and, in particular, blending,
  // should be performed). This space may not (on Windows) be suitable for
  // output.
  gfx::ColorSpace GetCompositingColorSpace(bool needs_alpha,
                                           ContentColorUsage color_usage) const;

  // Return true if the HDR color spaces are, indeed, HDR.
  bool SupportsHDR() const;

  // Return the primaries that define the color gamut of the display.
  const SkColorSpacePrimaries& GetPrimaries() const { return primaries_; }
  void SetPrimaries(const SkColorSpacePrimaries& primaries) {
    primaries_ = primaries;
  }

  // Output as a vector of strings. This is a helper function for printing in
  // about:gpu. All output vectors will be the same length. Each entry will be
  // the configuration name, its buffer format, and its color space.
  void ToStrings(std::vector<std::string>* out_names,
                 std::vector<gfx::ColorSpace>* out_color_spaces,
                 std::vector<gfx::BufferFormat>* out_buffer_formats) const;

  bool operator==(const DisplayColorSpaces& other) const;
  bool operator!=(const DisplayColorSpaces& other) const;

  // Return true if the two parameters are equal except for their
  // `hdr_max_luminance_relative_` member.
  static bool EqualExceptForHdrHeadroom(const DisplayColorSpaces& a,
                                        const DisplayColorSpaces& b);

 private:
  // Serialization of DisplayColorSpaces directly accesses members.
  friend struct IPC::ParamTraits<gfx::DisplayColorSpaces>;
  friend struct mojo::StructTraits<gfx::mojom::DisplayColorSpacesDataView,
                                   gfx::DisplayColorSpaces>;

  gfx::ColorSpace color_spaces_[kConfigCount];
  gfx::BufferFormat buffer_formats_[kConfigCount];
  SkColorSpacePrimaries primaries_ = SkNamedPrimariesExt::kSRGB;
  float sdr_max_luminance_nits_ = ColorSpace::kDefaultSDRWhiteLevel;
  float hdr_max_luminance_relative_ = 1.f;
};

}  // namespace gfx

#endif  // UI_GFX_DISPLAY_COLOR_SPACES_H_
