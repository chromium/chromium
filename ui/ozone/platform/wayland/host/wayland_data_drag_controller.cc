// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"

#include <viewporter-client-protocol.h>

#include <bitset>
#include <cstdint>
#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_cursor_position.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_event_source.h"
#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {
namespace {

using mojom::DragEventSource;
using mojom::DragOperation;

DragOperation DndActionToDragOperation(uint32_t action) {
  // Prevent the usage of this function for an operation mask.
  DCHECK_LE(std::bitset<32>(action).count(), 1u);
  switch (action) {
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY:
      return DragOperation::kCopy;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE:
      return DragOperation::kMove;
    case WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK:
      // Unsupported in the browser.
      [[fallthrough]];
    default:
      return DragOperation::kNone;
  }
}

int DndActionsToDragOperations(uint32_t actions) {
  int operations = DragDropTypes::DRAG_NONE;
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) {
    operations |= DragDropTypes::DRAG_COPY;
  }
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) {
    operations |= DragDropTypes::DRAG_MOVE;
  }
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
    operations |= DragDropTypes::DRAG_LINK;
  }
  return operations;
}

uint32_t DragOperationsToDndActions(int operations) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  if (operations & DragDropTypes::DRAG_COPY) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  }
  if (operations & DragDropTypes::DRAG_MOVE) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  }
  return dnd_actions;
}

}  // namespace

WaylandDataDragController::WaylandDataDragController(
    WaylandConnection* connection,
    WaylandDataDeviceManager* data_device_manager,
    WaylandPointer::Delegate* pointer_delegate,
    WaylandTouch::Delegate* touch_delegate)
    : connection_(connection),
      data_device_manager_(data_device_manager),
      data_device_(data_device_manager->GetDevice()),
      window_manager_(connection->window_manager()),
      pointer_delegate_(pointer_delegate),
      touch_delegate_(touch_delegate) {
  DCHECK(connection_);
  DCHECK(window_manager_);
  DCHECK(data_device_manager_);
  DCHECK(data_device_);

  // Start observing for potential destructions of windows involved in the
  // drag session, if there is any active. E.g: origin and entered window.
  window_manager_->AddObserver(this);
}

WaylandDataDragController::~WaylandDataDragController() {
  window_manager_->RemoveObserver(this);
}

