// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/display_color_spaces.h"

#include <array>
#include <cmath>

#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "skia/ext/skcolorspace_primaries.h"

namespace gfx {

namespace {

const ContentColorUsage kAllColorUsages[] = {
    ContentColorUsage::kSRGB,
    ContentColorUsage::kWideColorGamut,
    ContentColorUsage::kHDR,
};

gfx::BufferFormat DefaultBufferFormat() {
  // ChromeOS expects the default buffer format be BGRA_8888 in several places.
  // https://crbug.com/1057501, https://crbug.com/1073237
  // The default format on Mac is BGRA in screen_mac.cc, so we set it here
  // too so that it matches with --ensure-forced-color-profile.
  // https://crbug.com/1478708
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  return gfx::BufferFormat::BGRA_8888;
#else
  return gfx::BufferFormat::RGBA_8888;
#endif
}

size_t GetIndex(ContentColorUsage color_usage, bool needs_alpha) {
  switch (color_usage) {
    case ContentColorUsage::kSRGB:
      return 0 + needs_alpha;
    case ContentColorUsage::kWideColorGamut:
      return 2 + needs_alpha;
    case ContentColorUsage::kHDR:
      return 4 + needs_alpha;
  }
}

}  // namespace

DisplayColorSpaces::DisplayColorSpaces() {
  // TODO(crbug.com/40219387): Revert back to range-based for loops if possible
  for (size_t i = 0; i < kConfigCount; i++) {
    color_spaces_[i] = gfx::ColorSpace::CreateSRGB();
    buffer_formats_[i] = DefaultBufferFormat();
  }
}

DisplayColorSpaces::DisplayColorSpaces(const gfx::DisplayColorSpaces&) =
    default;

DisplayColorSpaces& DisplayColorSpaces::operator=(
    const gfx::DisplayColorSpaces&) = default;

DisplayColorSpaces::DisplayColorSpaces(const gfx::ColorSpace& c)
    : DisplayColorSpaces() {
  if (!c.IsValid())
    return;
  primaries_ = c.GetPrimaries();
  for (size_t i = 0; i < kConfigCount; i++)  // NOLINT (modernize-loop-convert)
    color_spaces_[i] = c;
}

DisplayColorSpaces::DisplayColorSpaces(const ColorSpace& c,
                                       viz::SharedImageFormat f)
    : DisplayColorSpaces(c) {
  auto buffer_format = viz::SinglePlaneSharedImageFormatToBufferFormat(f);
  for (size_t i = 0; i < kConfigCount; i++) {
    buffer_formats_[i] = buffer_format;
  }
}

void DisplayColorSpaces::SetOutputFormats(
    viz::SharedImageFormat format_no_alpha,
    viz::SharedImageFormat format_with_alpha) {
  for (const auto& color_usage : kAllColorUsages) {
    size_t i_no_alpha = GetIndex(color_usage, false);
    size_t i_needs_alpha = GetIndex(color_usage, true);
    buffer_formats_[i_no_alpha] =
        viz::SinglePlaneSharedImageFormatToBufferFormat(format_no_alpha);
    buffer_formats_[i_needs_alpha] =
        viz::SinglePlaneSharedImageFormatToBufferFormat(format_with_alpha);
  }
}

void DisplayColorSpaces::SetOutputColorSpaceAndFormat(
    ContentColorUsage color_usage,
    bool needs_alpha,
    const gfx::ColorSpace& color_space,
    viz::SharedImageFormat format) {
  size_t i = GetIndex(color_usage, needs_alpha);
  color_spaces_[i] = color_space;
  buffer_formats_[i] = viz::SinglePlaneSharedImageFormatToBufferFormat(format);
}

ColorSpace DisplayColorSpaces::GetOutputColorSpace(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return color_spaces_[GetIndex(color_usage, needs_alpha)];
}

viz::SharedImageFormat DisplayColorSpaces::GetOutputFormat(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return viz::GetSharedImageFormat(
      buffer_formats_[GetIndex(color_usage, needs_alpha)]);
}

ColorSpace DisplayColorSpaces::GetRasterAndCompositeColorSpace(
    ContentColorUsage color_usage) const {
  gfx::ColorSpace result;
  if (color_usage == ContentColorUsage::kSRGB) {
    result =
        GetOutputColorSpace(ContentColorUsage::kSRGB, /*needs_alpha=*/true);
    if (!result.IsSuitableForBlending()) {
      result = ColorSpace::CreateSRGB();
    }
  } else {
    result = GetOutputColorSpace(ContentColorUsage::kWideColorGamut,
                                 /*needs_alpha=*/true);

    // The below logic is to work around the issue that Windows' output buffer
    // choices are limited. It is not a generic operation. Windows only offers
    // three usable options for backbuffer formats:
    // * 8-bit sRGB
    // * 10-bit Rec2020 PQ
    // * F16 sRGB linear HDR (identical to PQ, with 1.0 matching 80 nits)
    // Absent in this list is an 8-bit wide color gamut option (like P3). This
    // gives us the options of:
    // * Raster wide color gamut content into F16 extended-sRGB buffers.
    //   This is expensive, but raster/composite results are always consistent.
    // * Raster wide color gamut into 8-bit P3 buffers
    //   This isn't expensive, but raster/composite results differ based on
    //   whether or not WCG content is visible.
    // We go with the second option, and do all non-sRGB raster and composite
    // in P3.
    if (!result.IsSuitableForBlending()) {
      result =
          ColorSpace(ColorSpace::PrimaryID::P3, ColorSpace::TransferID::SRGB);
    }

    // Report the HDR version of the space only if HDR is both requested and
    // supported.
    if (SupportsHDR() && color_usage == ContentColorUsage::kHDR) {
      result = result.GetAsHDR();
    }
  }

  return result;
}

bool DisplayColorSpaces::SupportsHDR() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false).IsHDR() ||
         GetOutputColorSpace(ContentColorUsage::kHDR, true).IsHDR() ||
         hdr_max_luminance_relative_ > 1.f;
}

