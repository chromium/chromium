// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/screen_manager.h"

#include <xf86drmMode.h>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

namespace {

// Copies the contents of the saved framebuffer from the CRTCs in |controller|
// to the surface for the new modeset buffer |surface|.
bool FillModesetBuffer(const scoped_refptr<DrmDevice>& drm,
                       HardwareDisplayController* controller,
                       SkSurface* surface,
                       const std::vector<uint64_t>& modifiers) {
  DCHECK(!controller->crtc_controllers().empty());
  CrtcController* first_crtc = controller->crtc_controllers()[0].get();
  ScopedDrmCrtcPtr saved_crtc(drm->GetCrtc(first_crtc->crtc()));
  if (!saved_crtc || !saved_crtc->buffer_id) {
    VLOG(2) << "Crtc has no saved state or wasn't modeset";
    return false;
  }

  for (const uint64_t modifier : modifiers) {
    // A value of 0 means DRM_FORMAT_MOD_NONE. If the CRTC has any other
    // modifier (tiling, compression, etc.) we can't read the fb and assume it's
    // a linear buffer.
    if (modifier) {
      VLOG(2) << "Crtc has a modifier and we might not know how to interpret "
                 "the fb.";
      return false;
    }
  }

  // If the display controller is in mirror mode, the CRTCs should be sharing
  // the same framebuffer.
  DrmDumbBuffer saved_buffer(drm);
  if (!saved_buffer.InitializeFromFramebuffer(saved_crtc->buffer_id)) {
    VLOG(2) << "Failed to grab saved framebuffer " << saved_crtc->buffer_id;
    return false;
  }

  // Don't copy anything if the sizes mismatch. This can happen when the user
  // changes modes.
  if (saved_buffer.GetCanvas()->getBaseLayerSize() !=
      surface->getCanvas()->getBaseLayerSize()) {
    VLOG(2) << "Previous buffer has a different size than modeset buffer";
    return false;
  }

  SkPaint paint;
  // Copy the source buffer. Do not perform any blending.
  paint.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawImage(saved_buffer.surface()->makeImageSnapshot(),
                                  0, 0, SkSamplingOptions(), &paint);
  return true;
}

CrtcController* GetCrtcController(HardwareDisplayController* controller,
                                  const scoped_refptr<DrmDevice>& drm,
                                  uint32_t crtc) {
  for (const auto& crtc_controller : controller->crtc_controllers()) {
    if (crtc_controller->crtc() == crtc)
      return crtc_controller.get();
  }

  NOTREACHED();
  return nullptr;
}

void ParamsToTracedValue(
    perfetto::TracedValue context,
    const ScreenManager::ControllerConfigsList& controllers_params,
    uint32_t modeset_flag) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("modeset_flag", modeset_flag);

  auto array = dict.AddArray("param");
  for (const auto& param : controllers_params) {
    auto param_dict = array.AppendDictionary();
    param_dict.Add("display_id", param.display_id);
    param_dict.Add("crtc", param.crtc);
    param_dict.Add("connector", param.connector);
    param_dict.Add("origin", param.origin.ToString());
    param_dict.Add("enable_vrr", param.enable_vrr);

    {
      auto drm_dict = param_dict.AddItem("drm");
      if (param.drm)
        param.drm->WriteIntoTrace(std::move(drm_dict).WriteDictionary());
    }

    {
      auto mode_dict = param_dict.AddItem("mode");
      if (param.mode)
        DrmWriteIntoTraceHelper(*param.mode, std::move(mode_dict));
    }
  }
}