bool WaylandDataDragController::StartSession(const OSExchangeData& data,
                                             int operations,
                                             DragEventSource source) {
  // TODO(crbug.com/340398746): Should be DCHECK'ed instead, though due to buggy
  // compositors, eg: KWin 6, which do not send data_source.dnd_finish|cancelled
  // in some cases, it was temporarily turned into this conditional avoid
  // browser crashes. Revert once it stabilizes at compositors side.
  if (state_ != State::kIdle) {
    Reset();
  }

  auto* origin_window = source == DragEventSource::kTouch
                            ? window_manager_->GetCurrentTouchFocusedWindow()
                            : window_manager_->GetCurrentPointerFocusedWindow();
  if (!origin_window) {
    LOG(ERROR) << "Failed to get focused window. source=" << source;
    return false;
  }

  // Drag start may be triggered asynchronously. Due this, it is possible that
  // by the time "start drag" gets processed by Ozone/Wayland, the origin
  // pointer event (touch or mouse) has already been released. In this case,
  // make sure the flow bails earlier, otherwise the drag loop keeps running,
  // causing hangs as observed in crbug.com/1209269.
  auto serial = GetAndValidateSerialForDrag(source);
  if (!serial.has_value()) {
    LOG(ERROR) << "Invalid state when trying to start drag. source=" << source;
    return false;
  }

  VLOG(1) << "Starting Drag-and-Drop session. tracker_id=" << serial->value
          << ", data_source="
          << (source == DragEventSource::kMouse ? "mouse" : "touch")
          << ", serial tracker=" << connection_->serial_tracker().ToString();

  // Create new data source and offers |data|.
  SetOfferedExchangeDataProvider(data);
  data_source_ = data_device_manager_->CreateSource(this);
  data_source_->Offer(GetOfferedExchangeDataProvider()->BuildMimeTypesList());
  data_source_->SetDndActions(DragOperationsToDndActions(operations));

  // Create drag icon surface (if any) and store the data to be exchanged.
  icon_image_ = data.provider().GetDragImage();
  if (!icon_image_.isNull()) {
    icon_surface_ = std::make_unique<WaylandSurface>(connection_, nullptr);
    if (icon_surface_->Initialize()) {
      // TODO(crbug.com/369219145): Revisit and double-check if latched state
      // can be used here (as well as in UpdateDragImage) instead. Original
      // reasoning: latched state is what is currently displayed to the user.
      icon_surface_buffer_scale_ = origin_window->applied_state().window_scale;
      icon_surface_->set_surface_buffer_scale(icon_surface_buffer_scale_);
      // Icon surface do not need input.
      const std::vector<gfx::Rect> kEmptyRegionPx{{}};
      icon_surface_->set_input_region(kEmptyRegionPx);
      icon_surface_->ApplyPendingState();

      auto icon_offset = -data.provider().GetDragImageOffset();
      pending_icon_offset_ = {icon_offset.x(), icon_offset.y()};
      current_icon_offset_ = {0, 0};
    } else {
      LOG(ERROR) << "Failed to create drag icon surface.";
      icon_surface_.reset();
      icon_surface_buffer_scale_ = 1.0f;
    }
  }

  // Starts the wayland drag session setting |this| object as delegate.
  state_ = State::kStarted;
  has_received_enter_ = false;
  drag_source_ = source;
  origin_window_ = origin_window;
  data_device_->StartDrag(*data_source_, *origin_window, serial->value,
                          icon_surface_ ? icon_surface_->surface() : nullptr,
                          this);

  SetUpWindowDraggingSessionIfNeeded(data);

  // Monitor mouse events so that the session can be aborted if needed.
  nested_dispatcher_ =
      PlatformEventSource::GetInstance()->OverrideDispatcher(this);
  return true;
}

void WaylandDataDragController::CancelSession() {
  // Inform the compositor that we're no longer interested in the data offer.
  if (data_offer_) {
    data_offer_->SetDndActions(0);
  }

  // If this is an outgoing drag session, Reset() will reset the wl_data_source
  // we created for the drag, which as per the spec for
  // wl_data_device.start_drag() will cancel the DnD session.
  //
  // Note: resetting immediately might lead to issues in tests in the future,
  // because WaylandWindow::StartDrag() will return even though the compositor
  // might still be in the process of ending the drag session. However,
  // deferring the reset (e.g. until we receive wl_data_device.leave) in turn
  // might lead to issues for real users if the compositor implements the spec
  // incorrectly or just in a way we didn't foresee.
  HandleDragEnd(DragResult::kCancelled, ui::EventTimeForNow());
  Reset();
}

bool WaylandDataDragController::IsDragInProgress() const {
  return state_ != State::kIdle;
}

void WaylandDataDragController::UpdateDragImage(const gfx::ImageSkia& image,
                                                const gfx::Vector2d& offset) {
  icon_image_ = image;

  if (icon_surface_ && window_) {
    icon_surface_buffer_scale_ = window_->applied_state().window_scale;
    icon_surface_->set_surface_buffer_scale(icon_surface_buffer_scale_);
    icon_surface_->ApplyPendingState();
  }

  pending_icon_offset_ = {-offset.x(), -offset.y()};

  DrawIconInternal();
}

bool WaylandDataDragController::ShouldReleaseCaptureForDrag(
    ui::OSExchangeData* data) const {
  DCHECK(data);
  // For a window dragging session, we must not release capture to be able to
  // handle window dragging even when dragging out of the window.
  return !IsWindowDraggingSession(*data);
}

bool WaylandDataDragController::IsWindowDragSessionRunning() const {
  return !!pointer_grabber_for_window_drag_;
}

