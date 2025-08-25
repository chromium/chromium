// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_image_description.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {

gfx::ColorSpace::PrimaryID ToGfxPrimaryID(
    wp_color_manager_v1_primaries primaries) {
  switch (primaries) {
    case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB:
      return gfx::ColorSpace::PrimaryID::BT709;
    case WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M:
      return gfx::ColorSpace::PrimaryID::BT470M;
    case WP_COLOR_MANAGER_V1_PRIMARIES_PAL:
      // This could also be EBU_3213_E, but BT470BG is the broader
      // equivalent for PAL systems.
      return gfx::ColorSpace::PrimaryID::BT470BG;
    case WP_COLOR_MANAGER_V1_PRIMARIES_NTSC:
      // This could also be SMPTE240M, but SMPTE170M is the more
      // common standard for NTSC/BT.601.
      return gfx::ColorSpace::PrimaryID::SMPTE170M;
    case WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM:
      return gfx::ColorSpace::PrimaryID::FILM;
    case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020:
      return gfx::ColorSpace::PrimaryID::BT2020;
    case WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ:
      // This could also be XYZ_D50, but SMPTEST428_1 is explicitly
      // mentioned in the Wayland protocol documentation.
      return gfx::ColorSpace::PrimaryID::SMPTEST428_1;
    case WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3:
      return gfx::ColorSpace::PrimaryID::SMPTEST431_2;
    case WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3:
      return gfx::ColorSpace::PrimaryID::P3;
    case WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB:
      return gfx::ColorSpace::PrimaryID::ADOBE_RGB;
    default:
      return gfx::ColorSpace::PrimaryID::INVALID;
  }
}

gfx::ColorSpace::TransferID ToGfxTransferID(
    wp_color_manager_v1_transfer_function transfer) {
  switch (transfer) {
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886:
      // This is the transfer function for BT.709, BT.601, and SDR BT.2020.
      // BT709 is the most common and representative choice.
      return gfx::ColorSpace::TransferID::BT709;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
      return gfx::ColorSpace::TransferID::GAMMA22;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28:
      return gfx::ColorSpace::TransferID::GAMMA28;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240:
      return gfx::ColorSpace::TransferID::SMPTE240M;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR:
      // While this could also map to LINEAR_HDR, the base LINEAR is the
      // more fundamental choice for a generic linear transfer.
      return gfx::ColorSpace::TransferID::LINEAR;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100:
      return gfx::ColorSpace::TransferID::LOG;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316:
      return gfx::ColorSpace::TransferID::LOG_SQRT;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC:
      return gfx::ColorSpace::TransferID::IEC61966_2_4;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
      return gfx::ColorSpace::TransferID::SRGB;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB:
      return gfx::ColorSpace::TransferID::SRGB_HDR;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
      return gfx::ColorSpace::TransferID::PQ;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428:
      return gfx::ColorSpace::TransferID::SMPTEST428_1;
    case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG:
      return gfx::ColorSpace::TransferID::HLG;
    default:
      return gfx::ColorSpace::TransferID::INVALID;
  }
}

}  // namespace

WaylandWpImageDescription::WaylandWpImageDescription(
    wl::Object<wp_image_description_v1> image_description,
    WaylandConnection* connection,
    std::optional<gfx::ColorSpace> color_space,
    CreationCallback callback)
    : image_description_(std::move(image_description)),
      connection_(connection),
      creation_callback_(std::move(callback)) {
  DCHECK(image_description_);
  if (color_space.has_value()) {
    color_space_ = *color_space;
  }
  static constexpr wp_image_description_v1_listener kListener = {
      .failed = &OnFailed,
      .ready = &OnReady,
  };
  wp_image_description_v1_add_listener(image_description_.get(), &kListener,
                                       this);
}

WaylandWpImageDescription::~WaylandWpImageDescription() = default;