// Returns a JSON-format log for a DRM configuration request represented by
// `controllers_params`. Note that this function assumes that all controllers in
// `controllers_params` are a part of the same DRM device.
std::string GenerateConfigurationLogForController(
    const ScreenManager::ControllerConfigsList& controllers_params) {
  DCHECK(!controllers_params.empty());

  base::flat_map<uint64_t, std::string> base_connectors_to_keys;
  base::Value::Dict drm_device;
  std::string base_connector_key;
  for (const auto& param : controllers_params) {
    const int64_t next_base_connector = param.base_connector_id;
    const auto& it = base_connectors_to_keys.find(next_base_connector);
    if (it == base_connectors_to_keys.end()) {
      base_connector_key = base::StrCat(
          {"base_connector=", base::NumberToString(next_base_connector)});
      drm_device.Set(base_connector_key, base::Value::List());

      base_connectors_to_keys.insert(
          std::make_pair(next_base_connector, base_connector_key));
    } else {
      base_connector_key = it->second;
    }

    std::string mode;
    if (param.mode) {
      const std::string size = ModeSize(*(param.mode.get())).ToString();
      const std::string refresh_rate =
          base::NumberToString(param.mode->vrefresh);
      mode = base::StrCat({size, "@", refresh_rate});
    } else {
      mode = "Disabled";
    }

    const std::string display =
        base::StrCat({"{connector=", base::NumberToString(param.connector), " ",
                      "crtc=", base::NumberToString(param.crtc), " mode=", mode,
                      "+(", param.origin.ToString(), ")}"});
    drm_device.FindList(base_connector_key)->Append(display);
  }
  const std::string device_name =
      controllers_params.back().drm->device_path().BaseName().value();
  base::Value::Dict drm_config;
  drm_config.Set(device_name, std::move(drm_device));
  std::string drm_config_log;
  const int json_writer_options = IsPrettyPrintDrmModesetConfigLogsEnabled()
                                      ? base::JSONWriter::OPTIONS_PRETTY_PRINT
                                      : 0;
  bool json_status = base::JSONWriter::WriteWithOptions(
      drm_config, json_writer_options, &drm_config_log);
  DCHECK(json_status);
  DCHECK(!drm_config_log.empty());
  // Remove trailing newline
  if (json_writer_options)
    drm_config_log.pop_back();
  return drm_config_log;
}

}  // namespace

ScreenManager::ScreenManager() = default;

ScreenManager::~ScreenManager() {
  DCHECK(window_map_.empty());
}

ScreenManager::ControllerConfigParams::ControllerConfigParams(
    int64_t display_id,
    scoped_refptr<DrmDevice> drm,
    uint32_t crtc,
    uint32_t connector,
    gfx::Point origin,
    std::unique_ptr<drmModeModeInfo> pmode,
    bool enable_vrr,
    uint64_t base_connector)
    : display_id(display_id),
      drm(drm),
      crtc(crtc),
      connector(connector),
      base_connector_id(base_connector ? base_connector
                                       : static_cast<uint64_t>(connector)),
      origin(origin),
      mode(std::move(pmode)),
      enable_vrr(enable_vrr) {}

ScreenManager::ControllerConfigParams::ControllerConfigParams(
    const ControllerConfigParams& other)
    : display_id(other.display_id),
      drm(other.drm),
      crtc(other.crtc),
      connector(other.connector),
      base_connector_id(other.base_connector_id),
      origin(other.origin),
      enable_vrr(other.enable_vrr) {
  if (other.mode) {
    drmModeModeInfo mode_obj = *other.mode.get();
    mode = std::make_unique<drmModeModeInfo>(mode_obj);
  }
}

ScreenManager::ControllerConfigParams::ControllerConfigParams(
    ControllerConfigParams&& other)
    : display_id(other.display_id),
      drm(other.drm),
      crtc(other.crtc),
      connector(other.connector),
      base_connector_id(other.base_connector_id),
      origin(other.origin),
      enable_vrr(other.enable_vrr) {
  if (other.mode) {
    drmModeModeInfo mode_obj = *other.mode.get();
    mode = std::make_unique<drmModeModeInfo>(mode_obj);
  }
}

ScreenManager::ControllerConfigParams::~ControllerConfigParams() = default;

void ScreenManager::AddDisplayController(const scoped_refptr<DrmDevice>& drm,
                                         uint32_t crtc,
                                         uint32_t connector) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  // TODO(dnicoara): Turn this into a DCHECK when async display configuration is
  // properly supported. (When there can't be a race between forcing initial
  // display configuration in ScreenManager and display::NativeDisplayDelegate
  // creating the display controllers.)
  if (it != controllers_.end()) {
    VLOG(2) << "Display controller (crtc=" << crtc << ") already present.";
    return;
  }

  controllers_.push_back(std::make_unique<HardwareDisplayController>(
      std::make_unique<CrtcController>(drm, crtc, connector), gfx::Point()));
}