void WaylandDataDragController::DumpState(std::ostream& out) const {
  constexpr auto kStateToString = base::MakeFixedFlatMap<State, const char*>(
      {{State::kIdle, "idle"},
       {State::kStarted, "started"},
       {State::kFetching, "fetching"}});
  out << "WaylandDataDragController: state="
      << GetMapValueOrDefault(kStateToString, state_)
      << ", drag_source=" << !!drag_source_
      << ", data_source=" << !!data_source_
      << ", origin_window=" << GetWindowName(origin_window_)
      << ", current_window=" << GetWindowName(window_)
      << ", last_drag_location=" << last_drag_location_.ToString()
      << ", icon_surface_bufer_scale=" << icon_surface_buffer_scale_
      << ", pending_icon_offset=" << pending_icon_offset_.ToString()
      << ", current_icon_offset=" << current_icon_offset_.ToString();
}

// Sessions initiated from Chromium, will have |data_source_| set. In which
// case, |offered_exchange_data_provider_| is expected to be non-null as well.
bool WaylandDataDragController::IsDragSource() const {
  DCHECK(!data_source_ || offered_exchange_data_provider_);
  return !!data_source_;
}

void WaylandDataDragController::DrawIcon() {
  if (!icon_surface_ || icon_image_.isNull()) {
    return;
  }

  static const wl_callback_listener kFrameListener{
      .done = WaylandDataDragController::OnDragSurfaceFrame};

  wl_surface* const surface = icon_surface_->surface();
  icon_frame_callback_.reset(wl_surface_frame(surface));
  wl_callback_add_listener(icon_frame_callback_.get(), &kFrameListener, this);

  // Some Wayland compositors seem to assume that the icon surface will already
  // have a non-null buffer attached when wl_data_device.start_drag is issued,
  // otherwise it does not get drawn when, for example, attached in an upcoming
  // wl_surface.frame callback. This was observed, at least in Sway/Wlroots and
  // Weston, see https://crbug.com/1359364 for details.
  DrawIconInternal();
}

void WaylandDataDragController::OnDragSurfaceFrame(void* data,
                                                   struct wl_callback* callback,
                                                   uint32_t time) {
  auto* self = static_cast<WaylandDataDragController*>(data);
  DCHECK(self);
  self->DrawIconInternal();
  self->icon_frame_callback_.reset();
  self->connection_->Flush();
}

SkBitmap WaylandDataDragController::GetIconBitmap() {
  return icon_image_.GetRepresentation(icon_surface_buffer_scale_).GetBitmap();
}

void WaylandDataDragController::DrawIconInternal() {
  VLOG(1) << "Draw drag icon.";

  // If there was a drag icon before but now there isn't, attach a null buffer
  // to the icon surface to hide it.
  if (icon_surface_ && icon_image_.isNull()) {
    auto* const surface = icon_surface_->surface();
    wl_surface_attach(surface, nullptr, 0, 0);
    wl_surface_commit(surface);
  }

  if (!icon_surface_ || icon_image_.isNull()) {
    return;
  }

  auto icon_bitmap = GetIconBitmap();
  CHECK(!icon_bitmap.drawsNothing());
  // The protocol expects the attached buffer to have a pixel size that is a
  // multiple of the surface's scale factor. Some compositors (eg. Wlroots) will
  // refuse to attach the buffer if this condition is not met.
  const gfx::Size size_dip =
      gfx::ScaleToCeiledSize({icon_bitmap.width(), icon_bitmap.height()},
                             1.0f / icon_surface_buffer_scale_);
  const gfx::Size size_px =
      gfx::ScaleToCeiledSize(size_dip, icon_surface_buffer_scale_);

  icon_buffer_ = std::make_unique<WaylandShmBuffer>(
      connection_->buffer_factory(), size_px);
  if (!icon_buffer_->IsValid()) {
    LOG(ERROR) << "Failed to create drag icon buffer.";
    return;
  }

  DVLOG(3) << "Drawing drag icon. size_px=" << size_px.ToString();
  wl::DrawBitmap(icon_bitmap, icon_buffer_.get());
  auto* const surface = icon_surface_->surface();
  if (wl::get_version_of_object(surface) < WL_SURFACE_OFFSET_SINCE_VERSION) {
    wl_surface_attach(surface, icon_buffer_->get(),
                      pending_icon_offset_.x() - current_icon_offset_.x(),
                      pending_icon_offset_.y() - current_icon_offset_.y());
  } else {
    wl_surface_attach(surface, icon_buffer_->get(), 0, 0);
    wl_surface_offset(surface,
                      pending_icon_offset_.x() - current_icon_offset_.x(),
                      pending_icon_offset_.y() - current_icon_offset_.y());
  }
  if (connection_->UseViewporterSurfaceScaling() && icon_surface_->viewport()) {
    wp_viewport_set_destination(icon_surface_->viewport(), size_dip.width(),
                                size_dip.height());
  }
  wl_surface_damage(surface, 0, 0, size_px.width(), size_px.height());
  wl_surface_commit(surface);

  current_icon_offset_ = pending_icon_offset_;
}

