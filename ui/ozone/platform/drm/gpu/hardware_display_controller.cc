// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

#include <drm.h>
#include <string.h>
#include <xf86drm.h>

#include <ios>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/tile_property.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"
#include "ui/ozone/platform/drm/gpu/page_flip_request.h"
#include "ui/ozone/platform/drm/gpu/page_flip_watchdog.h"

namespace ui {

namespace {

void CompletePageFlip(
    base::WeakPtr<HardwareDisplayController> hardware_display_controller_,
    int modeset_sequence,
    PresentationOnceCallback callback,
    DrmOverlayPlaneList plane_list,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (hardware_display_controller_) {
    hardware_display_controller_->OnPageFlipComplete(
        modeset_sequence, std::move(plane_list), presentation_feedback);
  }
  std::move(callback).Run(presentation_feedback);
}

void DrawCursor(DrmDumbBuffer* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  canvas->drawImageRect(image.asImage(), damage, SkSamplingOptions());
}

template <typename T>
std::string NumberToHexString(const T value) {
  static_assert(std::is_unsigned<T>::value,
                "Can only convert unsigned ints to hex");

  std::stringstream ss;
  ss << "0x" << std::hex << std::uppercase << value;
  return ss.str();
}

bool IsRockchipAfbc(uint64_t modifier) {
  return modifier ==
         DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_16x16 |
                                 AFBC_FORMAT_MOD_SPARSE | AFBC_FORMAT_MOD_YTR);
}

std::unique_ptr<DrmDumbBuffer> MakeCursorDrmBuffer(
    gfx::Size size,
    scoped_refptr<DrmDevice> drm_device) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  auto buffer = std::make_unique<DrmDumbBuffer>(drm_device);

  // Don't register a framebuffer for cursors since they are special (they
  // aren't modesetting buffers and drivers may fail to register them due to
  // their small sizes).
  if (!buffer->Initialize(info)) {
    LOG(FATAL) << "Failed to initialize cursor buffer";
  }
  return buffer;
}

}  // namespace

HardwareDisplayController::HardwareDisplayController(
    std::unique_ptr<CrtcController> controller,
    const gfx::Point& origin,
    raw_ptr<DrmModifiersFilter> drm_modifiers_filter)
    : origin_(origin),
      drm_modifiers_filter_(drm_modifiers_filter),
      tile_property_(controller->tile_property()) {
  AddCrtc(std::move(controller));
  InitSupportedCursorSizes();
  AllocateCursorBuffers();
}

HardwareDisplayController::~HardwareDisplayController() = default;

void HardwareDisplayController::GetModesetProps(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes,
    const drmModeModeInfo& mode,
    bool enable_vrr) {
  GetModesetPropsForCrtcs(commit_request, modeset_planes,
                          /*use_current_crtc_mode=*/false, mode, enable_vrr);
}

void HardwareDisplayController::GetEnableProps(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes) {
  // TODO(markyacoub): Simplify and remove the use of empty_mode.
  drmModeModeInfo empty_mode = {};
  GetModesetPropsForCrtcs(commit_request, modeset_planes,
                          /*use_current_crtc_mode=*/true, empty_mode,
                          /*enable_vrr=*/std::nullopt);
}

void HardwareDisplayController::GetModesetPropsForCrtcs(
    CommitRequest* commit_request,
    const DrmOverlayPlaneList& modeset_planes,
    bool use_current_crtc_mode,
    const drmModeModeInfo& mode,
    std::optional<bool> enable_vrr) {
  DCHECK(commit_request);

  GetDrmDevice()->plane_manager()->BeginFrame(&owned_hardware_planes_);

  for (const auto& controller : crtc_controllers_) {
    drmModeModeInfo modeset_mode =
        use_current_crtc_mode ? controller->mode() : mode;

    if (ShouldDisableNonprimaryTileController(*controller, modeset_mode,
                                              use_current_crtc_mode)) {
      if (controller->is_enabled()) {
        CrtcCommitRequest request = CrtcCommitRequest::DisableCrtcRequest(
            controller->crtc(), controller->connector());
        commit_request->push_back(std::move(request));
      }

      continue;
    }

    DrmOverlayPlaneList overlays = DrmOverlayPlane::Clone(modeset_planes);
    CrtcCommitRequest request = CrtcCommitRequest::EnableCrtcRequest(
        controller->crtc(), controller->connector(), modeset_mode, origin_,
        &owned_hardware_planes_, std::move(overlays),
        enable_vrr.value_or(controller->vrr_enabled()));
    commit_request->push_back(std::move(request));
  }
}