void ScreenManager::RemoveDisplayControllers(
    const CrtcsWithDrmList& controllers_to_remove) {
  TRACE_EVENT1("drm", "ScreenManager::RemoveDisplayControllers",
               "display_count", controllers_to_remove.size());

  // Split them to different lists unique to each DRM Device.
  base::flat_map<scoped_refptr<DrmDevice>, CrtcsWithDrmList>
      controllers_for_drm_devices;
  for (const auto& controller : controllers_to_remove) {
    auto drm = controller.second;
    auto it = controllers_for_drm_devices.find(drm);
    if (it == controllers_for_drm_devices.end()) {
      controllers_for_drm_devices.insert(
          std::make_pair(drm, CrtcsWithDrmList()));
    }
    controllers_for_drm_devices[drm].emplace_back(controller);
  }

  bool should_update_controllers_to_window_mapping = false;
  for (const auto& controllers_on_drm : controllers_for_drm_devices) {
    CrtcsWithDrmList drm_controllers_to_remove = controllers_on_drm.second;

    CommitRequest commit_request;
    auto drm = controllers_on_drm.first;
    for (const auto& controller : drm_controllers_to_remove) {
      uint32_t crtc_id = controller.first;
      auto it = FindDisplayController(drm, crtc_id);
      if (it == controllers_.end())
        continue;

      bool is_mirrored = (*it)->IsMirrored();

      std::unique_ptr<CrtcController> crtc = (*it)->RemoveCrtc(drm, crtc_id);
      if (crtc->is_enabled()) {
        commit_request.push_back(CrtcCommitRequest::DisableCrtcRequest(
            crtc->crtc(), crtc->connector()));
      }

      if (!is_mirrored) {
        controllers_.erase(it);
        should_update_controllers_to_window_mapping = true;
      }
    }
    if (!commit_request.empty()) {
      drm->plane_manager()->Commit(std::move(commit_request),
                                   DRM_MODE_ATOMIC_ALLOW_MODESET);
    }
  }

  if (should_update_controllers_to_window_mapping)
    UpdateControllerToWindowMapping();
}

bool ScreenManager::ConfigureDisplayControllers(
    const ControllerConfigsList& controllers_params,
    uint32_t modeset_flag) {
  TRACE_EVENT_BEGIN2(
      "drm", "ScreenManager::ConfigureDisplayControllers", "params",
      ([modeset_flag,
        &controllers_params](perfetto::TracedValue context) -> void {
        ParamsToTracedValue(std::move(context), controllers_params,
                            modeset_flag);
      }),
      "before", this);

  // At least one of these flags must be set.
  DCHECK(modeset_flag & (display::kCommitModeset | display::kTestModeset));

  // Split them to different lists unique to each DRM Device.
  base::flat_map<scoped_refptr<DrmDevice>, ControllerConfigsList>
      displays_for_drm_devices;

  for (auto& params : controllers_params) {
    auto it = displays_for_drm_devices.find(params.drm);
    if (it == displays_for_drm_devices.end()) {
      displays_for_drm_devices.insert(
          std::make_pair(params.drm, ControllerConfigsList()));
    }
    displays_for_drm_devices[params.drm].emplace_back(params);
  }

  const bool commit_modeset = modeset_flag & display::kCommitModeset;
  const bool is_seamless_modeset = modeset_flag & display::kSeamlessModeset;
  bool config_success = true;
  // Perform display configurations together for the same DRM only.
  for (const auto& configs_on_drm : displays_for_drm_devices) {
    const ControllerConfigsList& drm_controllers_params = configs_on_drm.second;
    VLOG(1) << "DRM " << (commit_modeset ? "configuring: " : "testing: ")
            << GenerateConfigurationLogForController(drm_controllers_params);

    if (modeset_flag & display::kTestModeset) {
      bool test_modeset =
          TestAndSetPreferredModifiers(drm_controllers_params,
                                       is_seamless_modeset) ||
          TestAndSetLinearModifier(drm_controllers_params, is_seamless_modeset);
      config_success &= test_modeset;
      VLOG(1) << "Test-modeset " << (test_modeset ? "succeeded." : "failed.");
      if (!test_modeset)
        continue;
    }

    if (commit_modeset) {
      bool can_modeset_with_overlays =
          TestModesetWithOverlays(drm_controllers_params, is_seamless_modeset);
      bool modeset_commit_result =
          Modeset(drm_controllers_params, can_modeset_with_overlays,
                  is_seamless_modeset);
      config_success &= modeset_commit_result;
      if (modeset_commit_result) {
        VLOG(1) << "Modeset succeeded.";
      } else {
        LOG(ERROR) << "Modeset commit failed after a successful test-modeset.";
      }
    }
  }

  if (commit_modeset && config_success)
    UpdateControllerToWindowMapping();

  TRACE_EVENT_END2("drm", "ScreenManager::ConfigureDisplayControllers", "after",
                   this, "success", config_success);
  return config_success;
}