void WaylandDataDragController::OnDragOffer(
    std::unique_ptr<WaylandDataOffer> offer) {
  DCHECK(!data_offer_);
  VLOG(1) << __FUNCTION__;
  data_offer_ = std::move(offer);
}

void WaylandDataDragController::OnDragEnter(WaylandWindow* window,
                                            const gfx::PointF& location,
                                            base::TimeTicks timestamp,
                                            uint32_t serial) {
  DCHECK(window);
  DCHECK(data_offer_);
  VLOG(1) << __FUNCTION__ << " is_source=" << IsDragSource();

  // Store the entered |window|. Its lifetime is monitored through
  // OnWindowRemoved, so that memory corruption issues are avoided.
  window_ = window;

  for (auto mime : data_offer_->mime_types()) {
    data_offer_->Accept(serial, mime);
  }

  // Update the focused window to ensure the window under the cursor receives
  // drag motion events.
  if (pointer_grabber_for_window_drag_) {
    DCHECK(drag_source_.has_value());
    if (*drag_source_ == DragEventSource::kMouse) {
      pointer_delegate_->OnPointerFocusChanged(
          window, location, timestamp, wl::EventDispatchPolicy::kImmediate);
    } else {
      touch_delegate_->OnTouchFocusChanged(window);
    }

    pointer_grabber_for_window_drag_ =
        window_manager_->GetCurrentPointerOrTouchFocusedWindow();
    DCHECK(pointer_grabber_for_window_drag_);
  }

  window_->OnDragEnter(
      location, DndActionsToDragOperations(data_offer_->source_actions()));

  if (IsDragSource()) {
    // If the DND session was initiated from a Chromium window,
    // |offered_exchange_data_provider_| already holds the data to be exchanged,
    // so we don't need to read it through Wayland and can just copy it here.
    DCHECK_EQ(state_, State::kStarted);
    DCHECK(offered_exchange_data_provider_);
    has_received_enter_ = true;
    window_->OnDragDataAvailable(std::make_unique<OSExchangeData>(
        offered_exchange_data_provider_->Clone()));
  } else {
    // Otherwise, we are about to accept data dragged from another application.
    // Reading the data may take some time so set |state_| to |kFetching|, and
    // schedule a task to do the actual data fetching. Also, as a safeguard
    // against buggy/malicious compositors, before posting a new fetch task
    // cancel the previous one, if any, so preventing from flooding the thread
    // pool.
    CancelDataFetchingIfNeeded();
    state_ = State::kFetching;
    auto cancel_flag = base::MakeRefCounted<CancelFlag>();
    PostDataFetchingTask(location, timestamp, cancel_flag);
  }

  OnDragMotion(location, timestamp);
}

