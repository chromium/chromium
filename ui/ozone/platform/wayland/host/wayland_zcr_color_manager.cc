// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_zcr_color_manager.h"

#include <chrome-color-management-client-protocol.h>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "ui/base/wayland/color_manager_util.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_color_space_creator.h"
#include "ui/ozone/platform/wayland/wayland_utils.h"

namespace ui {

namespace {
constexpr uint32_t kMinVersion = 1;
constexpr uint32_t kMaxVersion = 6;
}  // namespace

// static
constexpr char WaylandZcrColorManager::kInterfaceName[];

// static
void WaylandZcrColorManager::Instantiate(WaylandConnection* connection,
                                         wl_registry* registry,
                                         uint32_t name,
                                         const std::string& interface,
                                         uint32_t version) {
  CHECK_EQ(interface, kInterfaceName) << "Expected \"" << kInterfaceName
                                      << "\" but got \"" << interface << "\"";
  if (connection->zcr_color_manager_ ||
      !wl::CanBind(interface, version, kMinVersion, kMaxVersion)) {
    return;
  }
  auto color_manager = wl::Bind<struct zcr_color_manager_v1>(
      registry, name, std::min(version, kMaxVersion));
  if (!color_manager) {
    LOG(ERROR) << "Failed to bind zcr_color_manager_v1";
    return;
  }
  connection->zcr_color_manager_ = std::make_unique<WaylandZcrColorManager>(
      color_manager.release(), connection);
  if (connection->wayland_output_manager())
    connection->wayland_output_manager()->InitializeAllColorManagementOutputs();

  connection->zcr_color_manager_->version_ = std::min(version, kMaxVersion);
  connection->zcr_color_manager_->PreloadCommonColorSpaces();
}

// Calling this function during Instantiate creates a copy of these colorspaces
// ahead of time on the server so they're ready when first requested.
// These are common video colorspaces you might come across browsing the web:
// Youtube, meets calls, hdr video, etc.
// Eventually the ZcrColorManager protocol needs to be extended to support
// sending colorspaces immediately (b/280388004).
void WaylandZcrColorManager::PreloadCommonColorSpaces() {
  auto common_colorspaces = {
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::PQ,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::LIMITED),
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::HLG,
                      gfx::ColorSpace::MatrixID::BT2020_NCL,
                      gfx::ColorSpace::RangeID::LIMITED),
      gfx::ColorSpace::CreateJpeg(),
      gfx::ColorSpace::CreateSRGB(),
      gfx::ColorSpace::CreateREC601(),
      gfx::ColorSpace::CreateREC709(),
      gfx::ColorSpace::CreateDisplayP3D65(),
      gfx::ColorSpace::CreateExtendedSRGB10Bit()};

  for (auto& color_space : common_colorspaces) {
    GetColorSpace(color_space);
  }
}

WaylandZcrColorManager::WaylandZcrColorManager(
    zcr_color_manager_v1* zcr_color_manager,
    WaylandConnection* connection)
    : zcr_color_manager_(zcr_color_manager), connection_(connection) {}

WaylandZcrColorManager::~WaylandZcrColorManager() = default;

void WaylandZcrColorManager::OnColorSpaceCreated(
    gfx::ColorSpace color_space,
    scoped_refptr<WaylandZcrColorSpace> zcr_color_space,
    std::optional<uint32_t> error) {
  if (error.has_value()) {
    // TODO(mrfemi): Store in a creation failed map.
    LOG(ERROR) << "Failed to create WaylandZcrColorSpace";
    return;
  }

  saved_color_spaces_.Put(color_space, std::move(zcr_color_space));
  pending_color_spaces_.erase(color_space);
}