bool ScreenManager::TestAndSetPreferredModifiers(
    const ControllerConfigsList& controllers_params,
    bool is_seamless_modeset) {
  TRACE_EVENT1("drm", "ScreenManager::TestAndSetPreferredModifiers",
               "display_count", controllers_params.size());

  CrtcPreferredModifierMap crtcs_preferred_modifier;
  CommitRequest commit_request;
  auto drm = controllers_params[0].drm;

  for (const auto& params : controllers_params) {
    auto it = FindDisplayController(params.drm, params.crtc);
    DCHECK(controllers_.end() != it);
    HardwareDisplayController* controller = it->get();

    if (params.mode) {
      uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetFormatModifiersForTestModeset(fourcc_format);
      // Test with no overlays to go for a lower bandwidth usage.
      DrmOverlayPlaneList modeset_planes = GetModesetPlanes(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*include_overlays=*/false, /*is_testing=*/true);
      if (modeset_planes.empty())
        return false;

      uint64_t primary_modifier =
          DrmOverlayPlane::GetPrimaryPlane(modeset_planes)
              ->buffer->format_modifier();
      crtcs_preferred_modifier[params.crtc] =
          std::make_pair(modifiers.empty(), primary_modifier);

      GetModesetControllerProps(&commit_request, controller, params.origin,
                                *params.mode, modeset_planes,
                                params.enable_vrr);
    } else {
      controller->GetDisableProps(&commit_request);
    }
  }

  uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;
  if (!is_seamless_modeset)
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  if (!drm->plane_manager()->Commit(std::move(commit_request), flags)) {
    return false;
  }

  SetPreferredModifiers(controllers_params, crtcs_preferred_modifier);
  return true;
}

bool ScreenManager::TestAndSetLinearModifier(
    const ControllerConfigsList& controllers_params,
    bool is_seamless_modeset) {
  TRACE_EVENT1("drm", "ScreenManager::TestAndSetLinearModifier",
               "display_count", controllers_params.size());

  CrtcPreferredModifierMap crtcs_preferred_modifier;
  CommitRequest commit_request;
  auto drm = controllers_params[0].drm;

  for (const auto& params : controllers_params) {
    auto it = FindDisplayController(params.drm, params.crtc);
    DCHECK(controllers_.end() != it);
    HardwareDisplayController* controller = it->get();

    uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
        display::DisplaySnapshot::PrimaryFormat());
    std::vector<uint64_t> modifiers =
        controller->GetFormatModifiersForTestModeset(fourcc_format);
    // Test with an empty list if no preferred modifiers are advertised.
    // Platforms might not support gbm_bo_create_with_modifiers(). If the
    // platform doesn't expose modifiers, do not attempt to explicitly request
    // LINEAR otherwise we might CHECK() when trying to allocate buffers.
    if (!modifiers.empty())
      modifiers = std::vector<uint64_t>{DRM_FORMAT_MOD_LINEAR};
    crtcs_preferred_modifier[params.crtc] =
        std::make_pair(modifiers.empty(), DRM_FORMAT_MOD_LINEAR);

    if (params.mode) {
      // Test with no overlays to go for a lower bandwidth usage.
      DrmOverlayPlaneList modeset_planes = GetModesetPlanes(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*include_overlays=*/false, /*is_testing=*/true);
      if (modeset_planes.empty())
        return false;

      GetModesetControllerProps(&commit_request, controller, params.origin,
                                *params.mode, modeset_planes,
                                params.enable_vrr);
    } else {
      controller->GetDisableProps(&commit_request);
    }
  }

  uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;
  if (!is_seamless_modeset)
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  if (!drm->plane_manager()->Commit(std::move(commit_request), flags)) {
    return false;
  }

  SetPreferredModifiers(controllers_params, crtcs_preferred_modifier);
  return true;
}