void WaylandDataDragController::OnDragMotion(const gfx::PointF& location,
                                             base::TimeTicks timestamp) {
  VLOG(2) << __FUNCTION__ << " window=" << !!window_
          << " location=" << location.ToString()
          << " fetching=" << (state_ == State::kFetching);

  if (!window_) {
    return;
  }

  // The session might have been cancelled in response to OnDragEnter() or
  // OnDragDataAvailable().
  if (!data_offer_) {
    return;
  }

  // TODO(crbug.com/41492749): we should update the cursor position when some
  // data is dragged from another client. Currently `drag_source_` may be
  // nullopt in that case.
  if (drag_source_.has_value()) {
    // Update the cursor position only for drag with mouse.
    if (*drag_source_ == mojom::DragEventSource::kMouse) {
      auto* cursor_position = connection_->wayland_cursor_position();
      if (cursor_position) {
        CHECK(window_);
        // TODO(crbug.com/41494257): Once we enable the input region for
        // subsurfaces, we need to update this part since the location will no
        // longer be relative to the window.
        auto location_in_screen =
            gfx::ToRoundedPoint(location) +
            window_->GetBoundsInDIP().origin().OffsetFromOrigin();
        cursor_position->OnCursorPositionChanged(location_in_screen);
      }
    }
  }

  DCHECK(data_offer_);
  const int client_operations = window_->OnDragMotion(
      location, DndActionsToDragOperations(data_offer_->source_actions()));

  data_offer_->SetDndActions(DragOperationsToDndActions(client_operations));
}

void WaylandDataDragController::OnDragLeave(base::TimeTicks timestamp) {
  VLOG(1) << __FUNCTION__ << " window=" << !!window_
          << " fetching=" << (state_ == State::kFetching)
          << " is_source=" << IsDragSource();

  // For incoming drag sessions, i.e: originated in an external application,
  // reset state kIdle now. Otherwise, it'll be reset in OnDataSourceFinish.
  if (!IsDragSource()) {
    state_ = State::kIdle;
    CancelDataFetchingIfNeeded();
  }

  if (!window_) {
    return;
  }

  window_->OnDragLeave();
  window_ = nullptr;
  data_offer_.reset();
}

void WaylandDataDragController::OnDragDrop(base::TimeTicks timestamp) {
  VLOG(1) << __FUNCTION__ << " window=" << !!window_;
  if (!window_) {
    return;
  }

  window_->OnDragDrop();

  // Might have already been reset if the drag was cancelled in response to the
  // OnDragDrop() above.
  if (data_offer_) {
    // Offer must be finished and destroyed here as some compositors may delay
    // to send wl_data_source::finished|cancelled until owning client destroys
    // the drag offer. e.g: Exosphere.
    data_offer_->FinishOffer();
    data_offer_.reset();
  }
}

void WaylandDataDragController::OnDataSourceFinish(WaylandDataSource* source,
                                                   base::TimeTicks timestamp,
                                                   bool completed) {
  CHECK_EQ(data_source_.get(), source);
  VLOG(1) << __FUNCTION__ << " window=" << !!window_
          << " origin=" << !!origin_window_
          << " nested_dispatcher=" << !!nested_dispatcher_;

  // HandleDragEnd below is likely no-op, though is called to protect against
  // buggy/malicious compositors, where no prior dnd_drop_performed was
  // received, for example. In which case, it could result in UI hangs where
  // input events would stop being processed because the nested drag message
  // loop would keep running indefinitely.
  HandleDragEnd(completed ? DragResult::kCompleted : DragResult::kCancelled,
                timestamp);
  Reset();
}

void WaylandDataDragController::OnDataSourceDropPerformed(
    WaylandDataSource* source,
    base::TimeTicks timestamp) {
  CHECK_EQ(data_source_.get(), source);
  VLOG(1) << __FUNCTION__ << " window=" << !!window_
          << " origin=" << !!origin_window_
          << " nested_dispatcher=" << !!nested_dispatcher_;

  HandleDragEnd(DragResult::kCompleted, timestamp);
}

