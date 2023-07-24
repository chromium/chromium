// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_manager.h"

#include "base/android/jni_string.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "wolvic/jni_headers/VRManager_jni.h"
#include "wolvic/jni_headers/WVRSurfaceTexture_jni.h"

namespace wolvic {

namespace {

const int64_t kFrameTimeOutMilliseconds = 1000;

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

  size_t num_buttons = controller.numButtons;
  if (num_buttons > device::Gamepad::kButtonsLengthCap) {
    num_buttons = device::Gamepad::kButtonsLengthCap;
    DLOG(WARNING) << "Controller has too many buttons, truncating to "
                  << num_buttons;
  }

  gamepad.buttons_length = num_buttons;
  for (uint32_t i = 0; i < controller.numButtons; ++i) {
    gamepad.buttons[i].pressed = controller.buttonPressed & (1 << i);
    gamepad.buttons[i].touched = controller.buttonTouched & (1 << i);
    gamepad.buttons[i].value = controller.triggerValue[i];
  }

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

int32_t GetNextTextureHandleId() {
  static int32_t s_next_texture_handle_id = 0;
  if (s_next_texture_handle_id == std::numeric_limits<int32_t>::max())
    s_next_texture_handle_id = 0;
  return ++s_next_texture_handle_id;
}

}  // namespace

WvrManager::WvrManager()
    : texture_handle_id_(GetNextTextureHandleId()),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      webxr_(std::make_unique<device::WebXrPresentationState>()) {
  JNIEnv* env = base::android::AttachCurrentThread();
  shmem_ = reinterpret_cast<mozilla::gfx::VRExternalShmem*>(
      content::Java_VRManager_getExternalContext(env));
  memset((void*)&browser_state_, 0, sizeof(mozilla::gfx::VRBrowserState));
  memset((void*)&system_state_, 0, sizeof(mozilla::gfx::VRSystemState));
}

WvrManager::~WvrManager() {
  ClosePresentationBindings();
  ExitWebXRPresentation(base::NullCallback());
  webxr_->EndPresentation();

  if (j_surface_texture_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_WVRSurfaceTexture_release(env, j_surface_texture_);
  }
}

void WvrManager::InitializeGl(const gfx::Size& frame_size,
                              base::OnceClosure callback) {
  screen_size_ = frame_size;

  gl::init::DisableANGLE();

  gl::GLDisplay* display = nullptr;
  if (gl::GetGLImplementation() == gl::kGLImplementationNone) {
    display = gl::init::InitializeGLOneOff(/*system_device_id=*/0);
    if (!display) {
      LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
      return;
    }
  } else {
    display = gl::GetDefaultDisplayEGL();
  }

  surface_ = gl::init::CreateOffscreenGLSurface(display, gfx::Size());

  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    return;
  }

  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    return;
  }
  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glGenTextures(1, &texture_id_);

  std::move(callback).Run();
}

void WvrManager::StartWebXRPresentation(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
    base::OnceClosure exit_callback) {
  DCHECK(IsOnWvrThread());

  exit_vr_callback_ = std::move(exit_callback);

  // Indicate that we are ready to start immersive mode
  browser_state_.presentationActive = true;
  browser_state_.layerState[0].type =
      mozilla::gfx::VRLayerType::LayerType_Stereo_Immersive;
  PushState();

  PullState([&]() {
    return !system_state_.displayState.suppressFrames &&
           system_state_.displayState.isConnected;
  });

  presenting_generation_ = system_state_.displayState.presentingGeneration;

  ConnectPresentingService(std::move(options), std::move(callback));
}

void WvrManager::ExitWebXRPresentation(base::OnceClosure callback) {
  DCHECK(IsOnWvrThread());

  browser_state_.presentationActive = false;
  browser_state_.layerState[0].type =
      mozilla::gfx::VRLayerType::LayerType_None;
  PushState(true);

  if (callback)
    std::move(callback).Run();
}

bool WvrManager::IsOnWvrThread() const {
  return task_runner_->BelongsToCurrentThread();
}

