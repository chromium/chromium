// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/util/display_util.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/edid_parser.h"
#include "ui/gfx/icc_profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/display/display_features.h"
#endif

namespace display {

namespace {

base::flat_set<int64_t>* internal_display_ids() {
  static base::NoDestructor<base::flat_set<int64_t>> display_ids;
  return display_ids.get();
}

// A list of bogus sizes in mm that should be ignored.
// See crbug.com/136533. The first element maintains the minimum
// size required to be valid size.
constexpr int kInvalidDisplaySizeList[][2] = {
    {40, 30},
    {50, 40},
    {160, 90},
    {160, 100},
};

// Used in the GetColorSpaceFromEdid function to collect data on whether the
// color space extracted from an EDID blob passed the sanity checks.
void EmitEdidColorSpaceChecksOutcomeUma(EdidColorSpaceChecksOutcome outcome) {
  base::UmaHistogramEnumeration("DrmUtil.GetColorSpaceFromEdid.ChecksOutcome",
                                outcome);
}

// Returns true if each and all matrix values are within |epsilon| distance.
bool NearlyEqual(const skcms_Matrix3x3& lhs,
                 const skcms_Matrix3x3& rhs,
                 float epsilon) {
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (std::abs(lhs.vals[r][c] - rhs.vals[r][c]) > epsilon)
        return false;
    }
  }
  return true;
}

}  // namespace

bool IsDisplaySizeValid(const gfx::Size& physical_size) {
  // Ignore if the reported display is smaller than minimum size.
  if (physical_size.width() <= kInvalidDisplaySizeList[0][0] ||
      physical_size.height() <= kInvalidDisplaySizeList[0][1]) {
    VLOG(1) << "Smaller than minimum display size";
    return false;
  }
  for (size_t i = 1; i < std::size(kInvalidDisplaySizeList); ++i) {
    const gfx::Size size(kInvalidDisplaySizeList[i][0],
                         kInvalidDisplaySizeList[i][1]);
    if (physical_size == size) {
      VLOG(1) << "Invalid display size detected:" << size.ToString();
      return false;
    }
  }
  return true;
}

int64_t GenerateDisplayID(uint16_t manufacturer_id,
                          uint32_t product_code_hash,
                          uint8_t output_index) {
  return ((static_cast<int64_t>(manufacturer_id) << 40) |
          (static_cast<int64_t>(product_code_hash) << 8) | output_index);
}