const WaylandWindow* WaylandDataDragController::GetDragTarget() const {
  return window_;
}

void WaylandDataDragController::OnDataSourceSend(WaylandDataSource* source,
                                                 const std::string& mime_type,
                                                 std::string* buffer) {
  CHECK_EQ(data_source_.get(), source);
  CHECK(buffer);
  VLOG(1) << __FUNCTION__ << " mime=" << mime_type;

  // We don't actually have any data to send. Nothing except Chrome itself
  // should accept this MIME type, and Chrome won't request the non-existent
  // data; but the KDE desktop seems to accept and request the data. To prevent
  // hitting a CHECK in ExtractData() due to the MIME type, we exit early here.
  if (mime_type == ui::kMimeTypeWindowDrag) {
    return;
  }

  if (!GetOfferedExchangeDataProvider()->ExtractData(mime_type, buffer)) {
    LOG(WARNING) << "Cannot deliver data of type " << mime_type
                 << " and no text representation is available.";
  }
}

void WaylandDataDragController::OnWindowRemoved(WaylandWindow* window) {
  if (window == window_) {
    window_ = nullptr;
  }

  if (window == origin_window_) {
    // See the declaration of TakeWaylandSurface() for why this is needed.
    if (IsWindowDragSessionRunning()) {
      origin_surface_ = origin_window_->TakeWaylandSurface();
    }
    origin_window_ = nullptr;
  }

  if (window == pointer_grabber_for_window_drag_) {
    pointer_grabber_for_window_drag_ = nullptr;
  }
}

void WaylandDataDragController::PostDataFetchingTask(
    const gfx::PointF& location,
    base::TimeTicks start_time,
    const scoped_refptr<CancelFlag>& cancel_flag) {
  using FetchingInfo = std::vector<std::pair<std::string, int>>;

  DCHECK_EQ(state_, State::kFetching);
  DCHECK(!cancel_flag->data.IsSet());

  // This check may fail if OnDragEnter() runs a nested loop or other blocking
  // call that results in calling back to WaylandDataDragController. For now,
  // assume this isn't the case, so |data_offer_| should be non-null.
  DCHECK(data_offer_);

  FetchingInfo offered_data;
  for (const auto& mime_type : data_offer_->mime_types()) {
    if (!IsMimeTypeSupported(mime_type)) {
      LOG(WARNING) << "Skipping unsupported mime type " << mime_type;
      continue;
    }

    VLOG(1) << __func__ << " requesting to receive data for " << mime_type;
    base::ScopedFD fd = data_offer_->Receive(mime_type);
    if (!fd.is_valid()) {
      PLOG(ERROR) << "Failed to open file descriptor for " << mime_type;
      continue;
    }
    offered_data.emplace_back(mime_type, fd.release());
  }
  connection_->Flush();

  auto fetch_data_closure = [](FetchingInfo offered_data,
                               const scoped_refptr<CancelFlag>& cancel_flag)
      -> std::unique_ptr<OSExchangeData> {
    base::ScopedBlockingCall blocking_call(FROM_HERE,
                                           base::BlockingType::MAY_BLOCK);
    auto fetched_data = std::make_unique<WaylandExchangeDataProvider>();

    VLOG(1) << "Starting data fetching for " << offered_data.size()
            << " mime types.";

    for (const auto& [mime_type, fd_handle] : offered_data) {
      DCHECK(IsMimeTypeSupported(mime_type));

      if (cancel_flag->data.IsSet()) {
        VLOG(1) << "cancelled data fetching.";
        return {};
      }

      VLOG(1) << "will fetch data for " << mime_type;
      std::vector<uint8_t> contents;
      wl::ReadDataFromFD(base::ScopedFD(fd_handle), &contents);
      if (contents.empty()) {
        continue;
      }

      VLOG(1) << "did fetch " << contents.size() << " bytes.";
      fetched_data->AddData(base::RefCountedBytes::TakeVector(&contents),
                            mime_type);
    }

    return std::make_unique<OSExchangeData>(std::move(fetched_data));
  };

  last_drag_location_ = location;
  data_fetch_cancel_flag_ = cancel_flag;

  GetDataFetchTaskRunner().PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(fetch_data_closure, std::move(offered_data), cancel_flag),
      base::BindOnce(&WaylandDataDragController::OnDataFetchingFinished,
                     weak_factory_.GetWeakPtr(), start_time));
}