void WvrManager::CreateOrResizeWebXrSurface(const gfx::Size& size) {
  DCHECK(IsOnWvrThread());
  if (!surface_texture_) {
    surface_texture_ = gl::SurfaceTexture::Create(texture_id_);
    surface_texture_->SetFrameAvailableCallback(
        base::BindRepeating(&WvrManager::OnWebXrFrameAvailable, GetWeakPtr()));
  }

  if (!j_surface_texture_) {
    JNIEnv* env = base::android::AttachCurrentThread();
    j_surface_texture_ = Java_WVRSurfaceTexture_create(
        env, texture_handle_id_, surface_texture_->j_surface_texture());
  }

  surface_size_ = size;
  surface_texture_->SetDefaultBufferSize(surface_size_.width(),
                                         surface_size_.height());

  if (mailbox_bridge_)
    mailbox_bridge_->ResizeSurface(size.width(), size.height());
  else
    CreateSurfaceBridge();
}

void WvrManager::OnGpuProcessConnectionReady() {
  CHECK(mailbox_bridge_);

  DCHECK(!webxr_->HaveAnimatingFrame());
  webxr_->NotifyMailboxBridgeReady();
}

void WvrManager::CreateSurfaceBridge() {
  DCHECK(!mailbox_bridge_);
  DCHECK(surface_texture_);
  mailbox_bridge_ = std::make_unique<webxr::MailboxToSurfaceBridgeImpl>();
  mailbox_bridge_->CreateSurface(surface_texture_.get());
  mailbox_bridge_->CreateAndBindContextProvider(
      base::BindOnce(&WvrManager::OnGpuProcessConnectionReady, GetWeakPtr()));
}

void WvrManager::ConnectPresentingService(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    base::OnceCallback<void(device::mojom::XRSessionPtr)> callback) {
  DCHECK(IsOnWvrThread());
  ClosePresentationBindings();

  std::vector<device::mojom::XRViewPtr> views =
      CreateViews(system_state_.displayState, nullptr /*pose*/, screen_size_);
  int width = 0;
  int height = 0;
  for (const auto& view : views) {
    width += view->viewport.width();
    height = std::max(height, view->viewport.height());
  }

  CreateOrResizeWebXrSurface({width, height});

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_receiver =
      submit_client_.BindNewPipeAndPassReceiver();

  submit_client_.set_disconnect_handler(base::BindOnce(
      &WvrManager::OnSubmitClientMojoConnectionError, GetWeakPtr()));

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  transport_options->transport_method =
      device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
  transport_options->wait_for_transfer_notification = true;
  transport_options->wait_for_render_notification = true;
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
      system_state_.displayState.nativeFramebufferScaleFactor;
  session->interaction_mode = device::mojom::XRInteractionMode::kScreenSpace;

  std::move(callback).Run(std::move(session));
}