scoped_refptr<gfx::DisplayColorSpacesRef>
WaylandWpImageDescription::AsDisplayColorSpaces() const {
  auto display_color_spaces = gfx::DisplayColorSpaces(color_space_);

  // GetContentMaxLuminance returns a default of 1000 if the metadata does
  // not contain a peak luminance. Avoid this by checking first.
  if ((hdr_metadata_.cta_861_3 &&
       hdr_metadata_.cta_861_3->max_content_light_level > 0) ||
      (hdr_metadata_.smpte_st_2086 &&
       hdr_metadata_.smpte_st_2086->luminance_max > 0)) {
    float peak_brightness =
        gfx::HDRMetadata::GetContentMaxLuminance(hdr_metadata_);
    float sdr_nits = hdr_metadata_.ndwl
                         ? hdr_metadata_.ndwl->nits
                         : gfx::ColorSpace::kDefaultSDRWhiteLevel;
    if (sdr_nits > 0.f) {
      display_color_spaces.SetHDRMaxLuminanceRelative(peak_brightness /
                                                      sdr_nits);
    }
  }

  return base::MakeRefCounted<gfx::DisplayColorSpacesRef>(
      std::move(display_color_spaces));
}

void WaylandWpImageDescription::HandleReady() {
  if (creation_callback_) {
    std::move(creation_callback_).Run(this);
  }
}

// static
void WaylandWpImageDescription::OnFailed(
    void* data,
    wp_image_description_v1* image_description,
    uint32_t cause,
    const char* msg) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  LOG(ERROR) << "Failed to create image description: " << msg;
  if (self->creation_callback_) {
    std::move(self->creation_callback_).Run(nullptr);
  }
}

// static
void WaylandWpImageDescription::OnReady(
    void* data,
    wp_image_description_v1* image_description,
    uint32_t identity) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);

  // If this description was created from a gfx::ColorSpace, it's ready.
  if (self->color_space_.IsValid()) {
    self->HandleReady();
    return;
  }

  // Otherwise, this description came from the compositor (e.g., an output),
  // and we need to get its parameters to build a gfx::ColorSpace.
  static constexpr wp_image_description_info_v1_listener kInfoListener = {
      .done = &OnInfoDone,
      .icc_file = &OnInfoIccFile,
      .primaries = &OnInfoPrimaries,
      .primaries_named = &OnInfoPrimariesNamed,
      .tf_power = &OnInfoTfPower,
      .tf_named = &OnInfoTfNamed,
      .luminances = &OnInfoLuminances,
      .target_primaries = &OnInfoTargetPrimaries,
      .target_luminance = &OnInfoTargetLuminance,
      .target_max_cll = &OnInfoTargetMaxCll,
      .target_max_fall = &OnInfoTargetMaxFall,
  };
  self->info_.reset(wp_image_description_v1_get_information(image_description));
  wp_image_description_info_v1_add_listener(self->info_.get(), &kInfoListener,
                                            self);
}

// static
void WaylandWpImageDescription::OnInfoDone(
    void* data,
    wp_image_description_info_v1* image_info) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);

  // Construct ColorSpace from gathered info.
  if (self->pending_custom_primaries_ && self->pending_custom_transfer_fn_) {
    self->color_space_ = gfx::ColorSpace::CreateCustom(
        *self->pending_custom_primaries_, *self->pending_custom_transfer_fn_);
  } else if (self->pending_custom_primaries_ && self->pending_transfer_id_) {
    self->color_space_ = gfx::ColorSpace::CreateCustom(
        *self->pending_custom_primaries_, *self->pending_transfer_id_);
  } else if (self->pending_primary_id_ && self->pending_custom_transfer_fn_) {
    skcms_Matrix3x3 to_xyz;
    gfx::ColorSpace(*self->pending_primary_id_,
                    gfx::ColorSpace::TransferID::SRGB)
        .GetPrimaryMatrix(&to_xyz);
    self->color_space_ = gfx::ColorSpace::CreateCustom(
        to_xyz, *self->pending_custom_transfer_fn_);
  } else if (self->pending_primary_id_ && self->pending_transfer_id_) {
    self->color_space_ = gfx::ColorSpace(
        *self->pending_primary_id_, *self->pending_transfer_id_,
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  } else {
    LOG(ERROR) << "Incomplete image description info from compositor.";
    self->color_space_ = gfx::ColorSpace::CreateSRGB();
  }

  self->pending_custom_primaries_.reset();
  self->pending_custom_transfer_fn_.reset();
  self->pending_primary_id_.reset();
  self->pending_transfer_id_.reset();

  // The info object is implicitly destroyed by the server after done.
  self->info_.reset();

  self->HandleReady();
}