void WaylandDataDragController::OnDataFetchingFinished(
    base::TimeTicks timestamp,
    std::unique_ptr<OSExchangeData> received_data) {
  // This is expected to be called only in incoming drag sessions.
  DCHECK(!IsDragSource());
  VLOG(1) << __func__ << " fetching=" << (state_ == State::kFetching)
          << " window=" << !!window_;

  // Null `received_data` means the fetching task was cancelled. ie: drag leave
  // arrived before data was fetched, in which case the `window_` was already
  // notified and state already reset in OnDragLeave(), so just early out here.
  if (!received_data) {
    VLOG(1) << "fetching cancelled.";
    return;
  }

  // Move to `kStarted` state, regardless it is possible or not to deliver it.
  state_ = State::kStarted;

  // |window_| may have already been unset here if, eg: programmatically
  // destroyed by the application. See https://crbug.com/1487387.
  if (!window_) {
    return;
  }
  window_->OnDragDataAvailable(std::move(received_data));
}

void WaylandDataDragController::CancelDataFetchingIfNeeded() {
  if (state_ == State::kFetching) {
    return;
  }
  if (data_fetch_cancel_flag_) {
    VLOG_IF(1, data_fetch_cancel_flag_->data.IsSet())
        << "Cancelling data fetching.";
    data_fetch_cancel_flag_->data.Set();
    data_fetch_cancel_flag_ = nullptr;
  }
}

void WaylandDataDragController::Reset() {
  if (state_ == State::kIdle) {
    return;
  }

  data_source_.reset();
  data_offer_.reset();
  origin_window_ = nullptr;
  origin_surface_.reset();
  icon_buffer_.reset();
  icon_surface_.reset();
  icon_surface_buffer_scale_ = 1.0f;
  icon_image_ = gfx::ImageSkia();
  icon_frame_callback_.reset();
  offered_exchange_data_provider_.reset();
  data_device_->ResetDragDelegate();
  has_received_enter_ = false;
  state_ = State::kIdle;
}

// TODO(crbug.com/329479345): Drop DragResult in favor of intermediary states
// `kDropping` and `kCancelling`, move `Reset` back into this function, called
// conditionally depending on the state it would be transitioning to.
void WaylandDataDragController::HandleDragEnd(DragResult result,
                                              base::TimeTicks timestamp) {
  if (origin_window_) {
    DragOperation operation =
        (result == DragResult::kCompleted)
            ? DndActionToDragOperation(data_source_->dnd_action())
            : DragOperation::kNone;
    origin_window_->OnDragSessionClose(operation);
    // DnD handlers expect DragLeave to be sent for drag sessions that end up
    // with no data fetching (wl_data_source::cancelled event).
    if (result != DragResult::kCompleted) {
      origin_window_->OnDragLeave();
    }

    // Ensure we don't send a second leave event below.
    if (origin_window_ == window_) {
      window_ = nullptr;
    }
    origin_window_ = nullptr;
  }

  // We need to reset |nested_dispatcher_| before dispatching the pointer
  // release, or else the event will be intercepted by ourself.
  nested_dispatcher_.reset();

  // Dispatch this after calling WaylandWindow::OnDragSessionClose(), else the
  // extra leave event that is dispatched if |completed| is false may cause
  // problems.
  if (pointer_grabber_for_window_drag_) {
    DispatchPointerRelease(timestamp);
  }

  // If we called this method because we want to cancel an incoming drag
  // session, make sure we send a final leave event. The real leave event we'll
  // receive from the compositor won't be propagated because we reset
  // |data_device_|'s delegate below.
  if (window_) {
    window_->OnDragLeave();
    window_ = nullptr;
  }
}