float DisplayColorSpaces::GetHdrHeadroom() const {
  return std::log2(hdr_max_luminance_relative_);
}

ColorSpace DisplayColorSpaces::GetScreenInfoColorSpace() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false /* needs_alpha */);
}

void DisplayColorSpaces::ToStrings(
    std::vector<std::string>* out_names,
    std::vector<gfx::ColorSpace>* out_color_spaces,
    std::vector<viz::SharedImageFormat>* out_formats) const {
  // The names of the configurations.
  std::array<const char*, kConfigCount> config_names = {
      "sRGB/no-alpha", "sRGB/alpha",   "WCG/no-alpha",
      "WCG/alpha",     "HDR/no-alpha", "HDR/alpha",
  };
  // Names for special configuration subsets (e.g, all sRGB, all WCG, etc).
  constexpr size_t kSpecialConfigCount = 5;
  std::array<const char*, kSpecialConfigCount> special_config_names = {
      "sRGB", "WCG", "SDR", "HDR", "all",
  };
  const std::array<std::array<const size_t, 2>, kSpecialConfigCount>
      special_config_indices = {{
          {0, 2},
          {2, 4},
          {0, 4},
          {4, 6},
          {0, 6},
      }};

  // We don't want to take up 6 lines (one for each config) if we don't need to.
  // To avoid this, build up half-open intervals [i, j) which have the same
  // color space and buffer formats, and group them together. The above "special
  // configs" give groups that have a common name.
  size_t i = 0;
  size_t j = 0;
  while (i != kConfigCount) {
    // Keep growing the interval [i, j) until entry j is different, or past the
    // end.
    if (color_spaces_[i] == color_spaces_[j] &&
        buffer_formats_[i] == buffer_formats_[j] && j != kConfigCount) {
      j += 1;
      continue;
    }

    // Populate the name for the group from the "special config" names.
    std::string name;
    for (size_t k = 0; k < kSpecialConfigCount; ++k) {
      if (i == special_config_indices[k][0] &&
          j == special_config_indices[k][1]) {
        name = special_config_names[k];
        break;
      }
    }
    // If that didn't work, just list the configs.
    if (name.empty()) {
      for (size_t k = i; k < j; ++k) {
        name += std::string(config_names[k]);
        if (k != j - 1)
          name += ",";
      }
    }

    // Add an entry, and continue with the interval [j, j).
    out_names->push_back(name);
    out_formats->push_back(viz::GetSharedImageFormat(buffer_formats_[i]));
    out_color_spaces->push_back(color_spaces_[i]);
    i = j;
  };
}

void DisplayColorSpaces::AsValueInto(
    base::trace_event::TracedValue* value) const {
  std::vector<std::string> names;
  std::vector<gfx::ColorSpace> color_spaces;
  std::vector<viz::SharedImageFormat> formats;
  ToStrings(&names, &color_spaces, &formats);
  value->BeginArray("configs");
  for (size_t i = 0; i < names.size(); ++i) {
    value->BeginDictionary();
    value->SetString("name", names[i]);
    value->SetString("color_space", color_spaces[i].ToString());
    value->SetString("format", formats[i].ToString());
    value->EndDictionary();
  }
  value->EndArray();
  value->SetString("primaries",
                   skia::SkColorSpacePrimariesToString(primaries_));
  value->SetDouble("sdr_max_luminance_nits", sdr_max_luminance_nits_);
  value->SetDouble("hdr_max_luminance_relative", hdr_max_luminance_relative_);
}

bool DisplayColorSpaces::operator==(const DisplayColorSpaces& other) const {
  for (size_t i = 0; i < kConfigCount; ++i) {
    if (color_spaces_[i] != other.color_spaces_[i])
      return false;
    if (buffer_formats_[i] != other.buffer_formats_[i])
      return false;
  }
  if (primaries_ != other.primaries_)
    return false;
  if (sdr_max_luminance_nits_ != other.sdr_max_luminance_nits_)
    return false;
  if (hdr_max_luminance_relative_ != other.hdr_max_luminance_relative_)
    return false;

  return true;
}

// static
bool DisplayColorSpaces::EqualExceptForHdrHeadroom(
    const DisplayColorSpaces& a,
    const DisplayColorSpaces& b) {
  DisplayColorSpaces b_with_a_params = b;
  b_with_a_params.hdr_max_luminance_relative_ = a.hdr_max_luminance_relative_;
  return a == b_with_a_params;
}

DisplayColorSpacesRef::DisplayColorSpacesRef() = default;

DisplayColorSpacesRef::DisplayColorSpacesRef(
    const gfx::DisplayColorSpaces& color_spaces)
    : color_spaces_(color_spaces) {}

}  // namespace gfx
