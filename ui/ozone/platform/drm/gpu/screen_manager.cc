// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/screen_manager.h"

#include <xf86drmMode.h>
#include <memory>
#include <utility>

#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/linux/gbm_buffer.h"
#include "ui/gfx/skia_util.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
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
    std::unique_ptr<drmModeModeInfo> pmode)
    : display_id(display_id),
      drm(drm),
      crtc(crtc),
      connector(connector),
      origin(origin),
      mode(std::move(pmode)) {}

ScreenManager::ControllerConfigParams::ControllerConfigParams(
    const ControllerConfigParams& other)
    : display_id(other.display_id),
      drm(other.drm),
      crtc(other.crtc),
      connector(other.connector),
      origin(other.origin) {
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
      origin(other.origin) {
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
    LOG(WARNING) << "Display controller (crtc=" << crtc << ") already present.";
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
    CrtcsWithDrmList controllers_to_remove = controllers_on_drm.second;

    CommitRequest commit_request;
    auto drm = controllers_on_drm.first;
    for (const auto& controller : controllers_to_remove) {
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
    const ControllerConfigsList& controllers_params) {
  TRACE_EVENT0("drm", "ScreenManager::ConfigureDisplayControllers");

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

  bool config_success = true;
  // Perform display configurations together for the same DRM only.
  for (const auto& configs_on_drm : displays_for_drm_devices) {
    const ControllerConfigsList& controllers_params = configs_on_drm.second;
    config_success &=
        TestModeset(controllers_params) && Modeset(controllers_params);
  }

  if (config_success)
    UpdateControllerToWindowMapping();

  return config_success;
}

bool ScreenManager::TestModeset(
    const ControllerConfigsList& controllers_params) {
  return TestAndSetPreferredModifiers(controllers_params) ||
         TestAndSetLinearModifier(controllers_params);
}

bool ScreenManager::TestAndSetPreferredModifiers(
    const ControllerConfigsList& controllers_params) {
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
      uint32_t fourcc_format = ui::GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetFormatModifiersForTestModeset(fourcc_format);

      DrmOverlayPlane primary_plane = GetModesetBuffer(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*is_testing=*/true);
      if (!primary_plane.buffer) {
        return false;
      }

      crtcs_preferred_modifier[params.crtc] = std::make_pair(
          modifiers.empty(), primary_plane.buffer->format_modifier());

      GetModesetControllerProps(&commit_request, controller, params.origin,
                                *params.mode, primary_plane);
    } else {
      controller->GetDisableProps(&commit_request);
    }
  }

  if (!drm->plane_manager()->Commit(
          std::move(commit_request),
          DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET)) {
    return false;
  }

  SetPreferredModifiers(controllers_params, crtcs_preferred_modifier);
  return true;
}

bool ScreenManager::TestAndSetLinearModifier(
    const ControllerConfigsList& controllers_params) {
  TRACE_EVENT1("drm", "ScreenManager::TestAndSetLinearModifier",
               "display_count", controllers_params.size());

  CrtcPreferredModifierMap crtcs_preferred_modifier;
  CommitRequest commit_request;
  auto drm = controllers_params[0].drm;

  for (const auto& params : controllers_params) {
    auto it = FindDisplayController(params.drm, params.crtc);
    DCHECK(controllers_.end() != it);
    HardwareDisplayController* controller = it->get();

    uint32_t fourcc_format = ui::GetFourCCFormatForOpaqueFramebuffer(
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
      DrmOverlayPlane primary_plane = GetModesetBuffer(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*is_testing=*/true);
      if (!primary_plane.buffer)
        return false;

      GetModesetControllerProps(&commit_request, controller, params.origin,
                                *params.mode, primary_plane);
    } else {
      controller->GetDisableProps(&commit_request);
    }
  }

  if (!drm->plane_manager()->Commit(
          std::move(commit_request),
          DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET)) {
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
        it->get()->UpdatePreferredModiferForFormat(
            display::DisplaySnapshot::PrimaryFormat(), picked_modifier);
      }
    }
  }
}

