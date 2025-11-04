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
#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

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

bool TransferIsPowerCurve(gfx::ColorSpace::TransferID transfer) {
  switch (transfer) {
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
    case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::GAMMA18:
    case gfx::ColorSpace::TransferID::GAMMA22:
    case gfx::ColorSpace::TransferID::GAMMA24:
    case gfx::ColorSpace::TransferID::GAMMA28:
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
      return true;
    case gfx::ColorSpace::TransferID::INVALID:
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::SMPTE240M:
    case gfx::ColorSpace::TransferID::LOG:
    case gfx::ColorSpace::TransferID::LOG_SQRT:
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
    case gfx::ColorSpace::TransferID::BT1361_ECG:
    case gfx::ColorSpace::TransferID::SRGB:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::PQ:
    case gfx::ColorSpace::TransferID::HLG:
    case gfx::ColorSpace::TransferID::SRGB_HDR:
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
      return false;
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
  display_color_spaces.SetSDRMaxLuminanceNits(sdr_max_luminance_nits_);
  display_color_spaces.SetHDRMaxLuminanceRelative(hdr_max_luminance_relative_);
  return base::MakeRefCounted<gfx::DisplayColorSpacesRef>(
      std::move(display_color_spaces));
}

void WaylandWpImageDescription::HandleReady() {
  if (creation_callback_) {
    std::move(creation_callback_).Run(this);
  }
}

gfx::ColorSpace WaylandWpImageDescription::CreateColorSpaceFromPendingInfo(
    bool is_hdr) const {
  std::optional<gfx::ColorSpace::PrimaryID> primaries;
  std::optional<gfx::ColorSpace::TransferID> transfer;
  const skcms_Matrix3x3* custom_primaries = nullptr;
  const skcms_TransferFunction* custom_transfer_fn = nullptr;

  if (pending_primary_id_) {
    primaries = *pending_primary_id_;
  } else if (pending_custom_primaries_) {
    primaries = gfx::ColorSpace::PrimaryID::CUSTOM;
    custom_primaries = &*pending_custom_primaries_;
  }
  if (pending_transfer_id_) {
    transfer = *pending_transfer_id_;
  } else if (pending_custom_transfer_fn_) {
    transfer = is_hdr ? gfx::ColorSpace::TransferID::CUSTOM_HDR
                      : gfx::ColorSpace::TransferID::CUSTOM;
    custom_transfer_fn = &*pending_custom_transfer_fn_;
  }

  if (!primaries || !transfer) {
    LOG(ERROR) << "Incomplete image description info from compositor.";
    return gfx::ColorSpace::CreateSRGB();
  }

  gfx::ColorSpace color_space(
      *primaries, *transfer, gfx::ColorSpace::MatrixID::RGB,
      gfx::ColorSpace::RangeID::FULL, custom_primaries, custom_transfer_fn);

  // gfx::ColorSpace decides HDR based on the transfer function. If there's
  // any HDR headroom but the transfer function is something like gamma 2.2,
  // force a different transfer function. This workaround may be removed once
  // gfx::ColorSpace::IsHDR() is removed.
  if (!is_hdr || color_space_.IsHDR()) {
    return color_space;
  }

  auto* color_manager = connection_->wp_color_manager();
  if (color_manager->IsSupportedFeature(
          WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER) &&
      TransferIsPowerCurve(*transfer)) {
    // Convert to a CUSTOM_HDR transfer function if it's supported by the
    // compositor.
    return color_space.GetAsHDR();
  }

  constexpr struct {
    wp_color_manager_v1_transfer_function wl_transfer;
    gfx::ColorSpace::TransferID gfx_transfer;
  } kFallbackHdrTransfers[] = {
      {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ,
       gfx::ColorSpace::TransferID::PQ},
      {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG,
       gfx::ColorSpace::TransferID::HLG},
      {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB,
       gfx::ColorSpace::TransferID::SRGB_HDR},
      {WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR,
       gfx::ColorSpace::TransferID::LINEAR_HDR},
  };

  for (const auto& fallback : kFallbackHdrTransfers) {
    if (color_manager->IsSupportedTransferFunction(fallback.wl_transfer)) {
      return gfx::ColorSpace(
          *primaries, fallback.gfx_transfer, gfx::ColorSpace::MatrixID::RGB,
          gfx::ColorSpace::RangeID::FULL, custom_primaries, custom_transfer_fn);
    }
  }

  LOG(ERROR) << "No valid fallback HDR transfer function.";
  return gfx::ColorSpace::CreateSRGB();
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

  self->sdr_max_luminance_nits_ = self->pending_reference_lum_.value_or(
      gfx::ColorSpace::kDefaultSDRWhiteLevel);
  const uint32_t max_lum =
      self->pending_target_max_lum_.value_or(self->sdr_max_luminance_nits_);
  self->hdr_max_luminance_relative_ = max_lum / self->sdr_max_luminance_nits_;

  bool is_hdr = self->hdr_max_luminance_relative_ > 1.f;
  self->color_space_ = self->CreateColorSpaceFromPendingInfo(is_hdr);

  self->pending_custom_primaries_.reset();
  self->pending_custom_transfer_fn_.reset();
  self->pending_primary_id_.reset();
  self->pending_transfer_id_.reset();
  self->pending_reference_lum_.reset();
  self->pending_target_max_lum_.reset();

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

// static
void WaylandWpImageDescription::OnInfoTfPower(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t eexp) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  self->pending_custom_transfer_fn_ = skcms_TransferFunction{
      .g = eexp / 10000.f, .a = 1.0f, .b = 0, .c = 0, .d = 0, .e = 0, .f = 0};
}

// static
void WaylandWpImageDescription::OnInfoLuminances(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t min_lum,
    uint32_t max_lum,
    uint32_t reference_lum) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (reference_lum) {
    self->pending_reference_lum_ = reference_lum;
  }
}

// static
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
    int32_t w_y) {}

// static
void WaylandWpImageDescription::OnInfoTargetLuminance(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t min_lum,
    uint32_t max_lum) {
  auto* self = static_cast<WaylandWpImageDescription*>(data);
  DCHECK(self);
  if (max_lum) {
    self->pending_target_max_lum_ = max_lum;
  }
}

// static
void WaylandWpImageDescription::OnInfoTargetMaxCll(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t max_cll) {}

// static
void WaylandWpImageDescription::OnInfoTargetMaxFall(
    void* data,
    wp_image_description_info_v1* info,
    uint32_t max_fall) {}

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
