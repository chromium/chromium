// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/display_color_spaces.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC)
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

DisplayColorSpaces::DisplayColorSpaces(const ColorSpace& c, BufferFormat f)
    : DisplayColorSpaces(c) {
  for (size_t i = 0; i < kConfigCount; i++) {
    buffer_formats_[i] = f;
  }
}

void DisplayColorSpaces::SetOutputBufferFormats(
    gfx::BufferFormat buffer_format_no_alpha,
    gfx::BufferFormat buffer_format_needs_alpha) {
  for (const auto& color_usage : kAllColorUsages) {
    size_t i_no_alpha = GetIndex(color_usage, false);
    size_t i_needs_alpha = GetIndex(color_usage, true);
    buffer_formats_[i_no_alpha] = buffer_format_no_alpha;
    buffer_formats_[i_needs_alpha] = buffer_format_needs_alpha;
  }
}

void DisplayColorSpaces::SetOutputColorSpaceAndBufferFormat(
    ContentColorUsage color_usage,
    bool needs_alpha,
    const gfx::ColorSpace& color_space,
    gfx::BufferFormat buffer_format) {
  size_t i = GetIndex(color_usage, needs_alpha);
  color_spaces_[i] = color_space;
  buffer_formats_[i] = buffer_format;
}

ColorSpace DisplayColorSpaces::GetOutputColorSpace(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return color_spaces_[GetIndex(color_usage, needs_alpha)];
}

BufferFormat DisplayColorSpaces::GetOutputBufferFormat(
    ContentColorUsage color_usage,
    bool needs_alpha) const {
  return buffer_formats_[GetIndex(color_usage, needs_alpha)];
}

gfx::ColorSpace DisplayColorSpaces::GetRasterColorSpace() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false /* needs_alpha */);
}

gfx::ColorSpace DisplayColorSpaces::GetCompositingColorSpace(
    bool needs_alpha,
    ContentColorUsage color_usage) const {
  gfx::ColorSpace result = GetOutputColorSpace(color_usage, needs_alpha);
  if (!result.IsSuitableForBlending())
    result = gfx::ColorSpace::CreateExtendedSRGB();
  return result;
}

bool DisplayColorSpaces::SupportsHDR() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false).IsHDR() ||
         GetOutputColorSpace(ContentColorUsage::kHDR, true).IsHDR() ||
         hdr_max_luminance_relative_ > 1.f;
}

ColorSpace DisplayColorSpaces::GetScreenInfoColorSpace() const {
  return GetOutputColorSpace(ContentColorUsage::kHDR, false /* needs_alpha */);
}

void DisplayColorSpaces::ToStrings(
    std::vector<std::string>* out_names,
    std::vector<gfx::ColorSpace>* out_color_spaces,
    std::vector<gfx::BufferFormat>* out_buffer_formats) const {
  // The names of the configurations.
  const char* config_names[kConfigCount] = {
      "sRGB/no-alpha", "sRGB/alpha",   "WCG/no-alpha",
      "WCG/alpha",     "HDR/no-alpha", "HDR/alpha",
  };
  // Names for special configuration subsets (e.g, all sRGB, all WCG, etc).
  constexpr size_t kSpecialConfigCount = 5;
  const char* special_config_names[kSpecialConfigCount] = {
      "sRGB", "WCG", "SDR", "HDR", "all",
  };
  const size_t special_config_indices[kSpecialConfigCount][2] = {
      {0, 2}, {2, 4}, {0, 4}, {4, 6}, {0, 6},
  };

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
    out_buffer_formats->push_back(buffer_formats_[i]);
    out_color_spaces->push_back(color_spaces_[i]);
    i = j;
  };
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

bool DisplayColorSpaces::operator!=(const DisplayColorSpaces& other) const {
  return !(*this == other);
}

// static
bool DisplayColorSpaces::EqualExceptForHdrHeadroom(
    const DisplayColorSpaces& a,
    const DisplayColorSpaces& b) {
  DisplayColorSpaces b_with_a_params = b;
  b_with_a_params.hdr_max_luminance_relative_ = a.hdr_max_luminance_relative_;
  return a == b_with_a_params;
}

}  // namespace gfx