std::optional<wl::Serial>
WaylandDataDragController::GetAndValidateSerialForDrag(DragEventSource source) {
  wl::SerialType serial_type;
  bool should_drag = false;
  switch (source) {
    case DragEventSource::kMouse:
      serial_type = wl::SerialType::kMousePress;
      should_drag = pointer_delegate_->IsPointerButtonPressed(EF_MOUSE_BUTTON);
      break;
    case DragEventSource::kTouch:
      serial_type = wl::SerialType::kTouchPress;
      should_drag = !touch_delegate_->GetActiveTouchPointIds().empty();
      break;
  }
  return should_drag ? connection_->serial_tracker().GetSerial(serial_type)
                     : std::nullopt;
}

void WaylandDataDragController::SetOfferedExchangeDataProvider(
    const OSExchangeData& data) {
  offered_exchange_data_provider_ = data.provider().Clone();
}

const WaylandExchangeDataProvider*
WaylandDataDragController::GetOfferedExchangeDataProvider() const {
  DCHECK(offered_exchange_data_provider_);
  return static_cast<const WaylandExchangeDataProvider*>(
      offered_exchange_data_provider_.get());
}

bool WaylandDataDragController::IsWindowDraggingSession(
    const ui::OSExchangeData& data) const {
  auto custom_format =
      ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag);
  return data.provider().HasCustomFormat(custom_format);
}

void WaylandDataDragController::SetUpWindowDraggingSessionIfNeeded(
    const ui::OSExchangeData& data) {
  if (!IsWindowDraggingSession(data)) {
    return;
  }

  DCHECK(origin_window_);
  pointer_grabber_for_window_drag_ = origin_window_;
}

void WaylandDataDragController::DispatchPointerRelease(
    base::TimeTicks timestamp) {
  DCHECK(pointer_grabber_for_window_drag_);
  pointer_delegate_->OnPointerButtonEvent(
      EventType::kMouseReleased, EF_LEFT_MOUSE_BUTTON, timestamp,
      pointer_grabber_for_window_drag_, wl::EventDispatchPolicy::kImmediate,
      /*allow_release_of_unpressed_button=*/true,
      /*is_synthesized=*/true);
  pointer_grabber_for_window_drag_ = nullptr;
}

bool WaylandDataDragController::CanDispatchEvent(const PlatformEvent& event) {
  return state_ != State::kIdle;
}

uint32_t WaylandDataDragController::DispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(state_, State::kIdle);

  // Currently, there's no reliable way in the protocol to determine when the
  // drag session has effectively started, so as a best-effort heuristic we
  // consider it started once wl_data_device.enter has been received at least
  // once.
  auto cancel_drag_cb = base::BindOnce(
      [](WaylandDataDragController* self, base::TimeTicks time_stamp) {
        self->HandleDragEnd(DragResult::kCancelled, time_stamp);
        self->Reset();
      },
      base::Unretained(this), event->time_stamp());
  if (wl::MaybeHandlePlatformEventForDrag(
          event, /*start_drag_ack_received=*/has_received_enter_,
          std::move(cancel_drag_cb))) {
    return POST_DISPATCH_STOP_PROPAGATION;
  }

  return POST_DISPATCH_PERFORM_DEFAULT;
}

//  Data fetch task runner is lazily initialized so unrelated test suites
//  (without ThreadPool available) do not fail.
//
//  TODO(b/328574254): Move to constructor once the offending test suites
//  provide a suitable task environment.
base::TaskRunner& WaylandDataDragController::GetDataFetchTaskRunner() {
  if (!data_fetch_task_runner_) {
    data_fetch_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE, base::MayBlock()});
  }
  CHECK(data_fetch_task_runner_);
  return *data_fetch_task_runner_;
}

}  // namespace ui