bool ScreenManager::Modeset(const ControllerConfigsList& controllers_params) {
  TRACE_EVENT1("drm", "ScreenManager::Modeset", "display_count",
               controllers_params.size());

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
          controller->GetSupportedModifiers(fourcc_format);
      DrmOverlayPlane primary_plane = GetModesetBuffer(
          controller, gfx::Rect(params.origin, ModeSize(*params.mode)),
          modifiers, /*is_testing=*/false);
      if (!primary_plane.buffer)
        return false;

      SetDisplayControllerForEnableAndGetProps(
          &commit_request, params.drm, params.crtc, params.connector,
          params.origin, *params.mode, primary_plane);

    } else {
      bool disable_set = SetDisableDisplayControllerForDisableAndGetProps(
          &commit_request, params.drm, params.crtc);
      if (!disable_set)
        return false;
    }
  }

  bool commit_status = drm->plane_manager()->Commit(
      commit_request, DRM_MODE_ATOMIC_ALLOW_MODESET);

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
    const DrmOverlayPlane& primary) {
  HardwareDisplayControllers::iterator it = FindDisplayController(drm, crtc);
  DCHECK(controllers_.end() != it)
      << "Display controller (crtc=" << crtc << ") doesn't exist.";

  HardwareDisplayController* controller = it->get();
  CrtcController* crtc_controller = GetCrtcController(controller, drm, crtc);
  // If nothing changed just enable the controller. Note, we perform an exact
  // comparison on the mode since the refresh rate may have changed.
  if (SameMode(mode, crtc_controller->mode()) &&
      origin == controller->origin()) {
    if (!controller->IsEnabled()) {
      // Even if there is a mirrored display, Modeset the CRTC with its mode in
      // the original controller so that only this CRTC is affected by the mode.
      // Otherwise it could apply a mode with the same resolution and refresh
      // rate but with different timings to the other CRTC.
      GetModesetControllerProps(commit_request, controller,
                                controller->origin(), mode, primary);
    } else {
      // Just get props to re-enable the controller re-using the current state.
      GetEnableControllerProps(commit_request, controller, primary);
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

  GetModesetControllerProps(commit_request, controller, origin, mode, primary);
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
    bool was_enabled = (crtc_request.should_enable());

    HardwareDisplayControllers::iterator it =
        FindDisplayController(drm, crtc_request.crtc_id());
    if (it != controllers_.end()) {
      it->get()->UpdateState(was_enabled, DrmOverlayPlane::GetPrimaryPlane(
                                              crtc_request.overlays()));

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
      uint32_t fourcc_format = ui::GetFourCCFormatForOpaqueFramebuffer(
          display::DisplaySnapshot::PrimaryFormat());
      std::vector<uint64_t> modifiers =
          controller->GetSupportedModifiers(fourcc_format);
      DrmOverlayPlane primary_plane = GetModesetBuffer(
          controller,
          gfx::Rect(controller->origin(), controller->GetModeSize()), modifiers,
          /*is_testing=*/false);
      DCHECK(primary_plane.buffer);

      CommitRequest commit_request;
      GetEnableControllerProps(&commit_request, controller, primary_plane);
      controller->GetDrmDevice()->plane_manager()->Commit(
          std::move(commit_request), DRM_MODE_ATOMIC_ALLOW_MODESET);
    }
  }
}

DrmOverlayPlane ScreenManager::GetModesetBuffer(
    HardwareDisplayController* controller,
    const gfx::Rect& bounds,
    const std::vector<uint64_t>& modifiers,
    bool is_testing) {
  scoped_refptr<DrmDevice> drm = controller->GetDrmDevice();
  uint32_t fourcc_format = ui::GetFourCCFormatForOpaqueFramebuffer(
      display::DisplaySnapshot::PrimaryFormat());
  // Get the buffer that best reflects what the next Page Flip will look like,
  // which is using the preferred modifiers from the controllers.
  std::unique_ptr<GbmBuffer> buffer =
      drm->gbm_device()->CreateBufferWithModifiers(
          fourcc_format, bounds.size(), GBM_BO_USE_SCANOUT, modifiers);
  if (!buffer) {
    LOG(ERROR) << "Failed to create scanout buffer";
    return DrmOverlayPlane::Error();
  }

  // If the current primary plane matches what we need for the next page flip,
  // we can clone it.
  DrmWindow* window = FindWindowAt(bounds);
  if (window) {
    const DrmOverlayPlane* primary = window->GetLastModesetBuffer();
    const DrmDevice* drm = controller->GetDrmDevice().get();
    if (primary && primary->buffer->size() == bounds.size() &&
        primary->buffer->drm_device() == drm) {
      if (primary->buffer->format_modifier() == buffer->GetFormatModifier())
        return primary->Clone();
    }
  }

  scoped_refptr<DrmFramebuffer> framebuffer = DrmFramebuffer::AddFramebuffer(
      drm, buffer.get(), buffer->GetSize(), modifiers);
  if (!framebuffer) {
    LOG(ERROR) << "Failed to add framebuffer for scanout buffer";
    return DrmOverlayPlane::Error();
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
  return DrmOverlayPlane(framebuffer, nullptr);
}

void ScreenManager::GetEnableControllerProps(
    CommitRequest* commit_request,
    HardwareDisplayController* controller,
    const DrmOverlayPlane& primary) {
  DCHECK(!controller->crtc_controllers().empty());

  controller->GetEnableProps(commit_request, primary);
}

void ScreenManager::GetModesetControllerProps(
    CommitRequest* commit_request,
    HardwareDisplayController* controller,
    const gfx::Point& origin,
    const drmModeModeInfo& mode,
    const DrmOverlayPlane& primary) {
  DCHECK(!controller->crtc_controllers().empty());

  controller->set_origin(origin);
  controller->GetModesetProps(commit_request, primary, mode);
}

DrmWindow* ScreenManager::FindWindowAt(const gfx::Rect& bounds) const {
  for (auto& pair : window_map_) {
    if (pair.second->bounds() == bounds)
      return pair.second.get();
  }

  return nullptr;
}

}  // namespace ui
