// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_manager.h"

#include "base/task/bind_post_task.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "wolvic/browser/vr/wvr_api.h"


namespace wolvic {

namespace {

void WvrMatToTransform(const std::array<float, 16>& in, gfx::Transform* out) {
  *out = gfx::Transform::ColMajor(in[0], in[1], in[2], in[3], in[4], in[5],
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

device::mojom::XRHandedness ToXRHandness(mozilla::gfx::ControllerHand hand) {
  using mozilla::gfx::ControllerHand;
  switch (hand) {
    case ControllerHand::Left:
      return device::mojom::XRHandedness::LEFT;
    case ControllerHand::Right:
      return device::mojom::XRHandedness::RIGHT;
    case ControllerHand::_empty:
    case ControllerHand::EndGuard_:
      // LEFT controller and RIGHT controller are currently the only supported
      // controllers. In the future, other controllers such as sound (which
      // does not have a handedness) will be added here.
      NOTREACHED();
      return device::mojom::XRHandedness::NONE;
  }
}

device::Gamepad ToGamepad(const mozilla::gfx::VRControllerState& controller) {
  DCHECK_GT(controller.numButtons, 0UL);
  DCHECK_LT(controller.numButtons, device::Gamepad::kButtonsLengthCap);

  // The xr-standard gamepad mapping is specified here
  // https://www.w3.org/TR/webxr-gamepads-module-1/#xr-standard-gamepad-mapping
  auto getGamepadButton = [&controller](int i) {
    return device::GamepadButton(controller.buttonPressed & (1 << i),
                                 controller.buttonTouched & (1 << i),
                                 controller.triggerValue[i]);
  };

  using device::GamepadBuilder;
  auto getAxesData =
      [&controller](
          GamepadBuilder::ButtonData::Type type) -> GamepadBuilder::ButtonData {
    DCHECK_LT(controller.numAxes, device::Gamepad::kAxesLengthCap);
    DCHECK_EQ(controller.numAxes, 4UL);

    bool isThumbstick = type == GamepadBuilder::ButtonData::Type::kThumbstick;
    DCHECK(type == GamepadBuilder::ButtonData::Type::kTouchpad || isThumbstick);

    GamepadBuilder::ButtonData data = {
        .type = type,
        .x_axis = controller.axisValue[isThumbstick ? 2 : 0],
        .y_axis = controller.axisValue[isThumbstick ? 3 : 1]};
    int buttonIndex = isThumbstick ? 3 : 2;
    data.touched = controller.buttonTouched & (1 << buttonIndex);
    data.pressed = controller.buttonPressed & (1 << buttonIndex);
    data.value = controller.triggerValue[buttonIndex];
    return data;
  };

  device::XRStandardGamepadBuilder builder(ToXRHandness(controller.hand));
  builder.SetPrimaryButton(getGamepadButton(0));

  if (controller.numButtons > 1) {
    builder.SetSecondaryButton(getGamepadButton(1));
  }

  if (controller.numButtons > 2) {
    builder.SetTouchpadData(
        getAxesData(GamepadBuilder::ButtonData::Type::kTouchpad));
  }

  if (controller.numButtons > 3) {
    builder.SetThumbstickData(
        getAxesData(GamepadBuilder::ButtonData::Type::kThumbstick));
  }

  if (controller.numButtons > 4) {
    for (uint32_t i = 4; i < controller.numButtons; ++i) {
      builder.AddOptionalButtonData(getGamepadButton(i));
    }
  }

  // TODO: Fill Gamepad data when there are no controllers, e.g during hand-tracking
  //       https://www.w3.org/TR/webxr-gamepads-module-1/#gamepad-api-integration

  return builder.GetGamepad().value();
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
    view->mojo_from_view = head_mat * eye_from_head;
  }
  return view;
}

gfx::Size GetSuggestedFrameSize(WvrApi* wvr_api) {
  // Make sure we're fetching an up to date state.
  wvr_api->PullSystemState();

  const mozilla::gfx::VRDisplayState& ds =
      wvr_api->get_system_state().displayState;

  // We compute the suggested frame size based on eye resolution.
  gfx::Size frame_size(ds.eyeResolution.width * 2, ds.eyeResolution.height);
  return frame_size;
}

std::vector<device::mojom::XRViewPtr> CreateViews(
    WvrApi* wvr_api, const mozilla::gfx::VRPose* pose) {
  const mozilla::gfx::VRDisplayState& display_state =
    wvr_api->get_system_state().displayState;
  gfx::Size suggested_size = GetSuggestedFrameSize(wvr_api);

  std::vector<device::mojom::XRViewPtr> views(2);
  views[0] = CreateView(mozilla::gfx::VRDisplayState::Eye::Eye_Left,
                        display_state, suggested_size, pose);
  views[1] = CreateView(mozilla::gfx::VRDisplayState::Eye::Eye_Right,
                        display_state, suggested_size, pose);
  return views;
}

bool ValidateRect(const gfx::RectF& bounds) {
  // Bounds should be between 0 and 1, with positive width/height.
  // We simply clamp to [0,1], but still validate that the bounds are not NAN.
  return !std::isnan(bounds.width()) && !std::isnan(bounds.height()) &&
         !std::isnan(bounds.x()) && !std::isnan(bounds.y());
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

static bool
supportsBlendMode(mozilla::gfx::VRDisplayState& display_state, const mozilla::gfx::VRDisplayBlendMode blend_mode) {
  for (auto& supported_blend_mode : display_state.blendModes) {
    if (supported_blend_mode == mozilla::gfx::VRDisplayBlendMode::_empty)
      return false;
    if (supported_blend_mode == blend_mode)
      return true;
  }
  return false;
}

void WvrManager::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) {
  DCHECK(IsOnWvrThread());
  ClosePresentationBindings();

  std::vector<device::mojom::XRViewPtr> views =
      CreateViews(wvr_api_, nullptr /*pose*/);
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

  auto toMojoBlendMode = [](mozilla::gfx::VRDisplayBlendMode mode) {
    switch (mode) {
      case mozilla::gfx::VRDisplayBlendMode::Opaque:
        return device::mojom::XREnvironmentBlendMode::kOpaque;
      case mozilla::gfx::VRDisplayBlendMode::AlphaBlend:
        return device::mojom::XREnvironmentBlendMode::kAlphaBlend;
      case mozilla::gfx::VRDisplayBlendMode::Additive:
        return device::mojom::XREnvironmentBlendMode::kAdditive;
      case mozilla::gfx::VRDisplayBlendMode::_empty:
        NOTREACHED();
        return device::mojom::XREnvironmentBlendMode::kOpaque;
    }
  };

  config->views = std::move(views);
  config->supports_viewport_scaling = true;
  auto session_blend_mode = PickEnvironmentBlendModeForSession(options->mode);
  session->enviroment_blend_mode = toMojoBlendMode(session_blend_mode);
  auto display_state = wvr_api_->get_system_state().displayState;
  config->default_framebuffer_scale = display_state.nativeFramebufferScaleFactor;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  blend_mode_ = supportsBlendMode(display_state, session_blend_mode) ? session_blend_mode : display_state.blendModes[0];

  auto toGfxSessionType = [](device::mojom::XRSessionMode mode) {
    switch (mode) {
      case device::mojom::XRSessionMode::kImmersiveVr:
        return mozilla::gfx::ImmersiveXRSessionType::VR;
      case device::mojom::XRSessionMode::kImmersiveAr:
        return mozilla::gfx::ImmersiveXRSessionType::AR;
      case device::mojom::XRSessionMode::kInline:
        NOTREACHED();
        return mozilla::gfx::ImmersiveXRSessionType::VR;
    }
  };
  session_type_ = toGfxSessionType(options->mode);

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

  is_frame_submmitted_ = true;

  // This event should only occur in response to a SwapBuffers from
  // an incoming SubmitFrame call.
  DCHECK(!pending_frames_.empty()) << ": Frame arrived before SubmitFrame";

  // LIFECYCLE: we should have exactly one pending frame. This is true
  // even after exiting a session with a not-yet-surfaced frame.
  DCHECK_EQ(pending_frames_.size(), 1U);

  int frame_index = pending_frames_.front();
  DVLOG(2) << __func__ << "frame: " << frame_index;
  pending_frames_.pop();

  // LIFECYCLE: we should be in processing state.
  DCHECK(webxr_.HaveProcessingFrame());
  device::WebXrFrame* processing_frame = webxr_.GetProcessingFrame();

  // Frame should be locked. Unlock it.
  DCHECK(processing_frame->state_locked);
  processing_frame->state_locked = false;

  // Continue with submit immediately.
  DrawFrameSubmitNow(processing_frame);
}

void WvrManager::ClosePresentationBindings() {
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
    const mozilla::gfx::VRControllerState& controller,
    bool add_hand_profile) {
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
    case mozilla::gfx::VRControllerType::MetaQuest3:
      input_source->description->profiles = {
          "meta-quest-touch-plus", "oculus-touch-v3", "oculus-touch",
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
    case mozilla::gfx::VRControllerType::PicoNeo3:
      input_source->description->profiles = {
          "pico-neo3", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::Pico4:
      input_source->description->profiles = {
          "pico-4", "generic-trigger-squeeze-thumbstick"};
      break;
    case mozilla::gfx::VRControllerType::PicoGaze:
    case mozilla::gfx::VRControllerType::_empty:
      input_source->description->profiles = {"generic-button"};
      break;
    case mozilla::gfx::VRControllerType::MagicLeap2:
      input_source->description->profiles = {
	"magicleap-one", "generic-trigger-squeeze-touchpad"};
      break;
    case mozilla::gfx::VRControllerType::_end:
      NOTREACHED();
      break;
  };

  if (add_hand_profile)
    input_source->description->profiles.insert(input_source->description->profiles.begin(), "generic-hand");
}

device::mojom::XRHandTrackingDataPtr
GetHandTrackingData(const mozilla::gfx::VRControllerState& controller) {
  if (!controller.hasHandTrackingData)
    return nullptr;

  auto hand_tracking_data = device::mojom::XRHandTrackingData::New();

  hand_tracking_data->hand_joint_data.resize(mozilla::gfx::kHandTrackingNumJoints);

  for (uint32_t i = 0; i < mozilla::gfx::kHandTrackingNumJoints; i++) {
    auto hand_joint_dst = device::mojom::XRHandJointData::New();
    auto& hand_joint_src = controller.handTrackingData.handJointData[i];

    hand_joint_dst->joint = static_cast<device::mojom::XRHandJoint>(i);

    gfx::Transform transform;
    WvrMatToTransform(hand_joint_src.transform, &transform);
    hand_joint_dst->mojo_from_joint.emplace(transform);

    hand_joint_dst->radius = hand_joint_src.radius;

    hand_tracking_data->hand_joint_data[i] = std::move(hand_joint_dst);
  }

  return hand_tracking_data;
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
    input_source->primary_input_pressed = controller.buttonPressed & 1ULL;
    input_source->primary_input_clicked =
        (controller_state_[i].buttonPressed & 1ULL) &&
        !input_source->primary_input_pressed;

    if (controller.numButtons > 1) {
      input_source->primary_squeeze_pressed =
          controller.buttonPressed & (1ULL << 1);
      input_source->primary_squeeze_clicked =
          (controller_state_[i].buttonPressed & (1ULL << 1)) &&
          !input_source->primary_squeeze_pressed;
    }

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

    PopulateProfiles(input_source, controller, controller.hasHandTrackingData);

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

    input_source->hand_tracking_data = GetHandTrackingData(controller);

    input_sources.push_back(std::move(input_source));
  }

  controller_state_ = wvr_api_->get_system_state().controllerState;
  return input_sources;
}

void WvrManager::DrawFrameSubmitNow(device::WebXrFrame* processing_frame) {
  // Report rendering completion to the Renderer so that it's permitted to
  // submit a fresh frame. We could do this earlier, as soon as the frame
  // got pulled off the transfer surface, but that results in overstuffed
  // buffers.

  // Renderer is waiting for the previous frame to render, unblock it now.
  submit_client_->OnSubmitFrameRendered();

  if (webxr_.HaveRenderingFrame())
    webxr_.EndFrameRendering();
  webxr_.TransitionFrameProcessingToRendering();

  // See if we can animate a new WebXR frame.
  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }
}

bool WvrManager::WebXrCanAnimateFrame() {
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

  return true;
}

void WvrManager::GetFrameData(
    device::mojom::XRFrameDataRequestOptionsPtr options,
    device::mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (!WebXrCanAnimateFrame()) {
    // We bind this as a post task so that whatever processing is run when we
    // attempt to get new frame data can complete before the pending
    // GetFrameData call actually happens.
    DCHECK(pending_getframedata_.is_null());
    pending_getframedata_ = base::BindPostTask(
        task_runner_, base::BindOnce(&WvrManager::GetFrameData,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(options), std::move(callback)));
    return;
  }

  get_frame_data_callback_ = std::move(callback);
  WebXrTryStartAnimatingFrame();
}

void WvrManager::WebXrTryStartAnimatingFrame() {
  DCHECK(IsOnWvrThread());

  device::mojom::XRFrameDataPtr frame_data = device::mojom::XRFrameData::New();
  frame_data->frame_id = webxr_.StartFrameAnimating();

  // Process all events.
  if (!SubmitFrameInternal(frame_data->frame_id))
   return;

  base::TimeTicks now = base::TimeTicks::Now();
  mozilla::gfx::VRSystemState system_state = wvr_api_->get_system_state();
  const mozilla::gfx::VRPose* pose = &system_state.sensorState.pose;

  frame_data->views = CreateViews(wvr_api_, pose);
  frame_data->input_state = GetInputSourceState();

  frame_data->mojo_from_viewer = PoseToVRPosePtr(pose);

  frame_data->time_delta = now - base::TimeTicks();

  // On 3D graphics the translation on a 4x4 transform matrix is stored in the first 3 values of
  // the 4th column. This means that (x,y,z) are on indexes (12, 13, 14).
  auto floor_height = system_state.displayState.sittingToStandingTransform[13];
  if (floor_height_ != floor_height) {
    frame_data->stage_parameters_id = ++stage_parameters_id_;
    floor_height_ = floor_height;
  }

  frame_data->stage_parameters = device::mojom::VRStageParameters::New();
  frame_data->stage_parameters->mojo_from_floor = gfx::Transform();
  frame_data->stage_parameters->mojo_from_floor.Translate3d(0, -floor_height_, 0);

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

bool WvrManager::SubmitFrameInternal(int16_t frame_index) {
  DCHECK(IsOnWvrThread());

  // Force exit WebXR before pushing the frame if the presenting generation is
  // changed by stopping presenting in Wolvic.
  if (wvr_api_->PresentingGenerationChanged()) {
    if (!get_frame_data_callback_.is_null())
      std::move(get_frame_data_callback_).Run(nullptr);

    if (exit_vr_callback_)
      std::move(exit_vr_callback_).Run();
    DLOG(WARNING) << __func__
                  << "Presenting generation changed. Don't submit frame";
    return false;
  }

  if (!wvr_api_->SyncState(
          is_frame_submmitted_, graphics_->webxr_texture_handle(),
          graphics_->webxr_surface_size(), blend_mode_, session_type_)) {
    DLOG(WARNING) << __func__
                  << "SyncState failed. Don't submit frame";
    return false;
  }

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
  is_frame_submmitted_ = false;
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

  if (pending_getframedata_) {
    std::move(pending_getframedata_).Run();
  }
}

void WvrManager::SubmitFrame(int16_t frame_index,
                             const gpu::MailboxHolder& mailbox,
                             base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  webxr_.ProcessOrDefer(
      base::BindOnce(&WvrManager::ProcessWebXrFrameFromMailbox,
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

void WvrManager::ProcessWebXrFrameFromMailbox(
    int16_t frame_index,
    const gpu::MailboxHolder& mailbox) {
  // LIFECYCLE: pending_frames_ should be empty when there's no processing
  // frame. It gets one element here, and then is emptied again before leaving
  // processing state. Swapping twice on a Surface without calling
  // updateTexImage in between can lose frames, so don't draw+swap if we
  // already have a pending frame we haven't consumed yet.
  DCHECK(pending_frames_.empty());

  // LIFECYCLE: We shouldn't have gotten here unless mailbox_bridge_ is ready.
  DCHECK(webxr_.mailbox_bridge_ready());

  // Don't allow any state changes for this processing frame until it
  // arrives on the Surface. See OnWebXrFrameAvailable.
  DCHECK(webxr_.HaveProcessingFrame());
  webxr_.GetProcessingFrame()->state_locked = true;

  bool swapped = mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox, gfx::Transform());
  DCHECK(swapped);
  // Tell OnWebXrFrameAvailable to expect a new frame to arrive on
  // the SurfaceTexture, and save the associated frame index.
  pending_frames_.emplace(frame_index);

  // LIFECYCLE: we should have a pending frame now.
  DCHECK_EQ(pending_frames_.size(), 1U);

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);
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
  if (!ValidateRect(left_bounds) || !ValidateRect(right_bounds)) {
    presentation_receiver_.ReportBadMessage(
        "UpdateLayerBounds called with invalid bounds");
    ClosePresentationBindings();
    return;
  }

  if (frame_index >= 0 && !webxr_.HaveAnimatingFrame()) {
    // The optional UpdateLayerBounds call must happen before SubmitFrame.
    presentation_receiver_.ReportBadMessage(
        "UpdateLayerBounds called without animating frame");
    ClosePresentationBindings();
    return;
  }

  CreateOrResizeWebXrSurface(source_size);
}

// Returns the blend mode that should be used by the WebXR session depending on
// the display technology (i.e. the supported blend modes). See
// https://www.w3.org/TR/webxr-ar-module-1/#xr-compositor-behaviors
mozilla::gfx::VRDisplayBlendMode
WvrManager::PickEnvironmentBlendModeForSession(
    device::mojom::XRSessionMode session_mode) {
  auto system_state = wvr_api_->get_system_state();
  DCHECK(system_state.enumerationCompleted);

  auto& display_state = system_state.displayState;

  // Additive displays must use this regardless of the mode
  if (supportsBlendMode(display_state, mozilla::gfx::VRDisplayBlendMode::Additive))
    return mozilla::gfx::VRDisplayBlendMode::Additive;

  switch (session_mode) {
    case device::mojom::XRSessionMode::kImmersiveVr:
    case device::mojom::XRSessionMode::kInline:
      return mozilla::gfx::VRDisplayBlendMode::Opaque;
    case device::mojom::XRSessionMode::kImmersiveAr:
      // Even if the device does not support the blend mode we should pass an AR blend
      // mode to the Web Engine to let it add the alpha channel. Note that the device
      // might use a compositor layer to enable AR instead of a blend mode (like Meta).
      return mozilla::gfx::VRDisplayBlendMode::AlphaBlend;
  }
}

}  // namespace wolvic