gfx::ColorSpace GetColorSpaceFromEdid(const display::EdidParser& edid_parser) {
  const SkColorSpacePrimaries primaries = edid_parser.primaries();

  // Sanity check: primaries should verify By <= Ry <= Gy, Bx <= Rx and Gx <=
  // Rx, to guarantee that the R, G and B colors are each in the correct region.
  if (!(primaries.fBX <= primaries.fRX && primaries.fGX <= primaries.fRX &&
        primaries.fBY <= primaries.fRY && primaries.fRY <= primaries.fGY)) {
    EmitEdidColorSpaceChecksOutcomeUma(
        EdidColorSpaceChecksOutcome::kErrorBadCoordinates);
    return gfx::ColorSpace();
  }

  // Sanity check: the area spawned by the primaries' triangle is too small,
  // i.e. less than half the surface of the triangle spawned by sRGB/BT.709.
  constexpr double kBT709PrimariesArea = 0.0954;
  const float primaries_area_twice =
      (primaries.fRX * primaries.fGY) + (primaries.fBX * primaries.fRY) +
      (primaries.fGX * primaries.fBY) - (primaries.fBX * primaries.fGY) -
      (primaries.fGX * primaries.fRY) - (primaries.fRX * primaries.fBY);
  if (primaries_area_twice < kBT709PrimariesArea) {
    EmitEdidColorSpaceChecksOutcomeUma(
        EdidColorSpaceChecksOutcome::kErrorPrimariesAreaTooSmall);
    return gfx::ColorSpace();
  }

  // Sanity check: https://crbug.com/809909, the blue primary coordinates should
  // not be too far left/upwards of the expected location (namely [0.15, 0.06]
  // for sRGB/ BT.709/ Adobe RGB/ DCI-P3, and [0.131, 0.046] for BT.2020).
  constexpr float kExpectedBluePrimaryX = 0.15f;
  constexpr float kBluePrimaryXDelta = 0.02f;
  constexpr float kExpectedBluePrimaryY = 0.06f;
  constexpr float kBluePrimaryYDelta = 0.031f;
  const bool is_blue_primary_broken =
      (std::abs(primaries.fBX - kExpectedBluePrimaryX) > kBluePrimaryXDelta) ||
      (std::abs(primaries.fBY - kExpectedBluePrimaryY) > kBluePrimaryYDelta);
  if (is_blue_primary_broken) {
    EmitEdidColorSpaceChecksOutcomeUma(
        EdidColorSpaceChecksOutcome::kErrorBluePrimaryIsBroken);
    return gfx::ColorSpace();
  }

  skcms_Matrix3x3 primaries_matrix;
  if (!primaries.toXYZD50(&primaries_matrix)) {
    EmitEdidColorSpaceChecksOutcomeUma(
        EdidColorSpaceChecksOutcome::kErrorCannotExtractToXYZD50);
    return gfx::ColorSpace();
  }

  // Snap the primaries to known standard ones (e.g. BT.709, DCI-P3) for
  // performance purposes, see crbug.com/1073467. kPrimariesTolerance is an
  // educated guess from various ChromeOS panels observations.
  auto color_space_primaries = gfx::ColorSpace::PrimaryID::INVALID;
  constexpr float kPrimariesTolerance = 0.025;
  if (NearlyEqual(primaries_matrix, SkNamedGamut::kSRGB, kPrimariesTolerance)) {
    color_space_primaries = gfx::ColorSpace::PrimaryID::BT709;
  } else if (NearlyEqual(primaries_matrix, SkNamedGamut::kDisplayP3,
                         kPrimariesTolerance)) {
    color_space_primaries = gfx::ColorSpace::PrimaryID::P3;
  }

  const float gamma = edid_parser.gamma();
  if (gamma < 1.0f) {
    EmitEdidColorSpaceChecksOutcomeUma(
        EdidColorSpaceChecksOutcome::kErrorBadGamma);
    return gfx::ColorSpace();
  }
  EmitEdidColorSpaceChecksOutcomeUma(EdidColorSpaceChecksOutcome::kSuccess);

  auto transfer_id = gfx::ColorSpace::TransferID::INVALID;
  if (base::Contains(
          edid_parser.supported_color_primary_matrix_ids(),
          EdidParser::PrimaryMatrixPair(gfx::ColorSpace::PrimaryID::BT2020,
                                        gfx::ColorSpace::MatrixID::RGB)) ||
      base::Contains(edid_parser.supported_color_primary_matrix_ids(),
                     EdidParser::PrimaryMatrixPair(
                         gfx::ColorSpace::PrimaryID::BT2020,
                         gfx::ColorSpace::MatrixID::BT2020_NCL))) {
    if (base::Contains(edid_parser.supported_color_transfer_ids(),
                       gfx::ColorSpace::TransferID::PQ)) {
      transfer_id = gfx::ColorSpace::TransferID::PQ;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (base::FeatureList::IsEnabled(
              display::features::kEnableExternalDisplayHDR10Mode) &&
          edid_parser.is_external_display() &&
          base::Contains(
              edid_parser.supported_color_primary_matrix_ids(),
              EdidParser::PrimaryMatrixPair(gfx::ColorSpace::PrimaryID::BT2020,
                                            gfx::ColorSpace::MatrixID::RGB))) {
        return gfx::ColorSpace::CreateHDR10();
      }
#endif
    } else if (base::Contains(edid_parser.supported_color_transfer_ids(),
                              gfx::ColorSpace::TransferID::HLG)) {
      transfer_id = gfx::ColorSpace::TransferID::HLG;
    }
    // If we reach here: CDB has {BT2020,RGB} or {BT2020,NCL},
    // but HDR Static Metadata Data Block does not contain PQ.
    // transfer == INVALID
  } else if (base::Contains(edid_parser.supported_color_primary_matrix_ids(),
                            EdidParser::PrimaryMatrixPair(
                                gfx::ColorSpace::PrimaryID::P3,
                                gfx::ColorSpace::MatrixID::RGB))) {
    return gfx::ColorSpace::CreateDisplayP3D65();
  } else if (gamma == 2.2f) {
    transfer_id = gfx::ColorSpace::TransferID::GAMMA22;
  } else if (gamma == 2.4f) {
    transfer_id = gfx::ColorSpace::TransferID::GAMMA24;
  }

  // Prefer to return a name-based ColorSpace to ease subsequent calculations.
  if (transfer_id != gfx::ColorSpace::TransferID::INVALID) {
    if (color_space_primaries != gfx::ColorSpace::PrimaryID::INVALID)
      return gfx::ColorSpace(color_space_primaries, transfer_id);
    return gfx::ColorSpace::CreateCustom(primaries_matrix, transfer_id);
  }

  skcms_TransferFunction transfer = {gamma, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
  if (color_space_primaries == gfx::ColorSpace::PrimaryID::INVALID)
    return gfx::ColorSpace::CreateCustom(primaries_matrix, transfer);
  return gfx::ColorSpace(
      color_space_primaries, gfx::ColorSpace::TransferID::CUSTOM,
      gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL,
      /*custom_primary_matrix=*/nullptr, &transfer);
}

bool CompareDisplayIds(int64_t id1, int64_t id2) {
  if (id1 == id2)
    return false;
  // Output index is stored in the first 8 bits. See GetDisplayIdFromEDID
  // in edid_parser.cc.
  int index_1 = id1 & 0xFF;
  int index_2 = id2 & 0xFF;
  DCHECK_NE(index_1, index_2) << id1 << " and " << id2;
  bool first_is_internal = IsInternalDisplayId(id1);
  bool second_is_internal = IsInternalDisplayId(id2);
  if (first_is_internal && !second_is_internal)
    return true;
  if (!first_is_internal && second_is_internal)
    return false;
  return index_1 < index_2;
}

bool IsInternalDisplayId(int64_t display_id) {
  return base::Contains(*internal_display_ids(), display_id);
}

const base::flat_set<int64_t>& GetInternalDisplayIds() {
  return *internal_display_ids();
}

// static
bool HasInternalDisplay() {
  return !GetInternalDisplayIds().empty();
}

void SetInternalDisplayIds(base::flat_set<int64_t> display_ids) {
  *internal_display_ids() = std::move(display_ids);
}

gfx::ColorSpace ForcedColorProfileStringToColorSpace(const std::string& value) {
  if (value == "srgb")
    return gfx::ColorSpace::CreateSRGB();
  if (value == "display-p3-d65")
    return gfx::ColorSpace::CreateDisplayP3D65();
  if (value == "rec2020") {
    return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                           gfx::ColorSpace::TransferID::BT2020_10);
  }
  if (value == "scrgb-linear")
    return gfx::ColorSpace::CreateSRGBLinear();
  if (value == "hdr10")
    return gfx::ColorSpace::CreateHDR10();
  if (value == "extended-srgb")
    return gfx::ColorSpace::CreateExtendedSRGB();
  if (value == "generic-rgb") {
    return gfx::ColorSpace(gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
                           gfx::ColorSpace::TransferID::GAMMA18);
  }
  if (value == "color-spin-gamma24") {
    // Run this color profile through an ICC profile. The resulting color space
    // is slightly different from the input color space, and removing the ICC
    // profile would require rebaselining many layout tests.
    gfx::ColorSpace color_space(
        gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN,
        gfx::ColorSpace::TransferID::GAMMA24);
    return gfx::ICCProfile::FromColorSpace(color_space).GetColorSpace();
  }
  LOG(ERROR) << "Invalid forced color profile: \"" << value << "\"";
  return gfx::ColorSpace::CreateSRGB();
}

