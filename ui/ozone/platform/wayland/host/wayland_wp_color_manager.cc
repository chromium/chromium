// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_wp_color_manager.h"

#include <color-management-v1-client-protocol.h>

#include "base/feature_list.h"
#include "base/logging.h"
#include "ui/gfx/hdr_metadata_agtm.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"

namespace ui {

namespace {

constexpr uint32_t kMinVersion = 1;

BASE_FEATURE(kWaylandWpColorManagerV1, base::FEATURE_ENABLED_BY_DEFAULT);

std::optional<wp_color_manager_v1_primaries> ColorSpaceToPrimaries(
    gfx::ColorSpace::PrimaryID primary_id) {
  switch (primary_id) {
    case gfx::ColorSpace::PrimaryID::BT709:
      return WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
    case gfx::ColorSpace::PrimaryID::BT470M:
      return WP_COLOR_MANAGER_V1_PRIMARIES_PAL_M;
    case gfx::ColorSpace::PrimaryID::BT470BG:
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      return WP_COLOR_MANAGER_V1_PRIMARIES_PAL;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      return WP_COLOR_MANAGER_V1_PRIMARIES_NTSC;
    case gfx::ColorSpace::PrimaryID::FILM:
      return WP_COLOR_MANAGER_V1_PRIMARIES_GENERIC_FILM;
    case gfx::ColorSpace::PrimaryID::BT2020:
      return WP_COLOR_MANAGER_V1_PRIMARIES_BT2020;
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
    case gfx::ColorSpace::PrimaryID::XYZ_D50:
      return WP_COLOR_MANAGER_V1_PRIMARIES_CIE1931_XYZ;
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
      return WP_COLOR_MANAGER_V1_PRIMARIES_DCI_P3;
    case gfx::ColorSpace::PrimaryID::P3:
      return WP_COLOR_MANAGER_V1_PRIMARIES_DISPLAY_P3;
    case gfx::ColorSpace::PrimaryID::ADOBE_RGB:
      return WP_COLOR_MANAGER_V1_PRIMARIES_ADOBE_RGB;
    case gfx::ColorSpace::PrimaryID::INVALID:
    case gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
    case gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
    case gfx::ColorSpace::PrimaryID::CUSTOM:
      // These do not have a direct mapping in the Wayland protocol.
      return std::nullopt;
  }
}

std::optional<wp_color_manager_v1_transfer_function>
TransferIdToTransferFunction(gfx::ColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::BT1361_ECG:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
    case gfx::ColorSpace::TransferID::GAMMA22:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
    case gfx::ColorSpace::TransferID::GAMMA28:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA28;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST240;
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
    case gfx::ColorSpace::TransferID::LOG:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_100;
    case gfx::ColorSpace::TransferID::LOG_SQRT:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_LOG_316;
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_XVYCC;
    case gfx::ColorSpace::TransferID::SRGB:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
    case gfx::ColorSpace::TransferID::SRGB_HDR:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_SRGB;
    case gfx::ColorSpace::TransferID::PQ:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST428;
    case gfx::ColorSpace::TransferID::HLG:
      return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG;
    case gfx::ColorSpace::TransferID::INVALID:
    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::GAMMA18:
    case gfx::ColorSpace::TransferID::GAMMA24:
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
    case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      // These do not have a direct mapping in the Wayland protocol.
      return std::nullopt;
  }
}

float GetReferenceLuminance(const gfx::ColorSpace& color_space,
                            const gfx::HDRMetadata& hdr_metadata) {
  gfx::HdrMetadataAgtmParsed agtm;
  if (hdr_metadata.agtm.has_value() && agtm.Parse(hdr_metadata.agtm.value())) {
    return agtm.hdr_reference_white;
  }

  if (hdr_metadata.ndwl.has_value() && hdr_metadata.ndwl->nits > 0.f) {
    return hdr_metadata.ndwl->nits;
  }

  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::PQ ||
      color_space.GetTransferID() == gfx::ColorSpace::TransferID::HLG) {
    auto sk_color_space = color_space.ToSkColorSpace();
    skcms_TransferFunction transfer_fn;
    sk_color_space->transferFn(&transfer_fn);
    return transfer_fn.a;
  }

