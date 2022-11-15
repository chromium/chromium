// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space.h"

#include <chrome-color-management-client-protocol.h>
#include <cstdint>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "ui/base/wayland/color_manager_util.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_utils.h"

namespace ui {

WaylandZcrColorSpace::WaylandZcrColorSpace(
    struct zcr_color_space_v1* color_space)
    : zcr_color_space_(color_space) {
  DCHECK(color_space);
  static const zcr_color_space_v1_listener listener = {
      &WaylandZcrColorSpace::OnIccFile,
      &WaylandZcrColorSpace::OnNames,
      &WaylandZcrColorSpace::OnParams,
      &WaylandZcrColorSpace::OnDone,
  };

  zcr_color_space_v1_add_listener(zcr_color_space_.get(), &listener, this);
  zcr_color_space_v1_get_information(zcr_color_space_.get());
}

WaylandZcrColorSpace::~WaylandZcrColorSpace() = default;

// static
void WaylandZcrColorSpace::OnIccFile(void* data,
                                     struct zcr_color_space_v1* cs,
                                     int32_t icc,
                                     uint32_t icc_size) {
  WaylandZcrColorSpace* zcr_color_space =
      static_cast<WaylandZcrColorSpace*>(data);
  DCHECK(zcr_color_space);
  // TODO(b/192562912): construct a color space from an icc file.
}

// static
void WaylandZcrColorSpace::OnNames(void* data,
                                   struct zcr_color_space_v1* cs,
                                   uint32_t eotf,
                                   uint32_t chromaticity,
                                   uint32_t whitepoint) {
  WaylandZcrColorSpace* zcr_color_space =
      static_cast<WaylandZcrColorSpace*>(data);
  DCHECK(zcr_color_space);
  auto primaryID = ui::wayland::kChromaticityMap.contains(chromaticity)
                       ? ui::wayland::kChromaticityMap.at(chromaticity)
                       : gfx::ColorSpace::PrimaryID::INVALID;
  auto transferID = ui::wayland::kEotfMap.contains(eotf)
                        ? ui::wayland::kEotfMap.at(eotf)
                        : gfx::ColorSpace::TransferID::INVALID;
  if (transferID == gfx::ColorSpace::TransferID::INVALID &&
      wayland::kHDRTransferMap.contains(eotf)) {
    auto transfer_fn = ui::wayland::kHDRTransferMap.at(eotf);
    zcr_color_space
        ->gathered_information[static_cast<uint8_t>(InformationType::kNames)] =
        gfx::ColorSpace(primaryID, gfx::ColorSpace::TransferID::CUSTOM_HDR,
                        gfx::ColorSpace::MatrixID::RGB,
                        gfx::ColorSpace::RangeID::FULL, nullptr, &transfer_fn,
                        /*is_hdr=*/true);
    return;
  }
  zcr_color_space
      ->gathered_information[static_cast<uint8_t>(InformationType::kNames)] =
      gfx::ColorSpace(primaryID, transferID, gfx::ColorSpace::MatrixID::RGB,
                      gfx::ColorSpace::RangeID::FULL);
}

// static
void WaylandZcrColorSpace::OnParams(void* data,
                                    struct zcr_color_space_v1* cs,
                                    uint32_t eotf,
                                    uint32_t primary_r_x,
                                    uint32_t primary_r_y,
                                    uint32_t primary_g_x,
                                    uint32_t primary_g_y,
                                    uint32_t primary_b_x,
                                    uint32_t primary_b_y,
                                    uint32_t whitepoint_x,
                                    uint32_t whitepoint_y) {
  WaylandZcrColorSpace* zcr_color_space =
      static_cast<WaylandZcrColorSpace*>(data);
  DCHECK(zcr_color_space);
  SkColorSpacePrimaries primaries = {
      PARAM_TO_FLOAT(primary_r_x),  PARAM_TO_FLOAT(primary_r_y),
      PARAM_TO_FLOAT(primary_g_x),  PARAM_TO_FLOAT(primary_g_y),
      PARAM_TO_FLOAT(primary_b_x),  PARAM_TO_FLOAT(primary_b_y),
      PARAM_TO_FLOAT(whitepoint_x), PARAM_TO_FLOAT(whitepoint_y)};

  skcms_Matrix3x3 xyzd50 = {};
  if (!primaries.toXYZD50(&xyzd50)) {
    DLOG(ERROR) << base::StringPrintf(
        "Unable to translate color space primaries to XYZD50: "
        "{%f, %f, %f, %f, %f, %f, %f, %f}",
        primaries.fRX, primaries.fRY, primaries.fGX, primaries.fGY,
        primaries.fBX, primaries.fBY, primaries.fWX, primaries.fWY);
    return;
  }

  auto transferID = ui::wayland::kEotfMap.contains(eotf)
                        ? ui::wayland::kEotfMap.at(eotf)
                        : gfx::ColorSpace::TransferID::INVALID;
  if (transferID == gfx::ColorSpace::TransferID::INVALID &&
      ui::wayland::kHDRTransferMap.contains(eotf)) {
    auto transfer_fn = ui::wayland::kHDRTransferMap.at(eotf);
    zcr_color_space
        ->gathered_information[static_cast<uint8_t>(InformationType::kParams)] =
        gfx::ColorSpace(gfx::ColorSpace::PrimaryID::CUSTOM,
                        gfx::ColorSpace::TransferID::CUSTOM_HDR,
                        gfx::ColorSpace::MatrixID::RGB,
                        gfx::ColorSpace::RangeID::FULL, &xyzd50, &transfer_fn,
                        true);

    return;
  }
  zcr_color_space
      ->gathered_information[static_cast<uint8_t>(InformationType::kParams)] =
      gfx::ColorSpace::CreateCustom(xyzd50, transferID);
}

gfx::ColorSpace WaylandZcrColorSpace::GetPriorityInformationType() {
  for (auto maybe_colorspace : gathered_information) {
    if (maybe_colorspace.has_value())
      return maybe_colorspace.value();
  }
  DLOG(ERROR) << "No color space information gathered";
  return gfx::ColorSpace::CreateSRGB();
}

// static
void WaylandZcrColorSpace::OnDone(void* data, struct zcr_color_space_v1* cs) {
  WaylandZcrColorSpace* zcr_color_space =
      static_cast<WaylandZcrColorSpace*>(data);
  DCHECK(zcr_color_space);
  if (zcr_color_space->HasColorSpaceDoneCallback())
    std::move(zcr_color_space->color_space_done_callback_)
        .Run(zcr_color_space->GetPriorityInformationType());
  zcr_color_space->gathered_information.fill({});
}

}  // namespace ui