void ScreenManager::SetPreferredModifiers(
    const ControllerConfigsList& controllers_params,
    const CrtcPreferredModifierMap& crtcs_preferred_modifier) {
  for (const auto& params : controllers_params) {
    if (params.mode) {
      bool was_modifiers_list_empty =
          crtcs_preferred_modifier.at(params.crtc).first;
      // No preferred modifiers should be saved as some platforms might not have
      // bo_create_with_modifiers implemented, this will send the preferred
      // modifiers list as an empty list.
      if (!was_modifiers_list_empty) {
        uint64_t picked_modifier =
            crtcs_preferred_modifier.at(params.crtc).second;
        auto it = FindDisplayController(params.drm, params.crtc);
        DCHECK(*it);
        it->get()->UpdatePreferredModifierForFormat(
            display::DisplaySnapshot::PrimaryFormat(), picked_modifier);
      }
    }
  }
}

bool ScreenManager::TestModesetWithOverlays(
    const ControllerConfigsList& controllers_params,
    bool is_seamless_modeset) {
  TRACE_EVENT1("drm", "ScreenManager::TestModesetWithOverlays", "display_count",
               controllers_params.size());

  bool does_an_overlay_exist = false;

  CommitRequest commit_request;
  auto drm = controllers_params[0].drm;
  for (const auto& params : controllers_params) {
    auto it = FindDisplayController(params.drm, params.crtc);
    DCHECK(controllers_.end() != it);
    HardwareDisplayController* controller = it->get();

    if (params.mode) {
      uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetSupportedModifiers(fourcc_format);

      DrmOverlayPlaneList modeset_planes = GetModesetPlanes(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*include_overlays=*/true, /*is_testing=*/true);
      DCHECK(!modeset_planes.empty());
      does_an_overlay_exist |= modeset_planes.size() > 1;

      GetModesetControllerProps(&commit_request, controller, params.origin,
                                *params.mode, modeset_planes,
                                params.enable_vrr);
    } else {
      controller->GetDisableProps(&commit_request);
    }
  }
  // If we have no overlays, report not modesetting with overlays as we haven't
  // tested with overlays.
  if (!does_an_overlay_exist)
    return false;

  uint32_t flags = DRM_MODE_ATOMIC_TEST_ONLY;
  if (!is_seamless_modeset)
    flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
  return drm->plane_manager()->Commit(std::move(commit_request), flags);
}

bool ScreenManager::Modeset(const ControllerConfigsList& controllers_params,
                            bool can_modeset_with_overlays,
                            bool is_seamless_modeset) {
  TRACE_EVENT2("drm", "ScreenManager::Modeset", "display_count",
               controllers_params.size(), "modeset_with_overlays",
               can_modeset_with_overlays);

  CommitRequest commit_request;
  auto drm = controllers_params[0].drm;

  for (const auto& params : controllers_params) {
    if (params.mode) {
      auto it = FindDisplayController(params.drm, params.crtc);
      DCHECK(controllers_.end() != it);
      HardwareDisplayController* controller = it->get();

      uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetSupportedModifiers(fourcc_format, /*is_modeset=*/true);

      gfx::Rect bounds = gfx::Rect(params.origin, ModeSize(*params.mode));
      DrmOverlayPlaneList modeset_planes =
          GetModesetPlanes(controller, bounds, modifiers,
                           can_modeset_with_overlays, /*is_testing=*/false);

      SetDisplayControllerForEnableAndGetProps(
          &commit_request, params.drm, params.crtc, params.connector,
          params.origin, *params.mode, modeset_planes, params.enable_vrr);

    } else {
      bool disable_set = SetDisableDisplayControllerForDisableAndGetProps(
          &commit_request, params.drm, params.crtc);
      if (!disable_set)
        return false;
    }
  }

  uint32_t flags = is_seamless_modeset ? 0 : DRM_MODE_ATOMIC_ALLOW_MODESET;
  bool commit_status = drm->plane_manager()->Commit(commit_request, flags);

  UpdateControllerStateAfterModeset(drm, commit_request, commit_status);

  return commit_status;
}