void WvrManager::OnWebXrFrameAvailable() {
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();

  // The processing frame would be empty when this method is called again from
  // Android system after OnWebXrTimedOut.
  if (webxr_->HaveProcessingFrame()) {
    // Frame should be locked. Unlock it.
    DCHECK(webxr_->GetProcessingFrame()->state_locked);
    webxr_->GetProcessingFrame()->state_locked = false;

    if (!SubmitFrameInternal(webxr_->GetProcessingFrame()->index))
      return;

    if (webxr_->HaveRenderingFrame())
      webxr_->EndFrameRendering();
    webxr_->TransitionFrameProcessingToRendering();
  }

  // Renderer is waiting for the previous frame to render, unblock it now.
  submit_client_->OnSubmitFrameRendered();

  TryStartAnimatingFrame();
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

std::vector<device::mojom::XRInputSourceStatePtr>
WvrManager::GetInputSourceState() {
  std::vector<device::mojom::XRInputSourceStatePtr> input_sources;

  for (uint32_t i = 0; i < mozilla::gfx::kVRControllerMaxCount; ++i) {
    const auto& controller = system_state_.controllerState[i];
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

    // TODO: Get from external
    input_source->description->profiles = {
        "oculus-touch-v3", "oculus-touch-v2", "oculus-touch",
        "generic-trigger-squeeze-thumbstick"};

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

bool WvrManager::CanStartNewAnimatingFrame() {
  if (webxr_->HaveAnimatingFrame()) {
    return false;
  }

  if (webxr_->HaveProcessingFrame()) {
    return false;
  }

  if (get_frame_data_callback_.is_null()) {
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
  TryStartAnimatingFrame();
}

void WvrManager::TryStartAnimatingFrame() {
  DCHECK(IsOnWvrThread());

  if (!CanStartNewAnimatingFrame()) {
    return;
  }

  device::mojom::XRFrameDataPtr frame_data = device::mojom::XRFrameData::New();
  frame_data->frame_id = webxr_->StartFrameAnimating();
  frame_data->views =
      CreateViews(system_state_.displayState, &system_state_.sensorState.pose,
                  surface_size_);

  frame_data->mojo_space_reset = true;

  frame_data->input_state = GetInputSourceState();

  frame_data->mojo_from_viewer =
      PoseToVRPosePtr(&system_state_.sensorState.pose);  // std::move(pose);

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
  if (presenting_generation_ != system_state_.displayState.presentingGeneration) {
    if (!get_frame_data_callback_.is_null())
      std::move(get_frame_data_callback_).Run(nullptr);

    if (exit_vr_callback_)
      std::move(exit_vr_callback_).Run();
    return false;
  }

  auto& layer = browser_state_.layerState[0].layer_stereo_immersive;
  layer.frameId = frame_index;
  layer.textureSize.width = surface_size_.width();
  layer.textureSize.height = surface_size_.height();
  layer.textureHandle = texture_handle_id_;

  // for (auto& view: views) {
  for (int i = 0; i < 2; ++i) {
    auto& externalRect = i == 0 ? layer.leftEyeRect : layer.rightEyeRect;

    // TODO : How to get this values?
    externalRect.x = i == 0 ? 0 : 0.5f;
    externalRect.y = 0;
    externalRect.width = 0.5f;
    externalRect.height = 1.0f;
  }

  last_frame_index_ = frame_index;
  PushState(true);
  PullState([this]() {
    return (system_state_.displayState.lastSubmittedFrameId ==
            last_frame_index_) ||
           system_state_.displayState.suppressFrames ||
           !system_state_.displayState.isConnected;
  });

  // Avoid racing texture between processing in chromium and consuming in
  // wolvic.
  layer.textureHandle = 0;
  PushState(true);

  return true;
}

bool WvrManager::IsSubmitFrameExpected(int16_t frame_index) {
  if (!submit_client_.get() || !webxr_->HaveAnimatingFrame())
    return false;

  device::WebXrFrame* animating_frame = webxr_->GetAnimatingFrame();
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

  if (webxr_->mailbox_bridge_ready())
    mailbox_bridge_->WaitSyncToken(sync_token);

  DCHECK(webxr_->HaveAnimatingFrame());
  webxr_->RecycleUnusedAnimatingFrame();
}

void WvrManager::SubmitFrame(int16_t frame_index,
                             const gpu::MailboxHolder& mailbox,
                             base::TimeDelta time_waited) {
  if (!IsSubmitFrameExpected(frame_index))
    return;

  webxr_->ProcessOrDefer(base::BindOnce(&WvrManager::ProcessFrameFromMailbox,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        frame_index, mailbox));
}

void WvrManager::ProcessFrameFromMailbox(int16_t frame_index,
                                         const gpu::MailboxHolder& mailbox) {
  DCHECK(webxr_->HaveProcessingFrame());

  webxr_->GetProcessingFrame()->state_locked = true;

  bool swapped = mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox);
  DCHECK(swapped);

  // Notify the client.
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

void WvrManager::PushState(bool notify_cond) {
  DCHECK(shmem_);

  if (pthread_mutex_lock((pthread_mutex_t*)&(shmem_->geckoMutex)) == 0) {
    memcpy((void*)&(shmem_->geckoState), (void*)&browser_state_,
           sizeof(mozilla::gfx::VRBrowserState));
    if (notify_cond)
      pthread_cond_signal((pthread_cond_t*)&(shmem_->geckoCond));
    pthread_mutex_unlock((pthread_mutex_t*)&(shmem_->geckoMutex));
  }
}

void WvrManager::PullState(const std::function<bool()>& wait_condition) {
  DCHECK(shmem_);

  bool done = false;
  while (!done) {
    if (pthread_mutex_lock((pthread_mutex_t*)&shmem_->systemMutex) == 0) {
      while (true) {
        memcpy(&system_state_, (void*)&shmem_->state,
               sizeof(mozilla::gfx::VRSystemState));
        if (!wait_condition || wait_condition()) {
          done = true;
          break;
        }
        // Block current thead using the condition variable until data
        // changes
        pthread_cond_wait((pthread_cond_t*)&shmem_->systemCond,
                          (pthread_mutex_t*)&shmem_->systemMutex);
      }
      pthread_mutex_unlock((pthread_mutex_t*)&(shmem_->systemMutex));
    } else if (!wait_condition) {
      // pthread_mutex_lock failed and we are not waiting for a condition to
      // exit from PullState call.
      return;
    }
  }
}

}  // namespace wolvic
