// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/ui_base_switches.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/common/drm_overlay_candidates.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/host/drm_cursor.h"
#include "ui/ozone/platform/drm/host/drm_display_host_manager.h"
#include "ui/ozone/platform/drm/host/drm_overlay_manager_host.h"
#include "ui/ozone/platform/drm/host/gpu_thread_observer.h"

namespace ui {

namespace {

// Helper class that provides DrmCursor with a mechanism to send messages
// to the GPU process.
class CursorIPC : public DrmCursorProxy {
 public:
  CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
            base::RepeatingCallback<void(IPC::Message*)> send_callback);
  ~CursorIPC() override;

  // DrmCursorProxy implementation.
  void CursorSet(gfx::AcceleratedWidget window,
                 const std::vector<SkBitmap>& bitmaps,
                 const gfx::Point& point,
                 int frame_delay_ms) override;
  void Move(gfx::AcceleratedWidget window, const gfx::Point& point) override;
  void InitializeOnEvdevIfNecessary() override;

 private:
  bool IsConnected();
  void Send(IPC::Message* message);

  scoped_refptr<base::SingleThreadTaskRunner> send_runner_;
  base::RepeatingCallback<void(IPC::Message*)> send_callback_;

  DISALLOW_COPY_AND_ASSIGN(CursorIPC);
};

CursorIPC::CursorIPC(scoped_refptr<base::SingleThreadTaskRunner> send_runner,
                     base::RepeatingCallback<void(IPC::Message*)> send_callback)
    : send_runner_(send_runner), send_callback_(std::move(send_callback)) {}

CursorIPC::~CursorIPC() {}

bool CursorIPC::IsConnected() {
  return !send_callback_.is_null();
}

void CursorIPC::CursorSet(gfx::AcceleratedWidget window,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& point,
                          int frame_delay_ms) {
  Send(new OzoneGpuMsg_CursorSet(window, bitmaps, point, frame_delay_ms));
}

void CursorIPC::Move(gfx::AcceleratedWidget window, const gfx::Point& point) {
  Send(new OzoneGpuMsg_CursorMove(window, point));
}

void CursorIPC::InitializeOnEvdevIfNecessary() {}

void CursorIPC::Send(IPC::Message* message) {
  if (IsConnected() && send_runner_->PostTask(
                           FROM_HERE, base::BindOnce(send_callback_, message)))
    return;

  // Drop disconnected updates. The cursor will get set once we connect, via
  // SetDrmCursorProxy().
  delete message;
}

}  // namespace

DrmGpuPlatformSupportHost::DrmGpuPlatformSupportHost(DrmCursor* cursor)
    : ui_runner_(base::ThreadTaskRunnerHandle::IsSet()
                     ? base::ThreadTaskRunnerHandle::Get()
                     : nullptr),
      cursor_(cursor) {
  if (ui_runner_)
    weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

DrmGpuPlatformSupportHost::~DrmGpuPlatformSupportHost() {}

void DrmGpuPlatformSupportHost::AddGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.AddObserver(observer);

  if (IsConnected())
    observer->OnGpuThreadReady();
}

void DrmGpuPlatformSupportHost::RemoveGpuThreadObserver(
    GpuThreadObserver* observer) {
  gpu_thread_observers_.RemoveObserver(observer);
}

bool DrmGpuPlatformSupportHost::IsConnected() {
  return host_id_ >= 0 && channel_established_;
}

void DrmGpuPlatformSupportHost::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  NOTREACHED() << "DrmGpuPlatformSupportHost::OnGpuServiceLaunched shouldn't "
                  "be used with pre-mojo IPC";
}

void DrmGpuPlatformSupportHost::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {
  // If there was a task runner set during construction, prefer using that.
  if (!ui_runner_) {
    ui_runner_ = std::move(ui_runner);
    weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  }
  DCHECK(!ui_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnGpuProcessLaunched",
               "host_id", host_id);
  host_id_ = host_id;
  send_runner_ = std::move(send_runner);
  send_callback_ = std::move(send_callback);

  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuProcessLaunched();

  ui_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DrmGpuPlatformSupportHost::OnChannelEstablished,
                     weak_ptr_));
}