void ScreenManager::SetDisplayControllerForEnableAndGetProps(
    CommitRequest* commit_request,
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc,
    uint32_t connector,
    const gfx::Point& origin,
    const drmModeModeInfo& mode,
    const DrmOverlayPlaneList& modeset_planes,
    bool enable_vrr) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  DCHECK(controllers_.end() != it)
      << "Display controller (crtc=" << crtc << ") doesn't exist.";

  HardwareDisplayController* controller = it->get();
  CrtcController* crtc_controller = GetCrtcController(controller, drm, crtc);
  // If nothing changed just enable the controller. Note, we perform an exact
  // comparison on the mode since the refresh rate may have changed.
  if (SameMode(mode, crtc_controller->mode()) &&
      origin == controller->origin() &&
      enable_vrr == crtc_controller->vrr_enabled()) {
    if (!controller->IsEnabled()) {
      // Even if there is a mirrored display, Modeset the CRTC with its mode in
      // the original controller so that only this CRTC is affected by the mode.
      // Otherwise it could apply a mode with the same resolution and refresh
      // rate but with different timings to the other CRTC.
      GetModesetControllerProps(commit_request, controller,
                                controller->origin(), mode, modeset_planes,
                                enable_vrr);
    } else {
      // Just get props to re-enable the controller re-using the current state.
      GetEnableControllerProps(commit_request, controller, modeset_planes);
    }
    return;
  }

  // Either the mode or the location of the display changed, so exit mirror
  // mode and configure the display independently. If the caller still wants
  // mirror mode, subsequent calls configuring the other controllers will
  // restore mirror mode.
  if (controller->IsMirrored()) {
    controllers_.push_back(std::make_unique<HardwareDisplayController>(
        controller->RemoveCrtc(drm, crtc), controller->origin()));
    it = controllers_.end() - 1;
    controller = it->get();
  }

  GetModesetControllerProps(commit_request, controller, origin, mode,
                            modeset_planes, enable_vrr);
}

bool ScreenManager::SetDisableDisplayControllerForDisableAndGetProps(
    CommitRequest* commit_request,
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  if (it != controllers_.end()) {
    HardwareDisplayController* controller = it->get();
    if (controller->IsMirrored()) {
      controllers_.push_back(std::make_unique<HardwareDisplayController>(
          controller->RemoveCrtc(drm, crtc), controller->origin()));
      controller = controllers_.back().get();
    }

    controller->GetDisableProps(commit_request);
    return true;
  }

  LOG(ERROR) << "Failed to find display controller crtc=" << crtc;
  return false;
}

void ScreenManager::UpdateControllerStateAfterModeset(
    const scoped_refptr<DrmDevice>& drm,
    const CommitRequest& commit_request,
    bool did_succeed) {
  for (const CrtcCommitRequest& crtc_request : commit_request) {
    bool was_enabled = (crtc_request.should_enable_crtc());

    HardwareDisplayControllers::iterator it =
        FindDisplayController(drm, crtc_request.crtc_id());
    if (it != controllers_.end()) {
      it->get()->UpdateState(crtc_request);

      // If the CRTC is mirrored, move it to the mirror controller.
      if (did_succeed && was_enabled)
        HandleMirrorIfExists(drm, crtc_request, it);
    }
  }
}

void ScreenManager::HandleMirrorIfExists(
    const scoped_refptr<DrmDevice>& drm,
    const CrtcCommitRequest& crtc_request,
    const HardwareDisplayControllers::iterator& controller) {
  gfx::Rect modeset_bounds(crtc_request.origin(),
                           ModeSize(crtc_request.mode()));
  HardwareDisplayControllers::iterator mirror =
      FindActiveDisplayControllerByLocation(drm, modeset_bounds);
  // TODO(dnicoara): This is hacky, instead the DrmDisplay and
  // CrtcController should be merged and picking the mode should be done
  // properly within HardwareDisplayController.
  if (mirror != controllers_.end() && controller != mirror) {
    // TODO(markyacoub): RemoveCrtc makes a blocking commit to
    // DisableOverlayPlanes. This should be redesigned and included as part of
    // the Modeset commit.
    (*mirror)->AddCrtc((*controller)->RemoveCrtc(drm, crtc_request.crtc_id()));
    controllers_.erase(controller);
  }
}