// static
void WaylandWpImageDescription::OnInfoIccFile(
    void* data,
    wp_image_description_info_v1* image_info,
    int32_t icc,
    uint32_t icc_size) {
  NOTIMPLEMENTED_LOG_ONCE();
  close(icc);
}

// static
void WaylandWpImageDescription::OnInfoPrimaries(
    void* data,
    wp_image_description_info_v1* image_info,
    int32_t r_x,
    int32_t r_y,
    int32_t g_x,
    int32_t g_y,
    int32_t b_x,
    int32_t b_y,
    int32_t w_x,
    int32_t w_y) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  SkColorSpacePrimaries primaries = {
      r_x / 1000000.f, r_y / 1000000.f, g_x / 1000000.f, g_y / 1000000.f,
      b_x / 1000000.f, b_y / 1000000.f, w_x / 1000000.f, w_y / 1000000.f};
  skcms_Matrix3x3 pending_custom_primaries;
  if (primaries.toXYZD50(&pending_custom_primaries)) {
    self->pending_custom_primaries_ = pending_custom_primaries;
  }
}

// static
void WaylandWpImageDescription::OnInfoTfNamed(
    void* data,
    wp_image_description_info_v1* image_info,
    uint32_t tf) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  self->pending_transfer_id_ =
      ToGfxTransferID(static_cast<wp_color_manager_v1_transfer_function>(tf));
}

void WaylandWpImageDescription::OnInfoTfPower(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t eexp) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  self->pending_custom_transfer_fn_ = skcms_TransferFunction{
      .g = eexp / 10000.f, .a = 1.0f, .b = 0, .c = 0, .d = 0, .e = 0, .f = 0};
}

void WaylandWpImageDescription::OnInfoLuminances(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t min_lum,
    uint32_t max_lum,
    uint32_t reference_lum) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  // The reference white luminance corresponds to SDR white level (or nominal
  // diffuse white level).
  self->hdr_metadata_.ndwl = gfx::HdrMetadataNdwl(reference_lum);
}

void WaylandWpImageDescription::OnInfoTargetPrimaries(
    void* data,
    wp_image_description_info_v1* info,
    int32_t r_x,
    int32_t r_y,
    int32_t g_x,
    int32_t g_y,
    int32_t b_x,
    int32_t b_y,
    int32_t w_x,
    int32_t w_y) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (!self->hdr_metadata_.smpte_st_2086) {
    self->hdr_metadata_.smpte_st_2086.emplace();
  }
  self->hdr_metadata_.smpte_st_2086->primaries = {
      r_x / 1000000.f, r_y / 1000000.f, g_x / 1000000.f, g_y / 1000000.f,
      b_x / 1000000.f, b_y / 1000000.f, w_x / 1000000.f, w_y / 1000000.f};
}

void WaylandWpImageDescription::OnInfoTargetLuminance(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t min_lum,
    uint32_t max_lum) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (!self->hdr_metadata_.smpte_st_2086) {
    self->hdr_metadata_.smpte_st_2086.emplace();
  }
  self->hdr_metadata_.smpte_st_2086->luminance_min = min_lum / 10000.f;
  self->hdr_metadata_.smpte_st_2086->luminance_max = max_lum;
}

void WaylandWpImageDescription::OnInfoTargetMaxCll(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t max_cll) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (!self->hdr_metadata_.cta_861_3) {
    self->hdr_metadata_.cta_861_3.emplace();
  }
  self->hdr_metadata_.cta_861_3->max_content_light_level = max_cll;
}

void WaylandWpImageDescription::OnInfoTargetMaxFall(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t max_fall) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (!self->hdr_metadata_.cta_861_3) {
    self->hdr_metadata_.cta_861_3.emplace();
  }
  self->hdr_metadata_.cta_861_3->max_frame_average_light_level = max_fall;
}

// static
void WaylandWpImageDescription::OnInfoPrimariesNamed(
    void* data,
    wp_image_description_info_v1* image_info,
    uint32_t primaries) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  self->pending_primary_id_ =
      ToGfxPrimaryID(static_cast<wp_color_manager_v1_primaries>(primaries));
}

}  // namespace ui
