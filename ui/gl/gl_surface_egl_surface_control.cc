// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_egl_surface_control.h"

#include <utility>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/build_info.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/functional/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_plane_data.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gl/android/scoped_a_native_window.h"
#include "ui/gl/android/scoped_java_surface_control.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_utils.h"

namespace gl {
namespace {

constexpr char kRootSurfaceName[] = "ChromeNativeWindowSurface";
constexpr char kChildSurfaceName[] = "ChromeChildSurface";

gfx::Size GetBufferSize(const AHardwareBuffer* buffer) {
  AHardwareBuffer_Desc desc;
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  return gfx::Size(desc.width, desc.height);
}

std::string BuildSurfaceName(const char* suffix) {
  return base::StrCat(
      {base::android::BuildInfo::GetInstance()->package_name(), "/", suffix});
}

base::TimeTicks GetSignalTime(const base::ScopedFD& fence) {
  if (!fence.is_valid())
    return base::TimeTicks();

  base::TimeTicks signal_time;
  auto status = gfx::GpuFence::GetStatusChangeTime(fence.get(), &signal_time);
  if (status != gfx::GpuFence::kSignaled)
    return base::TimeTicks();

  return signal_time;
}

}  // namespace

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(
    gl::ScopedANativeWindow window,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : GLSurfaceEGLSurfaceControl(
          base::MakeRefCounted<gfx::SurfaceControl::Surface>(
              window.a_native_window(),
              BuildSurfaceName(kRootSurfaceName).c_str()),
          std::move(task_runner)) {}

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(
    gl::ScopedJavaSurfaceControl scoped_java_surface_control,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : GLSurfaceEGLSurfaceControl(scoped_java_surface_control.MakeSurface(),
                                 std::move(task_runner)) {}

GLSurfaceEGLSurfaceControl::GLSurfaceEGLSurfaceControl(
    scoped_refptr<gfx::SurfaceControl::Surface> root_surface,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : child_surface_name_(BuildSurfaceName(kChildSurfaceName)),
      root_surface_(std::move(root_surface)),
      transaction_ack_timeout_manager_(task_runner),
      gpu_task_runner_(std::move(task_runner)),
      use_target_deadline_(features::IsAndroidFrameDeadlineEnabled()),
      using_on_commit_callback_(!use_target_deadline_ &&
                                gfx::SurfaceControl::SupportsOnCommit()) {}

GLSurfaceEGLSurfaceControl::~GLSurfaceEGLSurfaceControl() {
  Destroy();
}

bool GLSurfaceEGLSurfaceControl::Initialize() {
  if (!root_surface_->surface())
    return false;

  return true;
}

void GLSurfaceEGLSurfaceControl::PreserveChildSurfaceControls() {
  TRACE_EVENT_INSTANT0(
      "gpu", "GLSurfaceEGLSurfaceControl::PreserveChildSurfaceControls",
      TRACE_EVENT_SCOPE_THREAD);
  preserve_children_ = true;
}

void GLSurfaceEGLSurfaceControl::Destroy() {
  TRACE_EVENT0("gpu", "GLSurfaceEGLSurfaceControl::Destroy");
  // Detach all child layers to prevent leaking unless browser asked us not too.
  if (!preserve_children_) {
    gfx::SurfaceControl::Transaction transaction;
    for (auto& surface : surface_list_) {
      transaction.SetParent(*surface.surface, nullptr);
    }
    transaction.Apply();
  }

  pending_transaction_.reset();
  surface_list_.clear();
  root_surface_.reset();
}

bool GLSurfaceEGLSurfaceControl::Resize(const gfx::Size& size,
                                        float scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha) {
  // TODO(khushalsagar): Update GLSurfaceFormat using the |color_space| above?
  // We don't do this for the NativeViewGLSurfaceEGL as well yet.
  return true;
}

void GLSurfaceEGLSurfaceControl::Present(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    gfx::FrameData data) {
  CommitPendingTransaction(std::move(completion_callback),
                           std::move(presentation_callback));
}

void GLSurfaceEGLSurfaceControl::CommitPendingTransaction(
    SwapCompletionCallback completion_callback,
    PresentationCallback present_callback) {
  // The transaction is initialized on the first ScheduleOverlayPlane call. If
  // we don't have a transaction at this point, it means the scheduling the
  // overlay plane failed. Simply report a swap failure to lose the context and
  // recreate the surface.
  if (!pending_transaction_ || surface_lost_) {
    LOG(ERROR) << "CommitPendingTransaction failed because surface is lost";

    surface_lost_ = true;
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    std::move(present_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // This is to workaround an Android bug where not specifying a damage region
  // is assumed to mean nothing is damaged. See crbug.com/993977.
  for (size_t i = 0; i < pending_surfaces_count_; ++i) {
    const auto& surface_state = surface_list_[i];
    if (!surface_state.hardware_buffer)
      continue;

    pending_transaction_->SetDamageRect(
        *surface_state.surface,
        gfx::Rect(GetBufferSize(surface_state.hardware_buffer)));
  }

  // Surfaces which are present in the current frame but not in the next frame
  // need to be explicitly updated in order to get a release fence for them in
  // the next transaction.
  DCHECK_LE(pending_surfaces_count_, surface_list_.size());
  for (size_t i = pending_surfaces_count_; i < surface_list_.size(); ++i) {
    auto& surface_state = surface_list_[i];
    if (surface_state.hardware_buffer) {
      pending_transaction_->SetBuffer(*surface_state.surface, nullptr,
                                      base::ScopedFD());
      surface_state.hardware_buffer = nullptr;
    }
    if (surface_state.visibility) {
      pending_transaction_->SetVisibility(*surface_state.surface, false);
      surface_state.visibility = false;
    }
  }

  // TODO(khushalsagar): Consider using the SetDamageRect API for partial
  // invalidations. Note that the damage rect set should be in the space in
  // which the content is rendered (including the pre-transform). See
  // crbug.com/988857 for details.

  // Release resources for the current frame once the next frame is acked.
  ResourceRefs resources_to_release;
  resources_to_release.swap(current_frame_resources_);
  current_frame_resources_.clear();

  // Track resources to be owned by the framework after this transaction.
  current_frame_resources_.swap(pending_frame_resources_);
  pending_frame_resources_.clear();

  OnTransactionAckArgs::SequenceId ack_id =
      pending_transaction_ack_id_generator_.GenerateNextId();
  pending_transaction_acks_.emplace_back(
      ack_id, std::move(completion_callback), std::move(present_callback),
      std::move(resources_to_release), std::move(primary_plane_fences_));
  gfx::SurfaceControl::Transaction::OnCompleteCb complete_cb =
      base::BindOnce(&GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread,
                     weak_factory_.GetWeakPtr(), ack_id);
  primary_plane_fences_.reset();
  pending_transaction_->SetOnCompleteCb(std::move(complete_cb),
                                        gpu_task_runner_);

  if (use_target_deadline_) {
    DCHECK(!!choreographer_vsync_id_for_next_frame_);
    DCHECK(gfx::SurfaceControl::SupportsSetFrameTimeline());
    pending_transaction_->SetFrameTimelineId(
        choreographer_vsync_id_for_next_frame_.value());
    choreographer_vsync_id_for_next_frame_.reset();
  }

  if (using_on_commit_callback_) {
    gfx::SurfaceControl::Transaction::OnCommitCb commit_cb = base::BindOnce(
        &GLSurfaceEGLSurfaceControl::OnTransactionCommittedOnGpuThread,
        weak_factory_.GetWeakPtr());
    pending_transaction_->SetOnCommitCb(std::move(commit_cb), gpu_task_runner_);
  }

  pending_surfaces_count_ = 0u;
  frame_rate_update_pending_ = false;

  if (num_transaction_commit_or_ack_pending_ && !use_target_deadline_) {
    pending_transaction_queue_.push(std::move(pending_transaction_).value());
  } else {
    num_transaction_commit_or_ack_pending_++;
    pending_transaction_->Apply();
    transaction_ack_timeout_manager_.ScheduleHangDetection();
  }

  pending_transaction_.reset();
}

bool GLSurfaceEGLSurfaceControl::ScheduleOverlayPlane(
    OverlayImage image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (surface_lost_) {
    LOG(ERROR) << "ScheduleOverlayPlane failed because surface is lost";
    return false;
  }

  if (!pending_transaction_)
    pending_transaction_.emplace();

  bool uninitialized = false;
  if (pending_surfaces_count_ == surface_list_.size()) {
    uninitialized = true;
    surface_list_.emplace_back(*root_surface_, child_surface_name_);
  }
  pending_surfaces_count_++;
  auto& surface_state = surface_list_.at(pending_surfaces_count_ - 1);

  // Make the surface visible if its hidden or uninitialized..
  if (uninitialized || !surface_state.visibility) {
    pending_transaction_->SetVisibility(*surface_state.surface, true);
    surface_state.visibility = true;
  }

  if (uninitialized || surface_state.z_order != overlay_plane_data.z_order) {
    surface_state.z_order = overlay_plane_data.z_order;
    pending_transaction_->SetZOrder(*surface_state.surface,
                                    overlay_plane_data.z_order);
  }

  if (uninitialized && use_target_deadline_) {
    pending_transaction_->SetEnableBackPressure(*surface_state.surface, true);
  }

  AHardwareBuffer* hardware_buffer = nullptr;
  auto scoped_hardware_buffer = std::move(image);
  bool is_primary_plane = overlay_plane_data.is_root_overlay;
  if (scoped_hardware_buffer) {
    hardware_buffer = scoped_hardware_buffer->buffer();

    // We currently only promote the display compositor's buffer or a video
    // buffer to an overlay. So if this buffer is not for video then it implies
    // its the primary plane.
    DCHECK(!is_primary_plane || !primary_plane_fences_);
    if (is_primary_plane) {
      primary_plane_fences_.emplace();
      primary_plane_fences_->available_fence =
          scoped_hardware_buffer->TakeAvailableFence();
    }

    auto* a_surface = surface_state.surface->surface();
    DCHECK_EQ(pending_frame_resources_.count(a_surface), 0u);

    auto& resource_ref = pending_frame_resources_[a_surface];
    resource_ref.surface = surface_state.surface;
    resource_ref.scoped_buffer = std::move(scoped_hardware_buffer);
  }

  if (uninitialized || surface_state.hardware_buffer != hardware_buffer ||
      gpu_fence) {
    surface_state.hardware_buffer = hardware_buffer;

    base::ScopedFD fence_fd;
    if (gpu_fence && surface_state.hardware_buffer) {
      auto fence_handle = gpu_fence->GetGpuFenceHandle().Clone();
      DCHECK(!fence_handle.is_null());
      fence_fd = fence_handle.Release();
    }

    if (is_primary_plane) {
      primary_plane_fences_->ready_fence =
          base::ScopedFD(HANDLE_EINTR(dup(fence_fd.get())));
    }

    pending_transaction_->SetBuffer(*surface_state.surface,
                                    surface_state.hardware_buffer,
                                    std::move(fence_fd));
  }

  if (hardware_buffer) {
    gfx::Size buffer_size = GetBufferSize(hardware_buffer);
    gfx::RectF scaled_rect =
        gfx::ScaleRect(overlay_plane_data.crop_rect, buffer_size.width(),
                       buffer_size.height());

    gfx::Rect dst = gfx::ToNearestRect(overlay_plane_data.display_bounds);
    gfx::Rect src = gfx::ToEnclosedRect(scaled_rect);

    // When the video is being scrolled offscreen DisplayCompositor will crop it
    // to only visible portion and adjust crop_rect accordingly. When the video
    // is smaller than the surface is can lead to the crop rect being less than
    // a pixel in size. This adjusts the crop rect size to at least 1 pixel as
    // we want to stretch last visible pixel line/column in this case.
    // Note: We will do it even if crop_rect width/height is exact 0.0f. In
    // reality this should never happen and there is no way to display video
    // with empty crop rect, so display compositor should not request this.

    if (src.width() == 0) {
      src.set_width(1);
      if (src.right() > buffer_size.width())
        src.set_x(buffer_size.width() - 1);
    }
    if (src.height() == 0) {
      src.set_height(1);
      if (src.bottom() > buffer_size.height())
        src.set_y(buffer_size.height() - 1);
    }

    // When display compositor rounds up destination rect to integer coordinates
    // it becomes slightly bigger. After we adjust source rect accordingly, it
    // can become larger then a buffer so we clip it here. See crbug.com/1083412
    src.Intersect(gfx::Rect(buffer_size));

    auto transform =
        absl::get<gfx::OverlayTransform>(overlay_plane_data.plane_transform);
    if (uninitialized || surface_state.src != src || surface_state.dst != dst ||
        surface_state.transform != transform) {
      surface_state.src = src;
      surface_state.dst = dst;
      surface_state.transform = transform;
      pending_transaction_->SetGeometry(*surface_state.surface, src, dst,
                                        transform);
    }
  }

  bool opaque = !overlay_plane_data.enable_blend;
  if (uninitialized || surface_state.opaque != opaque) {
    surface_state.opaque = opaque;
    pending_transaction_->SetOpaque(*surface_state.surface, opaque);
  }

  const auto& image_color_space = overlay_plane_data.color_space;
  if (!gfx::SurfaceControl::SupportsColorSpace(image_color_space)) {
    LOG(DFATAL) << "Not supported color space used with overlay : "
                << image_color_space.ToString();
  }

  if (uninitialized || surface_state.color_space != image_color_space ||
      surface_state.hdr_metadata != overlay_plane_data.hdr_metadata) {
    surface_state.color_space = image_color_space;
    surface_state.hdr_metadata = overlay_plane_data.hdr_metadata;
    pending_transaction_->SetColorSpace(
        *surface_state.surface, image_color_space, surface_state.hdr_metadata);
  }

  if (frame_rate_update_pending_)
    pending_transaction_->SetFrameRate(*surface_state.surface, frame_rate_);

  return true;
}

bool GLSurfaceEGLSurfaceControl::SupportsPlaneGpuFences() const {
  return true;
}

void GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread(
    OnTransactionAckArgs::SequenceId id,
    gfx::SurfaceControl::TransactionStats transaction_stats) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  transaction_ack_timeout_manager_.OnTransactionAck();

  bool found = false;
  for (auto& args : pending_transaction_acks_) {
    if (args.id == id) {
      CHECK(!args.transaction_stats.has_value());
      found = true;
      args.transaction_stats = std::move(transaction_stats);
      break;
    }
  }
  CHECK(found);

  while (!pending_transaction_acks_.empty()) {
    auto& args = pending_transaction_acks_.front();
    if (!args.transaction_stats) {
      break;
    }
    OrderedOnTransactionAckOnGpuThread(
        std::move(args.completion_callback),
        std::move(args.presentation_callback),
        std::move(args.released_resources),
        std::move(args.primary_plane_fences),
        std::move(args.transaction_stats.value()));
    pending_transaction_acks_.pop_front();
  }
}

void GLSurfaceEGLSurfaceControl::OrderedOnTransactionAckOnGpuThread(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    ResourceRefs released_resources,
    std::optional<PrimaryPlaneFences> primary_plane_fences,
    gfx::SurfaceControl::TransactionStats transaction_stats) {
  TRACE_EVENT0("gpu",
               "GLSurfaceEGLSurfaceControl::OnTransactionAckOnGpuThread");

  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  for (auto& surface_stat : transaction_stats.surface_stats) {
    auto it = released_resources.find(surface_stat.surface);

    // The transaction ack includes data for all surfaces updated in this
    // transaction. So the following condition can occur if a new surface was
    // added in this transaction with a buffer. It'll be included in the ack
    // with no fence, since its not being released and so shouldn't be in
    // |released_resources| either.
    if (it == released_resources.end()) {
      // TODO(vasilyt): We used to DCHECK(!surface_stat.fence.is_valid()) here,
      // but due to flinger behavior it doesn't hold. This seems to be a
      // potential fligner bug.  DCHECK is useful for catching resource
      // life-time issues, so we should consider bringing it back when Android
      // side will be fixed.
      continue;
    }

    if (surface_stat.fence.is_valid()) {
      it->second.scoped_buffer->SetReadFence(std::move(surface_stat.fence));
    }
  }

  // Note that we may not see |surface_stats| for every resource above. This is
  // because we take a ref on every buffer used in a frame, even if it is not
  // updated in that frame. Since the transaction ack only includes surfaces
  // which were updated in that transaction, the surfaces with no buffer updates
  // won't be present in the ack.
  released_resources.clear();

  // The presentation feedback callback must run after swap completion.
  std::move(completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));

  PendingPresentationCallback pending_cb;
  if (primary_plane_fences) {
    pending_cb.available_time =
        GetSignalTime(primary_plane_fences->available_fence);
    pending_cb.ready_time = GetSignalTime(primary_plane_fences->ready_fence);
  }
  pending_cb.latch_time = transaction_stats.latch_time;
  pending_cb.present_fence = std::move(transaction_stats.present_fence);
  pending_cb.callback = std::move(presentation_callback);
  pending_presentation_callback_queue_.push(std::move(pending_cb));

  if (!using_on_commit_callback_) {
    CHECK(num_transaction_commit_or_ack_pending_);
    num_transaction_commit_or_ack_pending_--;
  }
  CheckPendingPresentationCallbacks();

  // If we don't use OnCommit, we advance transaction queue after we received
  // OnComplete.
  if (!use_target_deadline_ && !using_on_commit_callback_)
    AdvanceTransactionQueue();
}

void GLSurfaceEGLSurfaceControl::OnTransactionCommittedOnGpuThread() {
  TRACE_EVENT0("gpu",
               "GLSurfaceEGLSurfaceControl::OnTransactionCommittedOnGpuThread");
  DCHECK(using_on_commit_callback_);
  CHECK(num_transaction_commit_or_ack_pending_);
  num_transaction_commit_or_ack_pending_--;
  AdvanceTransactionQueue();
}

void GLSurfaceEGLSurfaceControl::AdvanceTransactionQueue() {
  if (!pending_transaction_queue_.empty()) {
    num_transaction_commit_or_ack_pending_++;
    pending_transaction_queue_.front().Apply();
    pending_transaction_queue_.pop();
    transaction_ack_timeout_manager_.ScheduleHangDetection();
  }
}

void GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks() {
  TRACE_EVENT0("gpu",
               "GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks");
  check_pending_presentation_callback_queue_task_.Cancel();

  while (!pending_presentation_callback_queue_.empty()) {
    auto& pending_cb = pending_presentation_callback_queue_.front();

    base::TimeTicks signal_time;
    auto status = pending_cb.present_fence.is_valid()
                      ? gfx::GpuFence::GetStatusChangeTime(
                            pending_cb.present_fence.get(), &signal_time)
                      : gfx::GpuFence::kInvalid;
    if (status == gfx::GpuFence::kNotSignaled)
      break;

    auto flags = gfx::PresentationFeedback::kHWCompletion |
                 gfx::PresentationFeedback::kVSync;
    if (status == gfx::GpuFence::kInvalid) {
      signal_time = pending_cb.latch_time;
      flags = 0u;
    }

    TRACE_EVENT_INSTANT0(
        "gpu",
        "GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks - "
        "presentation_feedback",
        TRACE_EVENT_SCOPE_THREAD);
    gfx::PresentationFeedback feedback(signal_time, base::TimeDelta(), flags);
    feedback.available_timestamp = pending_cb.available_time;
    feedback.ready_timestamp = pending_cb.ready_time;
    feedback.latch_timestamp = pending_cb.latch_time;

    std::move(pending_cb.callback).Run(feedback);
    pending_presentation_callback_queue_.pop();
  }

  // If there are unsignaled fences and we don't have any pending transactions,
  // schedule a task to poll the fences again. If there is a pending transaction
  // already, then we'll poll when that transaction is acked.
  // Note this check is interested in pending ack, not pending commit. However
  // pending commit always implies pending ack, so there is no false negative
  // where a recheck is necessary but not posted.
  if (!pending_presentation_callback_queue_.empty() &&
      pending_transaction_queue_.empty() &&
      !num_transaction_commit_or_ack_pending_) {
    check_pending_presentation_callback_queue_task_.Reset(base::BindOnce(
        &GLSurfaceEGLSurfaceControl::CheckPendingPresentationCallbacks,
        weak_factory_.GetWeakPtr()));
    gpu_task_runner_->PostDelayedTask(
        FROM_HERE, check_pending_presentation_callback_queue_task_.callback(),
        base::Seconds(1) / 60);
  }
}

void GLSurfaceEGLSurfaceControl::SetFrameRate(float frame_rate) {
  if (frame_rate_ == frame_rate)
    return;

  frame_rate_ = frame_rate;
  frame_rate_update_pending_ = true;
}

void GLSurfaceEGLSurfaceControl::SetChoreographerVsyncIdForNextFrame(
    std::optional<int64_t> choreographer_vsync_id) {
  choreographer_vsync_id_for_next_frame_ = choreographer_vsync_id;
}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(
    const gfx::SurfaceControl::Surface& parent,
    const std::string& name)
    : surface(new gfx::SurfaceControl::Surface(parent, name.c_str())) {}

GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState() = default;
GLSurfaceEGLSurfaceControl::SurfaceState::SurfaceState(SurfaceState&& other) =
    default;
GLSurfaceEGLSurfaceControl::SurfaceState&
GLSurfaceEGLSurfaceControl::SurfaceState::operator=(SurfaceState&& other) =
    default;

GLSurfaceEGLSurfaceControl::SurfaceState::~SurfaceState() = default;

GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::~ResourceRef() = default;
GLSurfaceEGLSurfaceControl::ResourceRef::ResourceRef(ResourceRef&& other) =
    default;
GLSurfaceEGLSurfaceControl::ResourceRef&
GLSurfaceEGLSurfaceControl::ResourceRef::operator=(ResourceRef&& other) =
    default;

GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    PendingPresentationCallback() = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    ~PendingPresentationCallback() = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::
    PendingPresentationCallback(PendingPresentationCallback&& other) = default;
GLSurfaceEGLSurfaceControl::PendingPresentationCallback&
GLSurfaceEGLSurfaceControl::PendingPresentationCallback::operator=(
    PendingPresentationCallback&& other) = default;

GLSurfaceEGLSurfaceControl::PrimaryPlaneFences::PrimaryPlaneFences() = default;
GLSurfaceEGLSurfaceControl::PrimaryPlaneFences::~PrimaryPlaneFences() = default;
GLSurfaceEGLSurfaceControl::PrimaryPlaneFences::PrimaryPlaneFences(
    PrimaryPlaneFences&& other) = default;
GLSurfaceEGLSurfaceControl::PrimaryPlaneFences&
GLSurfaceEGLSurfaceControl::PrimaryPlaneFences::operator=(
    PrimaryPlaneFences&& other) = default;

GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
    TransactionAckTimeoutManager(
        scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : gpu_task_runner_(std::move(task_runner)) {}
GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
    ~TransactionAckTimeoutManager() = default;

void GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
    ScheduleHangDetection() {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  ++current_transaction_id_;
  if (!hang_detection_cb_.IsCancelled())
    return;

  constexpr int kIdleDelaySeconds = 5;
  hang_detection_cb_.Reset(
      base::BindOnce(&GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
                         OnTransactionTimeout,
                     base::Unretained(this), current_transaction_id_));
  gpu_task_runner_->PostDelayedTask(FROM_HERE, hang_detection_cb_.callback(),
                                    base::Seconds(kIdleDelaySeconds));
}

void GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
    OnTransactionAck() {
  // Since only one transaction is in flight at a time, an ack is for the latest
  // transaction.
  last_acked_transaction_id_ = current_transaction_id_;
}

void GLSurfaceEGLSurfaceControl::TransactionAckTimeoutManager::
    OnTransactionTimeout(TransactionId transaction_id) {
  hang_detection_cb_.Cancel();

  // If the last transaction was already acked, we do not need to schedule
  // any checks until a new transaction comes.
  if (current_transaction_id_ == last_acked_transaction_id_)
    return;

  // If more transactions have happened since the last task, schedule another
  // hang detection check.
  if (transaction_id < current_transaction_id_) {
    // Decrement the |current_transaction_id_| since ScheduleHangDetection()
    // will increment it again.
    --current_transaction_id_;
    ScheduleHangDetection();
    return;
  }

  LOG(ERROR) << "Transaction id " << transaction_id
             << " haven't received any ack from past 5 second which indicates "
                "it hanged";
}

GLSurfaceEGLSurfaceControl::OnTransactionAckArgs::OnTransactionAckArgs(
    SequenceId id,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback,
    ResourceRefs released_resources,
    std::optional<PrimaryPlaneFences> primary_plane_fences)
    : id(id),
      completion_callback(std::move(completion_callback)),
      presentation_callback(std::move(presentation_callback)),
      released_resources(std::move(released_resources)),
      primary_plane_fences(std::move(primary_plane_fences)) {}

GLSurfaceEGLSurfaceControl::OnTransactionAckArgs::OnTransactionAckArgs(
    OnTransactionAckArgs&& other) = default;
GLSurfaceEGLSurfaceControl::OnTransactionAckArgs&
GLSurfaceEGLSurfaceControl::OnTransactionAckArgs::operator=(
    GLSurfaceEGLSurfaceControl::OnTransactionAckArgs&& other) = default;
GLSurfaceEGLSurfaceControl::OnTransactionAckArgs::~OnTransactionAckArgs() =
    default;

}  // namespace gl