HardwareDisplayController* ScreenManager::GetDisplayController(
    const gfx::Rect& bounds) {
  HardwareDisplayControllers::iterator it =
      FindActiveDisplayControllerByLocation(bounds);
  if (it != controllers_.end())
    return it->get();

  return nullptr;
}

void ScreenManager::AddWindow(gfx::AcceleratedWidget widget,
                              std::unique_ptr<DrmWindow> window) {
  std::pair<WidgetToWindowMap::iterator, bool> result =
      window_map_.emplace(widget, std::move(window));
  DCHECK(result.second) << "Window already added.";
  UpdateControllerToWindowMapping();
}

std::unique_ptr<DrmWindow> ScreenManager::RemoveWindow(
    gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window = std::move(window_map_[widget]);
  window_map_.erase(widget);
  DCHECK(window) << "Attempting to remove non-existing window for " << widget;
  UpdateControllerToWindowMapping();
  return window;
}

DrmWindow* ScreenManager::GetWindow(gfx::AcceleratedWidget widget) {
  WidgetToWindowMap::iterator it = window_map_.find(widget);
  if (it != window_map_.end())
    return it->second.get();

  return nullptr;
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindDisplayController(const scoped_refptr<DrmDevice>& drm,
                                     uint32_t crtc) {
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it) {
    if ((*it)->HasCrtc(drm, crtc))
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindActiveDisplayControllerByLocation(const gfx::Rect& bounds) {
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it) {
    gfx::Rect controller_bounds((*it)->origin(), (*it)->GetModeSize());
    if (controller_bounds == bounds && (*it)->IsEnabled())
      return it;
  }

  return controllers_.end();
}

ScreenManager::HardwareDisplayControllers::iterator
ScreenManager::FindActiveDisplayControllerByLocation(
    const scoped_refptr<DrmDevice>& drm,
    const gfx::Rect& bounds) {
  for (auto it = controllers_.begin(); it != controllers_.end(); ++it) {
    gfx::Rect controller_bounds((*it)->origin(), (*it)->GetModeSize());
    if ((*it)->GetDrmDevice() == drm && controller_bounds == bounds &&
        (*it)->IsEnabled())
      return it;
  }

  return controllers_.end();
}

void ScreenManager::UpdateControllerToWindowMapping() {
  std::map<DrmWindow*, HardwareDisplayController*> window_to_controller_map;
  // First create a unique mapping between a window and a controller. Note, a
  // controller may be associated with at most 1 window.
  for (const auto& controller : controllers_) {
    if (!controller->IsEnabled())
      continue;

    DrmWindow* window = FindWindowAt(
        gfx::Rect(controller->origin(), controller->GetModeSize()));
    if (!window)
      continue;

    window_to_controller_map[window] = controller.get();
  }

  // Apply the new mapping to all windows.
  for (auto& pair : window_map_) {
    auto it = window_to_controller_map.find(pair.second.get());
    HardwareDisplayController* controller = nullptr;
    if (it != window_to_controller_map.end())
      controller = it->second;

    bool should_enable = controller && pair.second->GetController() &&
                         pair.second->GetController() != controller;
    pair.second->SetController(controller);

    // If we're moving windows between controllers modeset the controller
    // otherwise the controller may be waiting for a page flip while the window
    // tries to schedule another buffer.
    if (should_enable) {
      uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetSupportedModifiers(fourcc_format);
      DrmOverlayPlaneList modeset_planes = GetModesetPlanes(
          controller,
          gfx::Rect(controller->origin(), controller->GetModeSize()), modifiers,
          /*include_overlays=*/true, /*is_testing=*/false);
      DCHECK(!modeset_planes.empty());

      CommitRequest commit_request;
      GetEnableControllerProps(&commit_request, controller, modeset_planes);
      controller->GetDrmDevice()->plane_manager()->Commit(
          std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET);
    }
  }
}

void ScreenManager::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("hardware_display_controllers", controllers_);

  {
    auto array = dict.AddArray("drm_devices");
    base::flat_set<base::FilePath> seen_devices;
    for (const auto& controller : controllers_) {
      if (seen_devices.contains(controller->GetDrmDevice()->device_path()))
        continue;

      seen_devices.insert(controller->GetDrmDevice()->device_path());
      array.Append(controller->GetDrmDevice());
    }
  }

  // TODO(jshargo): also trace WidgetToWindowMap window_map.
}

