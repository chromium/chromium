// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_manager.h"

#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "wolvic/browser/vr/wvr_api.h"


namespace wolvic {

namespace {

const int64_t kFrameTimeOutMilliseconds = 1000;
constexpr double kThumbstickDeadzone = 0.16; // From gamepad_builder.cc

void WvrMatToTransform(const float in[16], gfx::Transform* out) {
  *out = gfx::Transform::RowMajor(in[0], in[1], in[2], in[3], in[4], in[5],
                                  in[6], in[7], in[8], in[9], in[10], in[11],
                                  in[12], in[13], in[14], in[15]);
}

gfx::Transform WvrPoseToTransform(const mozilla::gfx::VRPose* pose) {
  gfx::DecomposedTransform decomp;

  decomp.quaternion =
      gfx::Quaternion(pose->orientation[0], pose->orientation[1],
                      pose->orientation[2], pose->orientation[3]);

  decomp.translate[0] = pose->position[0];
  decomp.translate[1] = pose->position[1];
  decomp.translate[2] = pose->position[2];

  return gfx::Transform::Compose(decomp);
}

device::mojom::VRPosePtr PoseToVRPosePtr(const mozilla::gfx::VRPose* p) {
  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();
  pose->position = gfx::Point3F(p->position[0], p->position[1], p->position[2]);
  pose->orientation = gfx::Quaternion(p->orientation[0], p->orientation[1],
                                      p->orientation[2], p->orientation[3]);
  return pose;
}

device::GamepadHand ToGamepadHand(mozilla::gfx::ControllerHand hand) {
  using mozilla::gfx::ControllerHand;

  switch (hand) {
    case ControllerHand::Left:
      return device::GamepadHand::kLeft;
    case ControllerHand::Right:
      return device::GamepadHand::kRight;
    case ControllerHand::_empty:
    case ControllerHand::EndGuard_:
      DCHECK(false) << "Unexpected ControllerHand value: "
                    << static_cast<uint8_t>(hand);
      return device::GamepadHand::kNone;
  }
}

device::Gamepad ToGamepad(const mozilla::gfx::VRControllerState& controller) {
  device::Gamepad gamepad;
  gamepad.hand = ToGamepadHand(controller.hand);

  DCHECK_LT(controller.numButtons, device::Gamepad::kButtonsLengthCap);
  gamepad.buttons_length = controller.numButtons;
  for (uint32_t i = 0; i < gamepad.buttons_length; ++i) {
    gamepad.buttons[i].pressed = controller.buttonPressed & (1 << i);
    gamepad.buttons[i].touched = controller.buttonTouched & (1 << i);
    gamepad.buttons[i].value = controller.triggerValue[i];
  }

  DCHECK_LT(controller.numAxes, device::Gamepad::kAxesLengthCap);
  gamepad.axes_length = controller.numAxes;
  for (uint32_t i = 0; i < gamepad.axes_length; ++i) {
    gamepad.axes[i] = std::fabs(controller.axisValue[i]) < kThumbstickDeadzone ? 0.0 : controller.axisValue[i];
  };

  return gamepad;
}

device::mojom::XRViewPtr CreateView(
    mozilla::gfx::VRDisplayState::Eye eye,
    const mozilla::gfx::VRDisplayState& display_state,
    const gfx::Size& maximum_size,
    const mozilla::gfx::VRPose* pose) {
  device::mojom::XRViewPtr view = device::mojom::XRView::New();
  if (eye == mozilla::gfx::VRDisplayState::Eye::Eye_Left) {
    view->eye = device::mojom::XREye::kLeft;
    view->viewport =
        gfx::Rect(0, 0, maximum_size.width() / 2, maximum_size.height());
  } else if (eye == mozilla::gfx::VRDisplayState::Eye::Eye_Right) {
    view->eye = device::mojom::XREye::kRight;
    view->viewport = gfx::Rect(maximum_size.width() / 2, 0,
                               maximum_size.width() / 2, maximum_size.height());
  } else {
    NOTREACHED();
  }

  view->field_of_view = device::mojom::VRFieldOfView::New();

  auto& eye_fov = display_state.eyeFOV[eye];
  view->field_of_view->up_degrees = eye_fov.upDegrees;
  view->field_of_view->down_degrees = eye_fov.downDegrees;
  view->field_of_view->left_degrees = eye_fov.leftDegrees;
  view->field_of_view->right_degrees = eye_fov.rightDegrees;

  if (pose) {
    const gfx::Transform head_mat = WvrPoseToTransform(pose);
    gfx::Transform eye_from_head;
    WvrMatToTransform(display_state.eyeTransform[eye], &eye_from_head);
    gfx::Transform head_from_eye = eye_from_head.GetCheckedInverse();

    view->mojo_from_view = head_mat * head_from_eye;
  }
  return view;
}

std::vector<device::mojom::XRViewPtr> CreateViews(
    const mozilla::gfx::VRDisplayState& display_state,
    const mozilla::gfx::VRPose* pose,
    gfx::Size maximum_size) {
  std::vector<device::mojom::XRViewPtr> views(2);
  views[0] = CreateView(mozilla::gfx::VRDisplayState::Eye::Eye_Left,
                        display_state, maximum_size, pose);
  views[1] = CreateView(mozilla::gfx::VRDisplayState::Eye::Eye_Right,
                        display_state, maximum_size, pose);

  return views;
}

}  // namespace

WvrManager::WvrManager(WvrApi *wvr_api, WvrGraphicsDelegate* graphics)
    : wvr_api_(wvr_api),
      graphics_(graphics),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

WvrManager::~WvrManager() {
  ClosePresentationBindings();
  ExitWebXRPresentation(base::NullCallback());
  webxr_.EndPresentation();
}

void WvrManager::StartWebXRPresentation(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
    base::OnceClosure exit_callback) {
  DCHECK(IsOnWvrThread());

  exit_vr_callback_ = std::move(exit_callback);

  wvr_api_->StartWebXR();

  ConnectPresentingService(std::move(options), std::move(callback));
}

void WvrManager::ExitWebXRPresentation(base::OnceClosure callback) {
  DCHECK(IsOnWvrThread());

  wvr_api_->ExitWebXR();

  if (callback)
    std::move(callback).Run();
}

bool WvrManager::IsOnWvrThread() const {
  return task_runner_->BelongsToCurrentThread();
}

void WvrManager::CreateOrResizeWebXrSurface(const gfx::Size& size) {
  DCHECK(IsOnWvrThread());
  if (!graphics_->CreateOrResizeWebXrSurface(
          size,
          base::BindRepeating(&WvrManager::OnWebXrFrameAvailable,
                              weak_ptr_factory_.GetWeakPtr()))) {
    return;
  }
  if (mailbox_bridge_)
    mailbox_bridge_->ResizeSurface(size.width(), size.height());
  else
    CreateSurfaceBridge(graphics_->webxr_surface_texture());
}

void WvrManager::OnGpuProcessConnectionReady() {
  CHECK(mailbox_bridge_);

  DCHECK(!webxr_.HaveAnimatingFrame());
  webxr_.NotifyMailboxBridgeReady();
}

void WvrManager::CreateSurfaceBridge(
    gl::SurfaceTexture* surface_texture) {
  DCHECK(!mailbox_bridge_);
  DCHECK(!webxr_.mailbox_bridge_ready());
  mailbox_bridge_ = std::make_unique<webxr::MailboxToSurfaceBridgeImpl>();
  if (surface_texture)
    mailbox_bridge_->CreateSurface(surface_texture);
  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&WvrManager::OnGpuProcessConnectionReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WvrManager::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) {
  DCHECK(IsOnWvrThread());
  ClosePresentationBindings();

  std::vector<device::mojom::XRViewPtr> views =
      CreateViews(wvr_api_->get_system_state().displayState, nullptr /*pose*/,
                  graphics_->get_screen_size());
  int width = 0;
  int height = 0;
  for (const auto& view : views) {
    width += view->viewport.width();
    height = std::max(height, view->viewport.height());
  }

  gfx::Size webxr_size(width, height);
  DVLOG(1) << __func__ << ": resize initial to " << webxr_size.width() << "x"
           << webxr_size.height();

  // Decide which transport mechanism we want to use. This sets
  // the webxr_use_* options as a side effect.
  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      GetWebXrFrameTransportOptions(options);

  CreateOrResizeWebXrSurface(webxr_size);

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();

  submit_client_.set_disconnect_handler(base::BindOnce(
      &WvrManager::OnSubmitClientMojoConnectionError, GetWeakPtr()));

  submit_frame_sink->transport_options = std::move(transport_options);
  submit_frame_sink->provider =
      presentation_receiver_.BindNewPipeAndPassRemote();

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_receiver_.BindNewPipeAndPassRemote();
  session->submit_frame_sink = std::move(submit_frame_sink);

  session->enabled_features.insert(session->enabled_features.end(),
                                   options->required_features.begin(),
                                   options->required_features.end());
  session->enabled_features.insert(session->enabled_features.end(),
                                   options->optional_features.begin(),
                                   options->optional_features.end());

  session->device_config = device::mojom::XRSessionDeviceConfig::New();
  auto* config = session->device_config.get();

  config->views = std::move(views);
  config->supports_viewport_scaling = true;
  session->enviroment_blend_mode =
      device::mojom::XREnvironmentBlendMode::kOpaque;
  config->default_framebuffer_scale =
      wvr_api_->get_system_state().displayState.nativeFramebufferScaleFactor;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  std::move(callback).Run(std::move(session));
}

device::mojom::XRPresentationTransportOptionsPtr
WvrManager::GetWebXrFrameTransportOptions(
    const device::mojom::XRRuntimeSessionOptionsPtr& options) {
  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;
  transport_options->transport_method =
      device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
  transport_options->wait_for_render_notification = true;

  return transport_options;
}

void WvrManager::OnWebXrFrameAvailable() {
  // This is called each time a frame that was drawn on the WebVR Surface
  // arrives on the SurfaceTexture.

  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();

  // The processing frame would be empty when this method is called again from
  // Android system after OnWebXrTimedOut.
  if (webxr_.HaveProcessingFrame()) {
    // Frame should be locked. Unlock it.
    DCHECK(webxr_.GetProcessingFrame()->state_locked);
    webxr_.GetProcessingFrame()->state_locked = false;

    if (!SubmitFrameInternal(webxr_.GetProcessingFrame()->index))
      return;

    if (webxr_.HaveRenderingFrame())
      webxr_.EndFrameRendering();
    webxr_.TransitionFrameProcessingToRendering();
  }

  // Renderer is waiting for the previous frame to render, unblock it now.
  submit_client_->OnSubmitFrameRendered();

  WebXrTryStartAnimatingFrame();
}

void WvrManager::OnWebXrTimedOut() {
  DCHECK(IsOnWvrThread());
  OnWebXrFrameAvailable();
}

void WvrManager::ClosePresentationBindings() {
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();

  if (!get_frame_data_callback_.is_null())
    std::move(get_frame_data_callback_).Run(nullptr);

  submit_client_.reset();
  presentation_receiver_.reset();
  frame_data_receiver_.reset();
}

void WvrManager::OnSubmitClientMojoConnectionError() {
  ClosePresentationBindings();
  ExitWebXRPresentation(base::NullCallback());
}

// See
// https://github.com/immersive-web/webxr-input-profiles/blob/main/packages/registry/profiles/
// for reference
static void PopulateProfiles(
    device::mojom::XRInputSourceStatePtr& input_source,
    const mozilla::gfx::VRControllerState& controller) {
  switch (controller.type) {
    case mozilla::gfx::VRControllerType::HTCVive:
      input_source->description->profiles = {
          "htc-vive", "generic-trigger-squeeze-touchpad"};
      break;
    case mozilla::gfx::VRControllerType::HTCViveCosmos:
      input_source->description->profiles = {
          "htc-vive-cosmos", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::HTCViveFocus:
      input_source->description->profiles = {"htc-vive-focus",
                                             "generic-trigger-touchpad"};
      break;
    case mozilla::gfx::VRControllerType::HTCViveFocusPlus:
      input_source->description->profiles = {
          "htc-vive-focus-plus", "generic-trigger-squeeze-touchpad"};
      break;
    case mozilla::gfx::VRControllerType::MSMR:
      input_source->description->profiles = {
          "microsoft-mixed-reality",
          "generic-trigger-squeeze-touchpad-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::ValveIndex:
      input_source->description->profiles = {
          "valve-index", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::OculusGo:
      input_source->description->profiles = {"oculus-go", "oculus-touch",
                                             "generic-trigger-touchpad"};
      break;
    case mozilla::gfx::VRControllerType::OculusTouch:
      input_source->description->profiles = {
          "oculus-touch-v2", "oculus-touch",
          "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::OculusTouch2:
      input_source->description->profiles = {
          "oculus-touch-v2", "oculus-touch",
          "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::OculusTouch3:
      input_source->description->profiles = {
          "oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
          "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::PicoG2:
      input_source->description->profiles = {
          "pico-g2", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::PicoNeo2:
      input_source->description->profiles = {
          "pico-neo2", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::Pico4:
      input_source->description->profiles = {
          "pico-4", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::PicoGaze:
    case mozilla::gfx::VRControllerType::_empty:
      input_source->description->profiles = {"generic-button"};
      break;
    case mozilla::gfx::VRControllerType::_end:
      NOTREACHED();
      break;
  };
}

std::vector<device::mojom::XRInputSourceStatePtr>
WvrManager::GetInputSourceState() {
  std::vector<device::mojom::XRInputSourceStatePtr> input_sources;

  for (uint32_t i = 0; i < mozilla::gfx::kVRControllerMaxCount; ++i) {
    const auto& controller = wvr_api_->get_system_state().controllerState[i];
    if (controller.type == mozilla::gfx::VRControllerType::_empty)
      continue;

    device::mojom::XRInputSourceStatePtr input_source =
        device::mojom::XRInputSourceState::New();

    // ID 0 will cause a DCHECK in the hash table used on the blink side.
    // To ensure that we don't have any collisions with other ids, increment
    // all of the ids by one.
    input_source->source_id = i + 1;
    input_source->primary_input_pressed = controller.buttonPressed;
    input_source->primary_input_clicked = controller.buttonTouched;

    input_source->description = device::mojom::XRInputSourceDescription::New();
    switch (controller.targetRayMode) {
      case mozilla::gfx::TargetRayMode::Gaze:
        input_source->description->target_ray_mode =
            device::mojom::XRTargetRayMode::GAZING;
        break;
      case mozilla::gfx::TargetRayMode::Screen:
        input_source->description->target_ray_mode =
            device::mojom::XRTargetRayMode::TAPPING;
        break;
      case mozilla::gfx::TargetRayMode::TrackedPointer:
        input_source->description->target_ray_mode =
            device::mojom::XRTargetRayMode::POINTING;
        break;
    }
    input_source->description->handedness =
        controller.hand == mozilla::gfx::ControllerHand::Left
            ? device::mojom::XRHandedness::LEFT
            : device::mojom::XRHandedness::RIGHT;

    PopulateProfiles(input_source, controller);

    input_source->gamepad = ToGamepad(controller);

    auto supportsControllerFlag =
        [&controller](mozilla::gfx::ControllerCapabilityFlags flag) {
          return (static_cast<int>(controller.flags) &
                  static_cast<int>(flag)) != 0;
        };
    input_source->emulated_position =
        !supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_Position) &&
        supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_PositionEmulated);

    if (supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_Orientation)) {
      input_source->description->input_from_pointer =
          WvrPoseToTransform(&controller.targetRayPose);
    }

    if (supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_Position) ||
        supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_PositionEmulated) ||
        supportsControllerFlag(
            mozilla::gfx::ControllerCapabilityFlags::Cap_GripSpacePosition)) {
      input_source->mojo_from_input = WvrPoseToTransform(&controller.pose);
    }

    input_sources.push_back(std::move(input_source));
  }
  return input_sources;
}

bool WvrManager::WebVrCanAnimateFrame() {
  // If we already have a JS frame that's animating, don't send another one.
  // This check depends on the Renderer calling either SubmitFrame or
  // SubmitFrameMissing for each animated frame.
  if (webxr_.HaveAnimatingFrame()) {
    DVLOG(2) << __func__
             << ": waiting for current animating frame to start processing";
    return false;
  }

  if (webxr_.HaveProcessingFrame()) {
    DVLOG(2) << __func__ << ": waiting for processing state";
    return false;
  }

  if (get_frame_data_callback_.is_null()) {
    DVLOG(2) << __func__ << ": waiting for get_frame_data_callback_";
    return false;
  }

  return true;
}

void WvrManager::GetFrameData(
    device::mojom::XRFrameDataRequestOptionsPtr options,
    device::mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (!get_frame_data_callback_.is_null()) {
    DLOG(WARNING) << ": previous get_frame_data_callback_ was not used yet";
    frame_data_receiver_.ReportBadMessage(
        "Requested VSync before waiting for response to previous request.");
    ClosePresentationBindings();
    return;
  }

  pending_time_ = base::TimeTicks();
  get_frame_data_callback_ = std::move(callback);
  WebXrTryStartAnimatingFrame();
}

void WvrManager::WebXrTryStartAnimatingFrame() {
  DCHECK(IsOnWvrThread());

  if (!WebVrCanAnimateFrame()) {
    return;
  }

  device::mojom::XRFrameDataPtr frame_data = device::mojom::XRFrameData::New();
  mozilla::gfx::VRSystemState system_state = wvr_api_->get_system_state();
  const mozilla::gfx::VRPose* pose = &system_state.sensorState.pose;

  frame_data->frame_id = webxr_.StartFrameAnimating();
  frame_data->views =
      CreateViews(wvr_api_->get_system_state().displayState,
                  pose,
                  graphics_->webxr_surface_size());

  frame_data->mojo_space_reset = true;

  frame_data->input_state = GetInputSourceState();

  frame_data->mojo_from_viewer = PoseToVRPosePtr(pose);

  frame_data->time_delta = pending_time_ - base::TimeTicks();

  std::move(get_frame_data_callback_).Run(std::move(frame_data));
}

void WvrManager::GetEnvironmentIntegrationProvider(
    mojo::PendingAssociatedReceiver<
        device::mojom::XREnvironmentIntegrationProvider> environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  frame_data_receiver_.ReportBadMessage(
      "Environment integration is not supported.");
}

void WvrManager::SetInputSourceButtonListener(
    mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>) {
  // Input eventing is not supported. This call should not
  // be made on this device.
  frame_data_receiver_.ReportBadMessage("Input eventing is not supported.");
}

bool WvrManager::SubmitFrameInternal(int16_t frame_index) {
  DCHECK(IsOnWvrThread());

  // Force exit WebXR before pushing the frame if the presenting generation is
  // changed by stopping presenting in Wolvic.
  if (wvr_api_->PresentingGenerationChanged()) {
    if (!get_frame_data_callback_.is_null())
      std::move(get_frame_data_callback_).Run(nullptr);

    if (exit_vr_callback_)
      std::move(exit_vr_callback_).Run();
    return false;
  }

  last_frame_index_ = frame_index;

  if (!wvr_api_->SyncState(frame_index,
                           graphics_->webxr_texture_handle(),
                           graphics_->webxr_surface_size().width(),
                           graphics_->webxr_surface_size().height()))
    return false;

  return true;
}

bool WvrManager::IsSubmitFrameExpected(int16_t frame_index) {
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  XRSessionClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_.HaveAnimatingFrame())
    return false;

  device::WebXrFrame* animating_frame = webxr_.GetAnimatingFrame();

  if (animating_frame->index != frame_index) {
    LOG(ERROR) << __func__ << ": wrong frame index, got " << frame_index
               << ", expected " << animating_frame->index;
    presentation_receiver_.ReportBadMessage(
        "SubmitFrame called with wrong frame index");
    ClosePresentationBindings();
    return false;
  }

  // Frame looks valid.
  return true;
}

void WvrManager::SubmitFrameMissing(int16_t frame_index,
                                    const gpu::SyncToken& sync_token) {
  if (!IsSubmitFrameExpected(frame_index))
    return;

  // Renderer didn't submit a frame. Wait for the sync token to ensure
  // that any mailbox_bridge_ operations for the next frame happen after
  // whatever drawing the Renderer may have done before exiting.
  if (webxr_.mailbox_bridge_ready())
    mailbox_bridge_->WaitSyncToken(sync_token);

  DVLOG(2) << __func__ << ": recycle unused animating frame";
  DCHECK(webxr_.HaveAnimatingFrame());
  webxr_.RecycleUnusedAnimatingFrame();
}

void WvrManager::SubmitFrame(int16_t frame_index,
                             const gpu::MailboxHolder& mailbox,
                             base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  webxr_.ProcessOrDefer(
      base::BindOnce(&WvrManager::ProcessWebVrFrameFromMailbox,
                     weak_ptr_factory_.GetWeakPtr(), frame_index, mailbox));
}

bool WvrManager::SubmitFrameCommon(int16_t frame_index,
                                   base::TimeDelta time_waited) {
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  if (!IsSubmitFrameExpected(frame_index))
    return false;

  // TODO(tiago): we'll be using time_waited next.

  return true;
}

void WvrManager::ProcessWebVrFrameFromMailbox(
    int16_t frame_index,
    const gpu::MailboxHolder& mailbox) {
  DCHECK(webxr_.HaveProcessingFrame());

  webxr_.GetProcessingFrame()->state_locked = true;

  bool swapped = mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox);
  DCHECK(swapped);

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  webxr_frame_timeout_closure_.Reset(
      base::BindOnce(&WvrManager::OnWebXrTimedOut, GetWeakPtr()));

  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_frame_timeout_closure_.callback(),
                                base::Milliseconds(kFrameTimeOutMilliseconds));
}

void WvrManager::SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                             const gpu::SyncToken& sync_token,
                                             base::TimeDelta time_waited) {
  NOTREACHED() << "Not implemented.";
}

void WvrManager::UpdateLayerBounds(int16_t frame_index,
                                   const gfx::RectF& left_bounds,
                                   const gfx::RectF& right_bounds,
                                   const gfx::Size& source_size) {
  CreateOrResizeWebXrSurface(source_size);
}

}  // namespace wolvic