bool HardwareDisplayController::ShouldDisableNonprimaryTileController(
    const CrtcController& controller,
    const drmModeModeInfo& mode,
    const bool use_current_crtc_mode) const {
  const bool is_nonprimary_tile =
      IsTiled() &&
      // The |controller| is a non-primary tile.
      controller.tile_property()->location != tile_property_->location;
  if (!is_nonprimary_tile) {
    return false;
  }

  // For tiled displays, all non-primary tiles should be disabled if the
  // requested mode is not a tile mode.
  bool should_disable = !IsTileMode(ModeSize(mode), *tile_property_);

  // Handles disconnect - the primary tile mode is not the same as the
  // current mode of the nonprimary |controller|.
  const bool is_same_mode_as_primary_controller =
      SameMode(crtc_controllers_[0]->mode(), controller.mode());
  should_disable = should_disable || (use_current_crtc_mode &&
                                      !is_same_mode_as_primary_controller);

  return should_disable;
}

void HardwareDisplayController::GetDisableProps(CommitRequest* commit_request) {
  for (const auto& controller : crtc_controllers_) {
    CrtcCommitRequest request = CrtcCommitRequest::DisableCrtcRequest(
        controller->crtc(), controller->connector(), &owned_hardware_planes_);
    commit_request->push_back(std::move(request));
  }
}

void HardwareDisplayController::UpdateState(
    const CrtcCommitRequest& crtc_request) {
  watchdog_.Disarm();

  // Verify that the current state matches the requested state.
  if (crtc_request.should_enable_crtc() && IsEnabled()) {
    DCHECK(!crtc_request.overlays().empty());
    // TODO(markyacoub): This should be absorbed in the commit request.
    ResetCursor();
    OnModesetComplete(crtc_request.overlays());
  }
}