wl::Object<zcr_color_space_creator_v1>
WaylandZcrColorManager::CreateZcrColorSpaceCreator(
    const gfx::ColorSpace& color_space) {
  auto eotf = wayland::ToColorManagerEOTF(
      color_space, zcr_color_manager_v1_get_version(zcr_color_manager_.get()));
  if (eotf == ZCR_COLOR_MANAGER_V1_EOTF_NAMES_UNKNOWN) {
    LOG(WARNING) << "Attempt to create color space from"
                 << " unsupported or invalid TransferID: "
                 << color_space.ToString() << ".";
    eotf = ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709;
  }
  auto matrix = wayland::ToColorManagerMatrix(
      color_space.GetMatrixID(),
      zcr_color_manager_v1_get_version(zcr_color_manager_.get()));
  if (matrix == ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_UNKNOWN) {
    LOG(WARNING) << "Attempt to create color space from"
                 << " unsupported or invalid MatrixID: "
                 << color_space.ToString();
    matrix = ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_RGB;
  }
  auto range = wayland::ToColorManagerRange(
      color_space.GetRangeID(),
      zcr_color_manager_v1_get_version(zcr_color_manager_.get()));
  if (range == ZCR_COLOR_MANAGER_V1_RANGE_NAMES_UNKNOWN) {
    LOG(WARNING) << "Attempt to create color space from"
                 << " unsupported or invalid RangeID: "
                 << color_space.ToString();
    range = ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL;
  }
  auto chromaticity = wayland::ToColorManagerChromaticity(
      color_space.GetPrimaryID(),
      zcr_color_manager_v1_get_version(zcr_color_manager_.get()));
  if (chromaticity != ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_UNKNOWN) {
    if (zcr_color_manager_v1_get_version(zcr_color_manager_.get()) <
        ZCR_COLOR_SPACE_V1_COMPLETE_NAMES_SINCE_VERSION) {
      return wl::Object<zcr_color_space_creator_v1>(
          zcr_color_manager_v1_create_color_space_from_names(
              zcr_color_manager_.get(), eotf, chromaticity,
              ZCR_COLOR_MANAGER_V1_WHITEPOINT_NAMES_D65));
    }
    return wl::Object<zcr_color_space_creator_v1>(
        zcr_color_manager_v1_create_color_space_from_complete_names(
            zcr_color_manager_.get(), eotf, chromaticity,
            ZCR_COLOR_MANAGER_V1_WHITEPOINT_NAMES_D65, matrix, range));
  }
  auto primaries = color_space.GetPrimaries();
  if (zcr_color_manager_v1_get_version(zcr_color_manager_.get()) <
      ZCR_COLOR_SPACE_V1_COMPLETE_PARAMS_SINCE_VERSION) {
    return wl::Object<zcr_color_space_creator_v1>(
        zcr_color_manager_v1_create_color_space_from_params(
            zcr_color_manager_.get(), eotf, FLOAT_TO_PARAM(primaries.fRX),
            FLOAT_TO_PARAM(primaries.fRY), FLOAT_TO_PARAM(primaries.fGX),
            FLOAT_TO_PARAM(primaries.fGY), FLOAT_TO_PARAM(primaries.fBX),
            FLOAT_TO_PARAM(primaries.fBY), FLOAT_TO_PARAM(primaries.fWX),
            FLOAT_TO_PARAM(primaries.fWY)));
  }
  return wl::Object<zcr_color_space_creator_v1>(
      zcr_color_manager_v1_create_color_space_from_complete_params(
          zcr_color_manager_.get(), eotf, matrix, range,
          FLOAT_TO_PARAM(primaries.fRX), FLOAT_TO_PARAM(primaries.fRY),
          FLOAT_TO_PARAM(primaries.fGX), FLOAT_TO_PARAM(primaries.fGY),
          FLOAT_TO_PARAM(primaries.fBX), FLOAT_TO_PARAM(primaries.fBY),
          FLOAT_TO_PARAM(primaries.fWX), FLOAT_TO_PARAM(primaries.fWY)));
}

scoped_refptr<WaylandZcrColorSpace> WaylandZcrColorManager::GetColorSpace(
    const gfx::ColorSpace& color_space) {
  auto it = saved_color_spaces_.Get(color_space);
  if (it != saved_color_spaces_.end()) {
    return it->second;
  }
  if (pending_color_spaces_.count(color_space) != 0)
    return nullptr;

  pending_color_spaces_[color_space] =
      std::make_unique<WaylandZcrColorSpaceCreator>(
          CreateZcrColorSpaceCreator(color_space),
          base::BindOnce(&WaylandZcrColorManager::OnColorSpaceCreated,
                         base::Unretained(this), color_space));
  return nullptr;
}

wl::Object<zcr_color_management_output_v1>
WaylandZcrColorManager::CreateColorManagementOutput(wl_output* output) {
  return wl::Object<zcr_color_management_output_v1>(
      zcr_color_manager_v1_get_color_management_output(zcr_color_manager_.get(),
                                                       output));
}

wl::Object<zcr_color_management_surface_v1>
WaylandZcrColorManager::CreateColorManagementSurface(wl_surface* surface) {
  return wl::Object<zcr_color_management_surface_v1>(
      zcr_color_manager_v1_get_color_management_surface(
          zcr_color_manager_.get(), surface));
}

}  // namespace ui