gfx::ColorSpace GetForcedDisplayColorProfile() {
  DCHECK(HasForceDisplayColorProfile());
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          /*switches::kForceDisplayColorProfile=*/"force-color-profile");
  return ForcedColorProfileStringToColorSpace(value);
}

bool HasForceDisplayColorProfile() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      /*switches::kForceDisplayColorProfile=*/"force-color-profile");
}

#if BUILDFLAG(IS_CHROMEOS)
// Constructs the raster DisplayColorSpaces out of |snapshot_color_space|,
// including the HDR ones if present and |allow_high_bit_depth| is set.
gfx::DisplayColorSpaces CreateDisplayColorSpaces(
    const gfx::ColorSpace& snapshot_color_space,
    bool allow_high_bit_depth,
    const std::optional<gfx::HDRStaticMetadata>& hdr_static_metadata) {
  if (HasForceDisplayColorProfile()) {
    return gfx::DisplayColorSpaces(GetForcedDisplayColorProfile(),
                                   DisplaySnapshot::PrimaryFormat());
  }

  // ChromeOS VMs (e.g. amd64-generic or betty) have INVALID Primaries; just
  // pass the color space along.
  if (!snapshot_color_space.IsValid()) {
    return gfx::DisplayColorSpaces(snapshot_color_space,
                                   DisplaySnapshot::PrimaryFormat());
  }

  // Make all displays report that they have sRGB primaries. Hardware color
  // management will convert to the device's color primaries.
  skcms_Matrix3x3 primary_matrix = SkNamedGamut::kSRGB;

  // Reconstruct the native colorspace with an IEC61966 2.1 transfer function
  // for SDR content (matching that of sRGB).
  gfx::ColorSpace sdr_color_space = gfx::ColorSpace::CreateCustom(
      primary_matrix, gfx::ColorSpace::TransferID::SRGB);

  // Use that color space for all content.
  gfx::DisplayColorSpaces display_color_spaces = gfx::DisplayColorSpaces(
      sdr_color_space, DisplaySnapshot::PrimaryFormat());

  // Claim 10% HDR headroom if HDR is available.
  if (allow_high_bit_depth && snapshot_color_space.IsHDR()) {
    gfx::ColorSpace hdr_color_space = gfx::ColorSpace::CreateCustom(
        primary_matrix, gfx::ColorSpace::TransferID::SRGB_HDR);

    display_color_spaces.SetOutputColorSpaceAndBufferFormat(
        gfx::ContentColorUsage::kHDR, false /* needs_alpha */, hdr_color_space,
        gfx::BufferFormat::RGBA_1010102);
    display_color_spaces.SetOutputColorSpaceAndBufferFormat(
        gfx::ContentColorUsage::kHDR, true /* needs_alpha */, hdr_color_space,
        gfx::BufferFormat::RGBA_1010102);
    display_color_spaces.SetHDRMaxLuminanceRelative(1.1f);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (allow_high_bit_depth &&
      snapshot_color_space == gfx::ColorSpace::CreateHDR10() &&
      base::FeatureList::IsEnabled(
          display::features::kEnableExternalDisplayHDR10Mode)) {
    // This forces the main UI plane to be always HDR10 regardless of
    // ContentColorUsage. BT2020 primaries and PQ transfer function require a
    // 10-bit buffer.
    display_color_spaces = gfx::DisplayColorSpaces(
        gfx::ColorSpace::CreateHDR10(), gfx::BufferFormat::RGBA_1010102);
    // TODO(b/165822222): Set initial luminance values based on display
    // brightness
    display_color_spaces.SetSDRMaxLuminanceNits(
        hdr_static_metadata->max / kDefaultHdrMaxLuminanceRelative);
    display_color_spaces.SetHDRMaxLuminanceRelative(
        kDefaultHdrMaxLuminanceRelative);
  }
#endif
  return display_color_spaces;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

int ConnectorIndex8(int device_index, int display_index) {
  DCHECK_LT(device_index, 16);
  DCHECK_LT(display_index, 16);
  return ((device_index << 4) + display_index) & 0xFF;
}

uint16_t ConnectorIndex16(uint8_t device_index, uint8_t display_index) {
  return ((device_index << 8) + display_index) & 0xFFFF;
}

}  // namespace display