void DrmGpuPlatformSupportHost::OnChannelDestroyed(int host_id) {
  TRACE_EVENT1("drm", "DrmGpuPlatformSupportHost::OnChannelDestroyed",
               "host_id", host_id);

  if (host_id_ == host_id) {
    cursor_->ResetDrmCursorProxy();
    host_id_ = -1;
    channel_established_ = false;
    send_runner_ = nullptr;
    send_callback_.Reset();
    for (GpuThreadObserver& observer : gpu_thread_observers_)
      observer.OnGpuThreadRetired();
  }
}

void DrmGpuPlatformSupportHost::OnMessageReceived(const IPC::Message& message) {
  DCHECK(ui_runner_);
  if (ui_runner_->BelongsToCurrentThread()) {
    if (OnMessageReceivedForDrmDisplayHostManager(message))
      return;
    OnMessageReceivedForDrmOverlayManager(message);
  } else {
    ui_runner_->PostTask(
        FROM_HERE, base::BindOnce(&DrmGpuPlatformSupportHost::OnMessageReceived,
                                  weak_ptr_, message));
  }
}

bool DrmGpuPlatformSupportHost::Send(IPC::Message* message) {
  if (IsConnected() && send_runner_->PostTask(
                           FROM_HERE, base::BindOnce(send_callback_, message)))
    return true;

  delete message;
  return false;
}

// DisplayHost
void DrmGpuPlatformSupportHost::RegisterHandlerForDrmDisplayHostManager(
    DrmDisplayHostManager* handler) {
  display_manager_ = handler;
}

void DrmGpuPlatformSupportHost::UnRegisterHandlerForDrmDisplayHostManager() {
  display_manager_ = nullptr;
}

void DrmGpuPlatformSupportHost::OnChannelEstablished() {
  TRACE_EVENT0("drm", "DrmGpuPlatformSupportHost::OnChannelEstablished");
  channel_established_ = true;

  for (GpuThreadObserver& observer : gpu_thread_observers_)
    observer.OnGpuThreadReady();

  // The cursor is special since it will process input events on the IO thread
  // and can by-pass the UI thread. This means that we need to special case it
  // and notify it after all other observers/handlers are notified such that the
  // (windowing) state on the GPU can be initialized before the cursor is
  // allowed to IPC messages (which are targeted to a specific window).
  cursor_->SetDrmCursorProxy(
      std::make_unique<CursorIPC>(send_runner_, send_callback_));
}

bool DrmGpuPlatformSupportHost::OnMessageReceivedForDrmDisplayHostManager(
    const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportHost, message)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_UpdateNativeDisplays,
                        OnUpdateNativeDisplays)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayConfigured, OnDisplayConfigured)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateReceived, OnHDCPStateReceived)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_HDCPStateUpdated, OnHDCPStateUpdated)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlTaken, OnTakeDisplayControl)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_DisplayControlRelinquished,
                        OnRelinquishDisplayControl)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmGpuPlatformSupportHost::OnUpdateNativeDisplays(
    const std::vector<DisplaySnapshot_Params>& params) {
  display_manager_->GpuHasUpdatedNativeDisplays(params);
}

void DrmGpuPlatformSupportHost::OnDisplayConfigured(int64_t display_id,
                                                    bool status) {
  display_manager_->GpuConfiguredDisplay(display_id, status);
}

void DrmGpuPlatformSupportHost::OnHDCPStateReceived(int64_t display_id,
                                                    bool status,
                                                    display::HDCPState state) {
  display_manager_->GpuReceivedHDCPState(display_id, status, state);
}

void DrmGpuPlatformSupportHost::OnHDCPStateUpdated(int64_t display_id,
                                                   bool status) {
  display_manager_->GpuUpdatedHDCPState(display_id, status);
}

void DrmGpuPlatformSupportHost::OnTakeDisplayControl(bool status) {
  display_manager_->GpuTookDisplayControl(status);
}

void DrmGpuPlatformSupportHost::OnRelinquishDisplayControl(bool status) {
  display_manager_->GpuRelinquishedDisplayControl(status);
}

bool DrmGpuPlatformSupportHost::GpuRefreshNativeDisplays() {
  return Send(new OzoneGpuMsg_RefreshNativeDisplays());
}

bool DrmGpuPlatformSupportHost::GpuTakeDisplayControl() {
  return Send(new OzoneGpuMsg_TakeDisplayControl());
}