void HardwareDisplayController::SchedulePageFlip(
    DrmOverlayPlaneList plane_list,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  DCHECK(!page_flip_request_);
  TRACE_EVENT0("drm", "HDC::SchedulePageFlip");
  scoped_refptr<PageFlipRequest> page_flip_request =
      base::MakeRefCounted<PageFlipRequest>(GetRefreshInterval());
  gfx::GpuFenceHandle release_fence;

  PageFlipResult result =
      ScheduleOrTestPageFlip(plane_list, page_flip_request, &release_fence);
  UMA_HISTOGRAM_ENUMERATION(
      "Compositing.Display.HardwareDisplayController.SchedulePageFlipResult",
      result);

  if (PageFlipResult::kFailedPlaneAssignment == result) {
    watchdog_.CrashOnFailedPlaneAssignment();

    // Plane assignment is usually an intermittent problem that recovers itself
    // within a few frames. Send back a NAK and hope for the best--the
    // watchdog will handle things if this problem is persistent.
    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  } else if (PageFlipResult::kFailedCommit == result) {
    for (const auto& plane : plane_list) {
      // If the page flip failed and we see that the buffer has been allocated
      // before the latest modeset, it could mean it was an in-flight buffer
      // carrying an obsolete configuration.
      // Request a buffer reallocation to reflect the new change.
      if (plane.buffer &&
          plane.buffer->modeset_sequence_id_at_allocation() <
              plane.buffer->drm_device()->modeset_sequence_id()) {
        std::move(submission_callback)
            .Run(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS,
                 /*release_fence=*/gfx::GpuFenceHandle());
        std::move(presentation_callback)
            .Run(gfx::PresentationFeedback::Failure());
        return;
      }
    }

    // No outdated buffers detected which makes this a true page flip failure.
    // Alert the watchdog.
    watchdog_.ArmForFailedCommit();

    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_FAILED,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }
  if (page_flip_request->page_flip_count() == 0) {
    // Apparently, there was nothing to do. This probably should not be
    // able to happen but both CrtcController::AssignOverlayPlanes and
    // HardwareDisplayPlaneManagerLegacy::Commit appear to have cases
    // where we ACK without actually scheduling a page flip.
    std::move(submission_callback)
        .Run(gfx::SwapResult::SWAP_ACK,
             /*release_fence=*/gfx::GpuFenceHandle());
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  std::move(submission_callback)
      .Run(gfx::SwapResult::SWAP_ACK, std::move(release_fence));

  watchdog_.OnSuccessfulPageFlip();
  // Everything was submitted successfully, wait for asynchronous completion.
  page_flip_request->TakeCallback(
      base::BindOnce(&CompletePageFlip, weak_ptr_factory_.GetWeakPtr(),
                     GetDrmDevice()->modeset_sequence_id(),
                     std::move(presentation_callback), std::move(plane_list)));
  page_flip_request_ = std::move(page_flip_request);
}

bool HardwareDisplayController::TestPageFlip(
    const DrmOverlayPlaneList& plane_list) {
  TRACE_EVENT0("drm", "HDC::TestPageFlip");
  return PageFlipResult::kSuccess ==
         ScheduleOrTestPageFlip(plane_list, nullptr, nullptr);
}

HardwareDisplayController::PageFlipResult
HardwareDisplayController::ScheduleOrTestPageFlip(
    const DrmOverlayPlaneList& plane_list,
    scoped_refptr<PageFlipRequest> page_flip_request,
    gfx::GpuFenceHandle* release_fence) {
  TRACE_EVENT0("drm", "HDC::ScheduleOrTestPageFlip");
  DCHECK(IsEnabled());

  // Ignore requests with no planes to schedule.
  if (plane_list.empty())
    return PageFlipResult::kSuccess;

  DrmOverlayPlaneList pending_planes = DrmOverlayPlane::Clone(plane_list);
  std::sort(pending_planes.begin(), pending_planes.end(),
            [](const DrmOverlayPlane& l, const DrmOverlayPlane& r) {
              return l.z_order < r.z_order;
            });
  GetDrmDevice()->plane_manager()->BeginFrame(&owned_hardware_planes_);

  for (const auto& controller : crtc_controllers_) {
    if (!controller->is_enabled()) {
      continue;
    }

    if (!controller->AssignOverlayPlanes(
            &owned_hardware_planes_, pending_planes, /*is_modesetting=*/false))
      return PageFlipResult::kFailedPlaneAssignment;
  }

  bool commit_success = GetDrmDevice()->plane_manager()->Commit(
      &owned_hardware_planes_, page_flip_request, release_fence);

  return commit_success ? PageFlipResult::kSuccess
                        : PageFlipResult::kFailedCommit;
}

bool HardwareDisplayController::TestSeamlessMode(int32_t crtc_id,
                                                 const drmModeModeInfo& mode) {
  return GetDrmDevice()->plane_manager()->TestSeamlessMode(crtc_id, mode);
}

std::vector<uint64_t> HardwareDisplayController::GetFormatModifiers(
    uint32_t fourcc_format) const {
  if (crtc_controllers_.empty())
    return std::vector<uint64_t>();

  std::vector<uint64_t> modifiers =
      crtc_controllers_[0]->GetFormatModifiers(fourcc_format);

  if (drm_modifiers_filter_) {
    gfx::BufferFormat buffer_format =
        GetBufferFormatFromFourCCFormat(fourcc_format);
    modifiers = drm_modifiers_filter_->Filter(buffer_format, modifiers);
  }

  for (size_t i = 1; i < crtc_controllers_.size(); ++i) {
    std::vector<uint64_t> other =
        crtc_controllers_[i]->GetFormatModifiers(fourcc_format);
    std::vector<uint64_t> intersection;

    std::set_intersection(modifiers.begin(), modifiers.end(), other.begin(),
                          other.end(), std::back_inserter(intersection));
    modifiers = std::move(intersection);
  }

  return modifiers;
}

std::vector<uint64_t> HardwareDisplayController::GetSupportedModifiers(
    uint32_t fourcc_format,
    bool is_modeset) const {
  if (preferred_format_modifier_.empty())
    return std::vector<uint64_t>();

  auto it = preferred_format_modifier_.find(fourcc_format);
  if (it != preferred_format_modifier_.end()) {
    uint64_t supported_modifier = it->second;
    // AFBC for modeset buffers doesn't work correctly, as we can't fill them
    // with a valid AFBC buffer (b/172227166).
    // For now, don't use AFBC for modeset buffers.
    if (is_modeset && IsRockchipAfbc(supported_modifier)) {
      supported_modifier = DRM_FORMAT_MOD_LINEAR;
    }
    return std::vector<uint64_t>{supported_modifier};
  }

  return GetFormatModifiers(fourcc_format);
}

std::vector<uint64_t>
HardwareDisplayController::GetFormatModifiersForTestModeset(
    uint32_t fourcc_format) {
  // If we're about to test, clear the current preferred modifier.
  preferred_format_modifier_.clear();
  return GetFormatModifiers(fourcc_format);
}

void HardwareDisplayController::UpdatePreferredModifierForFormat(
    gfx::BufferFormat buffer_format,
    uint64_t modifier) {
  uint32_t fourcc_format = GetFourCCFormatFromBufferFormat(buffer_format);
  preferred_format_modifier_[fourcc_format] = modifier;

  uint32_t opaque_fourcc_format =
      GetFourCCFormatForOpaqueFramebuffer(buffer_format);
  preferred_format_modifier_[opaque_fourcc_format] = modifier;
}

void HardwareDisplayController::MoveCursor(const gfx::Point& location) {
  cursor_location_ = location;
  UpdateCursorLocation();
}

void HardwareDisplayController::SetCursor(SkBitmap bitmap) {
  if (bitmap.drawsNothing()) {
    current_cursor_ = nullptr;
  } else {
    current_cursor_ = NextCursorBuffer(bitmap);
    DrawCursor(current_cursor_, bitmap);
  }

  UpdateCursorImage();
}

void HardwareDisplayController::AddCrtc(
    std::unique_ptr<CrtcController> controller) {
  scoped_refptr<DrmDevice> drm = controller->drm();
  DCHECK(crtc_controllers_.empty() || drm == GetDrmDevice());

  // Check if this controller owns any planes and ensure we keep track of them.
  const std::vector<std::unique_ptr<HardwareDisplayPlane>>& all_planes =
      drm->plane_manager()->planes();
  uint32_t crtc = controller->crtc();
  for (const auto& plane : all_planes) {
    if (plane->in_use() && (plane->owning_crtc() == crtc))
      owned_hardware_planes_.old_plane_list.push_back(plane.get());
  }

  crtc_controllers_.push_back(std::move(controller));
}

std::unique_ptr<CrtcController> HardwareDisplayController::RemoveCrtc(
    const scoped_refptr<DrmDevice>& drm,
    uint32_t crtc) {
  auto controller_it = base::ranges::find_if(
      crtc_controllers_,
      [drm, crtc](const std::unique_ptr<CrtcController>& crtc_controller) {
        return crtc_controller->drm() == drm && crtc_controller->crtc() == crtc;
      });
  if (controller_it == crtc_controllers_.end())
    return nullptr;

  std::unique_ptr<CrtcController> controller(std::move(*controller_it));
  crtc_controllers_.erase(controller_it);

  // Move all the planes that have been committed in the last pageflip for this
  // CRTC at the end of the collection.
  auto first_plane_to_disable_it =
      std::partition(owned_hardware_planes_.old_plane_list.begin(),
                     owned_hardware_planes_.old_plane_list.end(),
                     [crtc](const HardwareDisplayPlane* plane) {
                       return plane->owning_crtc() != crtc;
                     });

  // Disable the planes enabled with the last commit on |crtc|, otherwise
  // the planes will be visible if the crtc is reassigned to another connector.
  HardwareDisplayPlaneList hardware_plane_list;
  std::copy(first_plane_to_disable_it,
            owned_hardware_planes_.old_plane_list.end(),
            std::back_inserter(hardware_plane_list.old_plane_list));
  drm->plane_manager()->DisableOverlayPlanes(&hardware_plane_list);

  // Remove the planes assigned to |crtc|.
  owned_hardware_planes_.old_plane_list.erase(
      first_plane_to_disable_it, owned_hardware_planes_.old_plane_list.end());

  return controller;
}

bool HardwareDisplayController::HasCrtc(const scoped_refptr<DrmDevice>& drm,
                                        uint32_t crtc) const {
  for (const auto& controller : crtc_controllers_) {
    if (controller->drm() == drm && controller->crtc() == crtc)
      return true;
  }

  return false;
}

bool HardwareDisplayController::IsMirrored() const {
  return crtc_controllers_.size() > 1 && !IsTiled();
}

bool HardwareDisplayController::IsEnabled() const {
  bool is_enabled = false;

  for (const auto& controller : crtc_controllers_)
    is_enabled |= controller->is_enabled();

  return is_enabled;
}

bool HardwareDisplayController::IsTiled() const {
  return tile_property_.has_value();
}

gfx::Size HardwareDisplayController::GetModeSize() const {
  // If there are multiple CRTCs they should all have the same size.
  const gfx::Size mode_size = ModeSize(crtc_controllers_[0]->mode());
  if (tile_property_.has_value() && mode_size == tile_property_->tile_size) {
    return GetTotalTileDisplaySize(*tile_property_);
  }
  return mode_size;
}

float HardwareDisplayController::GetRefreshRate() const {
  // If there are multiple CRTCs they should all have the same refresh rate.
  return ModeRefreshRate(crtc_controllers_[0]->mode());
}

base::TimeDelta HardwareDisplayController::GetRefreshInterval() const {
  // If there are multiple CRTCs they should all have the same refresh rate.
  float vrefresh = GetRefreshRate();
  return vrefresh ? base::Seconds(1) / vrefresh : base::TimeDelta();
}

base::TimeTicks HardwareDisplayController::GetTimeOfLastFlip() const {
  return time_of_last_flip_;
}

scoped_refptr<DrmDevice> HardwareDisplayController::GetDrmDevice() const {
  DCHECK(!crtc_controllers_.empty());
  // TODO(dnicoara) When we support mirroring across DRM devices, figure out
  // which device should be used for allocations.
  return crtc_controllers_[0]->drm();
}

void HardwareDisplayController::OnPageFlipComplete(
    int modeset_sequence,
    DrmOverlayPlaneList pending_planes,
    const gfx::PresentationFeedback& presentation_feedback) {
  if (!page_flip_request_)
    return;  // Modeset occurred during this page flip.

  time_of_last_flip_ = presentation_feedback.timestamp;
  current_planes_ = std::move(pending_planes);

  for (const auto& controller : crtc_controllers_) {
    // Only reset the modeset buffer of the crtcs for pageflips that were
    // committed after the modeset.
    if (modeset_sequence == GetDrmDevice()->modeset_sequence_id()) {
      GetDrmDevice()->plane_manager()->ResetModesetStateForCrtc(
          controller->crtc());
    }
  }
  page_flip_request_ = nullptr;
}

void HardwareDisplayController::WriteIntoTrace(
    perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("origin", origin_.ToString());
  dict.Add("cursor_location", cursor_location_.ToString());
  dict.Add("has_page_flip_request", page_flip_request_ != nullptr);

  dict.Add("owned_hardware_planes", owned_hardware_planes_);
  dict.Add("crtc_controllers", crtc_controllers_);

  {
    auto array = dict.AddArray("preferred_format_modifiers");
    for (const auto& format_modifier : preferred_format_modifier_) {
      auto format_dict = array.AppendDictionary();

      format_dict.Add("format", NumberToHexString(format_modifier.first));
      format_dict.Add("modifier", NumberToHexString(format_modifier.second));
    }
  }
}

size_t HardwareDisplayController::NumOfSupportedCursorSizesForTesting() const {
  return supported_cursor_sizes_.size();
}

gfx::Size HardwareDisplayController::CurrentCursorSizeForTesting() const {
  return current_cursor_ ? current_cursor_->GetSize() : gfx::Size();
}

void HardwareDisplayController::OnModesetComplete(
    const DrmOverlayPlaneList& modeset_planes) {
  // Modesetting is blocking so it has an immediate effect. We can assume that
  // the current planes have been updated. However, if a page flip is still
  // pending, set the pending planes to the same values so that the callback
  // keeps the correct state.
  page_flip_request_ = nullptr;
  owned_hardware_planes_.legacy_page_flips.clear();
  current_planes_ = DrmOverlayPlane::Clone(modeset_planes);
  time_of_last_flip_ = base::TimeTicks::Now();
}

void HardwareDisplayController::AllocateCursorBuffers() {
  TRACE_EVENT0("drm", "HDC::AllocateCursorBuffers");
  constexpr int kActiveBufferCount = 2;

  for (auto& size : supported_cursor_sizes_) {
    for (int i = 0; i < kActiveBufferCount; i++) {
      cursor_buffer_map_[size].push_back(
          MakeCursorDrmBuffer(size, GetDrmDevice()));
    }
  }
}

DrmDumbBuffer* HardwareDisplayController::NextCursorBuffer(
    const SkBitmap& image) {
  // Use the largest buffer as default.
  gfx::Size buffer_size = supported_cursor_sizes_.back();

  // Find the smallest buffer size that fits the |image| size.
  for (auto size : supported_cursor_sizes_) {
    if (image.width() <= size.width() && image.height() <= size.width()) {
      buffer_size = size;
      break;
    }
  }

  // Return the not in-use buffer with the |buffer_size|.
  auto& active_buffers = cursor_buffer_map_[buffer_size];
  DrmDumbBuffer* next_buffer = active_buffers.front().get();
  if (next_buffer == current_cursor_) {
    return active_buffers.back().get();
  }
  return next_buffer;
}

void HardwareDisplayController::UpdateCursorImage() {
  uint32_t handle = 0;
  gfx::Size size;

  if (current_cursor_) {
    handle = current_cursor_->GetHandle();
    size = current_cursor_->GetSize();
  }

  for (const auto& controller : crtc_controllers_) {
    controller->SetCursor(handle, size);
  }
}

void HardwareDisplayController::UpdateCursorLocation() {
  for (const auto& controller : crtc_controllers_)
    controller->MoveCursor(cursor_location_);
}

void HardwareDisplayController::ResetCursor() {
  UpdateCursorLocation();
  UpdateCursorImage();
}

void HardwareDisplayController::InitSupportedCursorSizes() {
  // Only use dynamic cursor size on Intel GPUs.
  std::optional<std::string> driver = GetDrmDevice()->GetDriverName();
  bool use_dynamic_cursor_size = IsUseDynamicCursorSizeEnabled() &&
                                 driver.has_value() && *driver == "i915";
  if (use_dynamic_cursor_size) {
    const std::vector<std::unique_ptr<HardwareDisplayPlane>>& planes =
        GetDrmDevice()->plane_manager()->planes();
    for (const auto& plane : planes) {
      // Currently on Intel, if there are multiple CRTCs they should all have
      // the same supported cursor sizes.
      if (plane->type() == DRM_PLANE_TYPE_CURSOR) {
        const std::vector<gfx::Size>& supported_cursor_sizes =
            plane->supported_cursor_sizes();
        supported_cursor_sizes_.assign(supported_cursor_sizes.begin(),
                                       supported_cursor_sizes.end());
        break;
      }
    }
  }

  if (supported_cursor_sizes_.empty()) {
    // Get the maximum cursor size supported by the GPU.
    const gfx::Size max_cursor_size_supported =
        GetMaximumCursorSize(*GetDrmDevice());
    // max_cursor_size_supported can be as large as 4096 depending on platform
    // and driver capabilities, but we don't need huge buffer like that for the
    // cursor.
    supported_cursor_sizes_.push_back(gfx::Size(
        std::min(max_cursor_size_supported.width(), kMaxCursorBufferSize),
        std::min(max_cursor_size_supported.height(), kMaxCursorBufferSize)));
  }

  // Sort the supported cursor sizes in ascending order so that we can use the
  // smallest buffer.
  DCHECK(!supported_cursor_sizes_.empty());
  std::sort(supported_cursor_sizes_.begin(), supported_cursor_sizes_.end(),
            CursorSizeComparator());
}

}  // namespace ui