  return gfx::ColorSpace::kDefaultSDRWhiteLevel;
}

}  // namespace

// static
constexpr char WaylandWpColorManager::kInterfaceName[];

// static
void WaylandWpColorManager::Instantiate(WaylandConnection* connection,
                                        wl_registry* registry,
                                        uint32_t name,
                                        const std::string& interface,
                                        uint32_t version) {
  if (!base::FeatureList::IsEnabled(kWaylandWpColorManagerV1)) {
    return;
  }

  CHECK_EQ(interface, kInterfaceName)
      << "Expected " << kInterfaceName << " but got " << interface;

  if (connection->wp_color_manager() ||
      !wl::CanBind(interface, version, kMinVersion, kMinVersion)) {
    return;
  }

  auto manager = wl::Bind<wp_color_manager_v1>(registry, name, kMinVersion);
  if (!manager) {
    LOG(ERROR) << "Failed to bind " << kInterfaceName;
    return;
  }
  connection->wp_color_manager_ =
      std::make_unique<WaylandWpColorManager>(manager.release(), connection);
}

WaylandWpColorManager::WaylandWpColorManager(wp_color_manager_v1* color_manager,
                                             WaylandConnection* connection)
    : manager_(color_manager), connection_(connection) {
  DCHECK(manager_);
  static constexpr wp_color_manager_v1_listener kListener = {
      .supported_intent = &OnSupportedIntent,
      .supported_feature = &OnSupportedFeature,
      .supported_tf_named = &OnSupportedTfNamed,
      .supported_primaries_named = &OnSupportedPrimariesNamed,
      .done = &OnDone,
  };
  wp_color_manager_v1_add_listener(manager_.get(), &kListener, this);
}

WaylandWpColorManager::~WaylandWpColorManager() = default;

void WaylandWpColorManager::GetImageDescription(
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    WaylandWpImageDescription::CreationCallback callback) {
  ImageDescription key = {color_space, hdr_metadata};

  if (auto it = image_description_cache_.Get(key);
      it != image_description_cache_.end()) {
    std::move(callback).Run(it->second);
    return;
  }

  pending_callbacks_[key].push_back(std::move(callback));
  // Return early if there's already a pending creation for this key.
  if (pending_callbacks_[key].size() > 1) {
    return;
  }

  auto cleanup = [&]() {
    for (auto& cb : pending_callbacks_[key]) {
      std::move(cb).Run(nullptr);
    }
    pending_callbacks_.erase(key);
  };

  if (!IsSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)) {
    LOG(ERROR) << "Server does not support parametric color descriptions.";
    cleanup();
    return;
  }

  // Create a new image description.
  auto creator = wl::Object<wp_image_description_creator_params_v1>(
      wp_color_manager_v1_create_parametric_creator(manager_.get()));
  if (!creator) {
    LOG(ERROR) << "Failed to create wp_image_description_creator_params_v1";
    cleanup();
    return;
  }
  if (!PopulateDescriptionCreator(creator.get(), color_space, hdr_metadata)) {
    LOG(ERROR) << "Failed to populate image description for color space "
               << color_space.ToString();
    cleanup();
    return;
  }

  auto image_description_object = wl::Object<wp_image_description_v1>(
      wp_image_description_creator_params_v1_create(creator.release()));
  if (!image_description_object) {
    LOG(ERROR) << "Failed to create wp_image_description_v1";
    cleanup();
    return;
  }

  pending_creations_[key] = base::MakeRefCounted<WaylandWpImageDescription>(
      std::move(image_description_object), connection_, color_space,
      base::BindOnce(&WaylandWpColorManager::OnImageDescriptionCreated,
                     weak_factory_.GetWeakPtr(), color_space, hdr_metadata));
}