bool DrmGpuPlatformSupportHost::GpuRelinquishDisplayControl() {
  return Send(new OzoneGpuMsg_RelinquishDisplayControl());
}

bool DrmGpuPlatformSupportHost::GpuAddGraphicsDeviceOnUIThread(
    const base::FilePath& path,
    base::ScopedFD fd) {
  return Send(new OzoneGpuMsg_AddGraphicsDevice(
      path, base::FileDescriptor(std::move(fd))));
}

void DrmGpuPlatformSupportHost::GpuAddGraphicsDeviceOnIOThread(
    const base::FilePath& path,
    base::ScopedFD fd) {
  DCHECK(!send_callback_.is_null());
  send_callback_.Run(new OzoneGpuMsg_AddGraphicsDevice(
      path, base::FileDescriptor(std::move(fd))));
}

bool DrmGpuPlatformSupportHost::GpuRemoveGraphicsDevice(
    const base::FilePath& path) {
  return Send(new OzoneGpuMsg_RemoveGraphicsDevice(path));
}

// Overlays
void DrmGpuPlatformSupportHost::RegisterHandlerForDrmOverlayManager(
    DrmOverlayManagerHost* handler) {
  overlay_manager_ = handler;
}

void DrmGpuPlatformSupportHost::UnRegisterHandlerForDrmOverlayManager() {
  overlay_manager_ = nullptr;
}

bool DrmGpuPlatformSupportHost::OnMessageReceivedForDrmOverlayManager(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportHost, message)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_OverlayCapabilitiesReceived,
                        OnOverlayResult)
    // TODO(rjk): insert the extra
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DrmGpuPlatformSupportHost::OnOverlayResult(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params,
    const std::vector<OverlayCheckReturn_Params>& param_returns) {
  auto candidates = CreateOverlaySurfaceCandidateListFrom(params);
  auto returns = CreateOverlayStatusListFrom(param_returns);
  overlay_manager_->GpuSentOverlayResult(widget, candidates, returns);
}

bool DrmGpuPlatformSupportHost::GpuCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& candidates) {
  auto params = CreateParamsFromOverlaySurfaceCandidate(candidates);
  return Send(new OzoneGpuMsg_CheckOverlayCapabilities(widget, params));
}

// DrmDisplayHost
bool DrmGpuPlatformSupportHost::GpuConfigureNativeDisplay(
    int64_t display_id,
    const ui::DisplayMode_Params& display_mode,
    const gfx::Point& point) {
  return Send(
      new OzoneGpuMsg_ConfigureNativeDisplay(display_id, display_mode, point));
}

bool DrmGpuPlatformSupportHost::GpuDisableNativeDisplay(int64_t display_id) {
  return Send(new OzoneGpuMsg_DisableNativeDisplay(display_id));
}

bool DrmGpuPlatformSupportHost::GpuGetHDCPState(int64_t display_id) {
  return Send(new OzoneGpuMsg_GetHDCPState(display_id));
}

bool DrmGpuPlatformSupportHost::GpuSetHDCPState(int64_t display_id,
                                                display::HDCPState state) {
  return Send(new OzoneGpuMsg_SetHDCPState(display_id, state));
}

bool DrmGpuPlatformSupportHost::GpuSetColorMatrix(
    int64_t display_id,
    const std::vector<float>& color_matrix) {
  return Send(new OzoneGpuMsg_SetColorMatrix(display_id, color_matrix));
}

bool DrmGpuPlatformSupportHost::GpuSetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  return Send(
      new OzoneGpuMsg_SetGammaCorrection(display_id, degamma_lut, gamma_lut));
}

bool DrmGpuPlatformSupportHost::GpuDestroyWindow(
    gfx::AcceleratedWidget widget) {
  return Send(new OzoneGpuMsg_DestroyWindow(widget));
}

bool DrmGpuPlatformSupportHost::GpuCreateWindow(
    gfx::AcceleratedWidget widget,
    const gfx::Rect& initial_bounds) {
  return Send(new OzoneGpuMsg_CreateWindow(widget, initial_bounds));
}

bool DrmGpuPlatformSupportHost::GpuWindowBoundsChanged(
    gfx::AcceleratedWidget widget,
    const gfx::Rect& bounds) {
  return Send(new OzoneGpuMsg_WindowBoundsChanged(widget, bounds));
}

}  // namespace ui