DrmOverlayPlaneList ScreenManager::GetModesetPlanes(
    HardwareDisplayController* controller,
    const gfx::Rect& bounds,
    const std::vector<uint64_t>& modifiers,
    bool include_overlays,
    bool is_testing) {
  scoped_refptr<DrmDevice> drm = controller->GetDrmDevice();
  uint32_t fourcc_format = GetFourCCFormatForOpaqueFramebuffer(
      display::DisplaySnapshot::PrimaryFormat());
  // Get the buffer that best reflects what the next Page Flip will look like,
  // which is using the preferred modifiers from the controllers.
  std::unique_ptr<GbmBuffer> buffer =
      drm->gbm_device()->CreateBufferWithModifiers(
          fourcc_format, bounds.size(), GBM_BO_USE_SCANOUT, modifiers);
  if (!buffer) {
    LOG(ERROR) << "Failed to create scanout buffer";
    return DrmOverlayPlaneList();
  }

  // If the current primary plane matches what we need for the next page flip,
  // clone all last_submitted_planes (matching primary + overlays).
  DrmWindow* window = FindWindowAt(bounds);
  if (window) {
    const DrmOverlayPlaneList& last_submitted_planes =
        window->last_submitted_planes();
    const DrmOverlayPlane* primary =
        DrmOverlayPlane::GetPrimaryPlane(last_submitted_planes);
    if (primary && primary->buffer->size() == bounds.size() &&
        primary->buffer->drm_device() == controller->GetDrmDevice().get() &&
        primary->buffer->format_modifier() == buffer->GetFormatModifier()) {
      if (include_overlays) {
        return DrmOverlayPlane::Clone(last_submitted_planes);
      } else {
        DrmOverlayPlaneList modeset_plane;
        modeset_plane.push_back(primary->Clone());
        return modeset_plane;
      }
    }
  }

  scoped_refptr<DrmFramebuffer> framebuffer = DrmFramebuffer::AddFramebuffer(
      drm, buffer.get(), buffer->GetSize(), modifiers);
  if (!framebuffer) {
    LOG(ERROR) << "Failed to add framebuffer for scanout buffer";
    return DrmOverlayPlaneList();
  }

  if (!is_testing) {
    sk_sp<SkSurface> surface = buffer->GetSurface();
    if (!surface) {
      VLOG(2) << "Can't get a SkSurface from the modeset gbm buffer.";
    } else if (!FillModesetBuffer(drm, controller, surface.get(), modifiers)) {
      // If we fail to fill the modeset buffer, clear it black to avoid
      // displaying an uninitialized framebuffer.
      surface->getCanvas()->clear(SK_ColorBLACK);
    }
  }

  DrmOverlayPlaneList modeset_planes;
  modeset_planes.emplace_back(framebuffer, nullptr);
  return modeset_planes;
}

void ScreenManager::GetEnableControllerProps(
    CommitRequest* commit_request,
    HardwareDisplayController* controller,
    const DrmOverlayPlaneList& modeset_planes) {
  DCHECK(!controller->crtc_controllers().empty());

  controller->GetEnableProps(commit_request, modeset_planes);
}

void ScreenManager::GetModesetControllerProps(
    CommitRequest* commit_request,
    HardwareDisplayController* controller,
    const gfx::Point& origin,
    const drmModeModeInfo& mode,
    const DrmOverlayPlaneList& modeset_planes,
    bool enable_vrr) {
  DCHECK(!controller->crtc_controllers().empty());

  controller->set_origin(origin);
  controller->GetModesetProps(commit_request, modeset_planes, mode, enable_vrr);
}

DrmWindow* ScreenManager::FindWindowAt(const gfx::Rect& bounds) const {
  for (auto& pair : window_map_) {
    if (pair.second->bounds() == bounds)
      return pair.second.get();
  }

  return nullptr;
}

}  // namespace ui