void WaylandWpColorManager::OnImageDescriptionCreated(
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata,
    scoped_refptr<WaylandWpImageDescription> image_description) {
  ImageDescription key = {color_space, hdr_metadata};
  auto creation_it = pending_creations_.find(key);
  CHECK(creation_it != pending_creations_.end());
  pending_creations_.erase(creation_it);

  if (image_description) {
    image_description_cache_.Put(key, image_description);
  }

  auto callback_it = pending_callbacks_.find(key);
  CHECK(callback_it != pending_callbacks_.end());
  for (auto& cb : callback_it->second) {
    std::move(cb).Run(image_description);
  }
  pending_callbacks_.erase(callback_it);
}

bool WaylandWpColorManager::PopulateDescriptionCreator(
    wp_image_description_creator_params_v1* creator,
    const gfx::ColorSpace& color_space,
    const gfx::HDRMetadata& hdr_metadata) {
  // Set named primaries if possible.
  if (auto primary = ColorSpaceToPrimaries(color_space.GetPrimaryID());
      primary && IsSupportedPrimaries(*primary)) {
    wp_image_description_creator_params_v1_set_primaries_named(creator,
                                                               *primary);
  } else if (IsSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES)) {
    auto primaries = color_space.GetPrimaries();
    wp_image_description_creator_params_v1_set_primaries(
        creator, static_cast<int32_t>(primaries.fRX * 1000000),
        static_cast<int32_t>(primaries.fRY * 1000000),
        static_cast<int32_t>(primaries.fGX * 1000000),
        static_cast<int32_t>(primaries.fGY * 1000000),
        static_cast<int32_t>(primaries.fBX * 1000000),
        static_cast<int32_t>(primaries.fBY * 1000000),
        static_cast<int32_t>(primaries.fWX * 1000000),
        static_cast<int32_t>(primaries.fWY * 1000000));
  } else {
    LOG(ERROR) << "Unable to set image primaries.";
    return false;
  }

  // Set named transfer function if possible.
  if (auto transfer = TransferIdToTransferFunction(color_space.GetTransferID());
      transfer && IsSupportedTransferFunction(*transfer)) {
    wp_image_description_creator_params_v1_set_tf_named(creator, *transfer);
  } else if (IsSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER)) {
    skcms_TransferFunction fn;
    // The protocol only supports a pure power curve. Check for that.
    if (color_space.GetTransferFunction(&fn) && fn.a == 1.0f && fn.b == 0.0f &&
        fn.c == 0.0f && fn.d == 0.0f && fn.e == 0.0f && fn.f == 0.0f) {
      wp_image_description_creator_params_v1_set_tf_power(
          creator, static_cast<uint32_t>(fn.g * 10000));
    } else {
      LOG(ERROR) << "Unable to set non-power-curve transfer function.";
      return false;
    }
  } else {
    LOG(ERROR) << "Unable to set image transfer function.";
    return false;
  }

  if (color_space.IsHDR()) {
    if (IsSupportedFeature(
            WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES)) {
      if (hdr_metadata.smpte_st_2086 && hdr_metadata.smpte_st_2086->IsValid()) {
        const auto& smpte_st_2086 = *hdr_metadata.smpte_st_2086;
        const auto& primaries = smpte_st_2086.primaries;
        wp_image_description_creator_params_v1_set_mastering_display_primaries(
            creator, static_cast<int32_t>(primaries.fRX * 1000000),
            static_cast<int32_t>(primaries.fRY * 1000000),
            static_cast<int32_t>(primaries.fGX * 1000000),
            static_cast<int32_t>(primaries.fGY * 1000000),
            static_cast<int32_t>(primaries.fBX * 1000000),
            static_cast<int32_t>(primaries.fBY * 1000000),
            static_cast<int32_t>(primaries.fWX * 1000000),
            static_cast<int32_t>(primaries.fWY * 1000000));

        wp_image_description_creator_params_v1_set_mastering_luminance(
            creator, static_cast<uint32_t>(smpte_st_2086.luminance_min * 10000),
            static_cast<uint32_t>(smpte_st_2086.luminance_max));
      }
    }

    const float ref_luma = GetReferenceLuminance(color_space, hdr_metadata);
    if (IsSupportedFeature(WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES)) {
      wp_image_description_creator_params_v1_set_luminances(
          creator, 0, gfx::HDRMetadata::GetContentMaxLuminance(hdr_metadata),
          ref_luma);
    }

    uint32_t cll = ref_luma;
    uint32_t fall = ref_luma;
    if (hdr_metadata.cta_861_3 && hdr_metadata.cta_861_3->IsValid()) {
      const auto& cta_861_3 = *hdr_metadata.cta_861_3;
      if (cta_861_3.max_content_light_level > 0) {
        cll = cta_861_3.max_content_light_level;
      }
      if (cta_861_3.max_frame_average_light_level > 0) {
        fall = cta_861_3.max_frame_average_light_level;
      }
    }
    wp_image_description_creator_params_v1_set_max_cll(creator, cll);
    wp_image_description_creator_params_v1_set_max_fall(creator, fall);
  }

  return true;
}

wl::Object<wp_color_management_output_v1>
WaylandWpColorManager::CreateColorManagementOutput(wl_output* output) {
  return wl::Object<wp_color_management_output_v1>(
      wp_color_manager_v1_get_output(manager_.get(), output));
}

wl::Object<wp_color_management_surface_v1>
WaylandWpColorManager::CreateColorManagementSurface(wl_surface* surface) {
  return wl::Object<wp_color_management_surface_v1>(
      wp_color_manager_v1_get_surface(manager_.get(), surface));
}

wl::Object<wp_color_management_surface_feedback_v1>
WaylandWpColorManager::CreateColorManagementFeedbackSurface(
    wl_surface* surface) {
  return wl::Object<wp_color_management_surface_feedback_v1>(
      wp_color_manager_v1_get_surface_feedback(manager_.get(), surface));
}

void WaylandWpColorManager::OnHdrEnabledChanged(bool hdr_enabled) {
  hdr_enabled_ = hdr_enabled;
  for (auto& observer : observers_) {
    observer.OnHdrEnabledChanged(hdr_enabled);
  }
}

void WaylandWpColorManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WaylandWpColorManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool WaylandWpColorManager::IsSupportedRenderIntent(
    wp_color_manager_v1_render_intent intent) const {
  return supported_intents_ & (1 << intent);
}

bool WaylandWpColorManager::IsSupportedFeature(
    wp_color_manager_v1_feature feature) const {
  return supported_features_ & (1 << feature);
}

bool WaylandWpColorManager::IsSupportedPrimaries(
    wp_color_manager_v1_primaries primaries) const {
  return supported_primaries_ & (1 << primaries);
}

bool WaylandWpColorManager::IsSupportedTransferFunction(
    wp_color_manager_v1_transfer_function transfer_function) const {
  return supported_transfers_ & (1 << transfer_function);
}

// static
void WaylandWpColorManager::OnSupportedIntent(void* data,
                                              wp_color_manager_v1* manager,
                                              uint32_t render_intent) {
  auto* self = static_cast<WaylandWpColorManager*>(data);
  self->supported_intents_ |= 1 << render_intent;
}

// static
void WaylandWpColorManager::OnSupportedFeature(void* data,
                                               wp_color_manager_v1* manager,
                                               uint32_t feature) {
  auto* self = static_cast<WaylandWpColorManager*>(data);
  self->supported_features_ |= 1 << feature;
}

// static
void WaylandWpColorManager::OnSupportedTfNamed(void* data,
                                               wp_color_manager_v1* manager,
                                               uint32_t tf) {
  auto* self = static_cast<WaylandWpColorManager*>(data);
  self->supported_transfers_ |= 1 << tf;
}

// static
void WaylandWpColorManager::OnSupportedPrimariesNamed(
    void* data,
    wp_color_manager_v1* manager,
    uint32_t primaries) {
  auto* self = static_cast<WaylandWpColorManager*>(data);
  self->supported_primaries_ |= 1 << primaries;
}

// static
void WaylandWpColorManager::OnDone(void* data, wp_color_manager_v1* manager) {
  auto* self = static_cast<WaylandWpColorManager*>(data);
  self->ready_ = true;
  if (auto* output_manager = self->connection_->wayland_output_manager()) {
    output_manager->InitializeAllWpColorManagementOutputs();
  }
}

}  // namespace ui
