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
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/events/event_constants.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/dump_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
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
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!origin_window_);
  DCHECK(!icon_surface_);

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

  VLOG(1) << "Starting DND session.";

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
      // Corresponds to actual scale factor of the origin surface. Use the
      // latched state as that is what is currently displayed to the user and
      // used as buffers in these surfaces.
      icon_surface_buffer_scale_ = origin_window->applied_state().window_scale;
      icon_surface_->set_surface_buffer_scale(icon_surface_buffer_scale_);
      // Icon surface do not need input.
      const gfx::Rect empty_region_px;
      icon_surface_->set_input_region(empty_region_px);
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

void WaylandDataDragController::DumpState(std::ostream& out) const {
  constexpr auto kStateToString = base::MakeFixedFlatMap<State, const char*>(
      {{State::kIdle, "idle"},
       {State::kStarted, "started"},
       {State::kTransferring, "transferring"}});
  out << "WaylandDataDragController: state="
      << GetMapValueOrDefault(kStateToString, state_)
      << ", drag_source=" << !!drag_source_
      << ", data_source=" << !!data_source_
      << ", origin_window=" << GetWindowName(origin_window_)
      << ", current_window=" << GetWindowName(window_)
      << ", unprocessed_mime_types=" << ListToString(unprocessed_mime_types_)
      << ", last_drag_location=" << last_drag_location_.ToString()
      << ", is_leave_pending=" << is_leave_pending_
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
                                            uint32_t serial) {
  DCHECK(window);
  DCHECK(data_offer_);
  VLOG(1) << __FUNCTION__ << " is_source=" << IsDragSource();

  // Store the entered |window|. Its lifetime is monitored through
  // OnWindowRemoved, so that memory corruption issues are avoided.
  window_ = window;

  unprocessed_mime_types_.clear();
  for (auto mime : data_offer_->mime_types()) {
    unprocessed_mime_types_.push_back(mime);
    data_offer_->Accept(serial, mime);
  }

  // Update the focused window to ensure the window under the cursor receives
  // drag motion events.
  if (pointer_grabber_for_window_drag_) {
    DCHECK(drag_source_.has_value());
    if (*drag_source_ == DragEventSource::kMouse) {
      pointer_delegate_->OnPointerFocusChanged(
          window, location, wl::EventDispatchPolicy::kImmediate);
    } else {
      touch_delegate_->OnTouchFocusChanged(window);
    }

    pointer_grabber_for_window_drag_ =
        window_manager_->GetCurrentPointerOrTouchFocusedWindow();
    DCHECK(pointer_grabber_for_window_drag_);
  }

  if (IsDragSource()) {
    // If the DND session was initiated from a Chromium window,
    // |offered_exchange_data_provider_| already holds the data to be exchanged,
    // so we don't need to read it through Wayland and can just copy it here.
    DCHECK(offered_exchange_data_provider_);
    PropagateOnDragEnter(location,
                         std::make_unique<OSExchangeData>(
                             offered_exchange_data_provider_->Clone()));
  } else {
    // Otherwise, we are about to accept data dragged from another application.
    // Reading the data may take some time so set |state_| to |kTransferring|,
    // which will defer sending OnDragEnter to the client until the data
    // is ready.
    state_ = State::kTransferring;
    received_exchange_data_provider_ =
        std::make_unique<WaylandExchangeDataProvider>();
    last_drag_location_ = location;
    HandleUnprocessedMimeTypes(base::TimeTicks::Now());
  }
}

void WaylandDataDragController::OnDragMotion(const gfx::PointF& location) {
  VLOG(2) << __FUNCTION__ << " window=" << !!window_
          << " location=" << location.ToString()
          << " transferring=" << (state_ == State::kTransferring);

  if (!window_) {
    return;
  }

  if (state_ == State::kTransferring) {
    last_drag_location_ = location;
    return;
  }

  DCHECK(data_offer_);
  int available_operations =
      DndActionsToDragOperations(data_offer_->source_actions());
  int client_operations = window_->OnDragMotion(location, available_operations);

  data_offer_->SetDndActions(DragOperationsToDndActions(client_operations));
}

void WaylandDataDragController::OnDragLeave() {
  VLOG(2) << __FUNCTION__ << " window=" << !!window_
          << " transferring=" << (state_ == State::kTransferring);

  if (state_ == State::kTransferring) {
    // We cannot leave until the transfer is finished.  Postponing.
    is_leave_pending_ = true;
    return;
  }

  if (window_) {
    window_->OnDragLeave();
  }

  window_ = nullptr;
  data_offer_.reset();
  is_leave_pending_ = false;
}

void WaylandDataDragController::OnDragDrop() {
  VLOG(1) << __FUNCTION__ << " window=" << !!window_;
  if (!window_) {
    return;
  }

  window_->OnDragDrop();

  // Offer must be finished and destroyed here as some compositors may delay to
  // send wl_data_source::finished|cancelled until owning client destroys the
  // drag offer. e.g: Exosphere.
  data_offer_->FinishOffer();
  data_offer_.reset();
}

void WaylandDataDragController::OnDataSourceFinish(WaylandDataSource* source,
                                                   bool completed) {
  CHECK_EQ(data_source_.get(), source);
  VLOG(1) << __FUNCTION__ << " window=" << !!window_
          << " origin=" << !!origin_window_
          << " nested_dispatcher=" << !!nested_dispatcher_;

  if (origin_window_) {
    origin_window_->OnDragSessionClose(
        completed ? DndActionToDragOperation(data_source_->dnd_action())
                  : DragOperation::kNone);
    // DnD handlers expect DragLeave to be sent for drag sessions that end up
    // with no data transfer (wl_data_source::cancelled event).
    if (!completed) {
      origin_window_->OnDragLeave();
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
    DispatchPointerRelease();
  }

  data_source_.reset();
  data_offer_.reset();
  icon_buffer_.reset();
  icon_surface_.reset();
  icon_surface_buffer_scale_ = 1.0f;
  icon_image_ = gfx::ImageSkia();
  icon_frame_callback_.reset();
  offered_exchange_data_provider_.reset();
  data_device_->ResetDragDelegate();
  state_ = State::kIdle;
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
    origin_window_ = nullptr;
  }

  if (window == pointer_grabber_for_window_drag_) {
    pointer_grabber_for_window_drag_ = nullptr;
  }
}

// Asynchronously requests and reads data for every negotiated/supported mime
// type, one after another, OnMimeTypeDataTransferred calls back into this
// function once it finishes reading data for each mime type, until there is no
// more unprocessed mime types on the |unprocessed_mime_types_| queue. Once this
// process is finished, OnDataTransferFinished is called to deliver the
// |received_exchange_data_provider_| to the drop handler.
void WaylandDataDragController::HandleUnprocessedMimeTypes(
    base::TimeTicks start_time) {
  std::string mime_type = GetNextUnprocessedMimeType();
  VLOG(1) << __FUNCTION__ << " mime=" << mime_type;
  if (mime_type.empty() || is_leave_pending_ || state_ == State::kIdle ||
      !window_) {
    OnDataTransferFinished(start_time,
                           std::make_unique<OSExchangeData>(
                               std::move(received_exchange_data_provider_)));
  } else {
    DCHECK(data_offer_);
    data_device_->RequestData(
        data_offer_.get(), mime_type,
        base::BindOnce(&WaylandDataDragController::OnMimeTypeDataTransferred,
                       weak_factory_.GetWeakPtr(), start_time));
  }
}

// Post |data_transferred_callback_for_testing_|, if any, to be executed in the
// current task runner, such that test code is run only when current event is
// fully processed, ie: internal drag controller state consistency is assured.
void WaylandDataDragController::RunDataTransferredCallbackForTesting(
    const std::string& mime_type) {
  if (data_transferred_callback_for_testing_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(data_transferred_callback_for_testing_, mime_type));
  }
}

void WaylandDataDragController::OnMimeTypeDataTransferred(
    base::TimeTicks start_time,
    PlatformClipboard::Data contents) {
  DCHECK(contents);
  std::string mime_type = unprocessed_mime_types_.front();
  if (!contents->data().empty()) {
    received_exchange_data_provider_->AddData(contents, mime_type);
  }
  unprocessed_mime_types_.pop_front();

  RunDataTransferredCallbackForTesting(mime_type);  // IN-TEST

  // Continue reading data for other negotiated mime types.
  HandleUnprocessedMimeTypes(start_time);
}

void WaylandDataDragController::OnDataTransferFinished(
    base::TimeTicks start_time,
    std::unique_ptr<OSExchangeData> received_data) {
  VLOG(1) << __FUNCTION__ << " unprocessed=" << unprocessed_mime_types_.size()
          << " leave_pending=" << is_leave_pending_ << " window=" << !!window_;
  unprocessed_mime_types_.clear();
  if (state_ == State::kIdle) {
    return;
  }

  state_ = State::kIdle;

  RunDataTransferredCallbackForTesting();  // IN-TEST

  // If a 'leave' event was received while incoming data was on transit (see
  // OnDragLeave function), propagating 'enter' event at this point makes no
  // sense anymore. So reset internal state and exit.
  if (is_leave_pending_) {
    DCHECK(!IsDragSource());
    if (data_offer_) {
      data_offer_->FinishOffer();
      data_offer_.reset();
    }
    offered_exchange_data_provider_.reset();
    data_device_->ResetDragDelegateIfNotDragSource();
    is_leave_pending_ = false;
    return;
  }

  UMA_HISTOGRAM_TIMES("Event.WaylandDragDrop.IncomingDataTransferTime",
                      base::TimeTicks::Now() - start_time);

  PropagateOnDragEnter(last_drag_location_, std::move(received_data));
}

// Returns the next MIME type to be received from the source process, or an
// empty string if there are no more interesting MIME types left to process.
std::string WaylandDataDragController::GetNextUnprocessedMimeType() {
  while (!unprocessed_mime_types_.empty()) {
    const std::string& mime_type = unprocessed_mime_types_.front();
    if (!IsMimeTypeSupported(mime_type)) {
      VLOG(1) << "Skipping unsupported mime type: " << mime_type;
      unprocessed_mime_types_.pop_front();
      continue;
    }
    return mime_type;
  }
  return {};
}

void WaylandDataDragController::PropagateOnDragEnter(
    const gfx::PointF& location,
    std::unique_ptr<OSExchangeData> data) {
  VLOG(1) << __func__ << " window=" << !!window_ << " offer=" << !!data_offer_;

  // |data_offer_| may have already been destroyed at this point if, for
  // example, the drop event comes in while the data fetching was ongoing and no
  // subsequent 'leave' is received, so just early-out in this case.
  if (!data_offer_) {
    return;
  }

  // |window_| may have already been unset here if, for instance, user has
  // dragged out of it in incoming dnd sessions. See https://crbug.com/1487387.
  if (!window_) {
    return;
  }

  window_->OnDragEnter(
      location, std::move(data),
      DndActionsToDragOperations(data_offer_->source_actions()));
  OnDragMotion(location);
}

absl::optional<wl::Serial>
WaylandDataDragController::GetAndValidateSerialForDrag(DragEventSource source) {
  wl::SerialType serial_type;
  bool should_drag = false;
  switch (source) {
    case DragEventSource::kMouse:
      serial_type = wl::SerialType::kMousePress;
      should_drag =
          pointer_delegate_->IsPointerButtonPressed(EF_LEFT_MOUSE_BUTTON);
      break;
    case DragEventSource::kTouch:
      serial_type = wl::SerialType::kTouchPress;
      should_drag = !touch_delegate_->GetActiveTouchPointIds().empty();
      break;
  }
  return should_drag ? connection_->serial_tracker().GetSerial(serial_type)
                     : absl::nullopt;
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

void WaylandDataDragController::DispatchPointerRelease() {
  DCHECK(pointer_grabber_for_window_drag_);
  pointer_delegate_->OnPointerButtonEvent(
      ET_MOUSE_RELEASED, EF_LEFT_MOUSE_BUTTON, pointer_grabber_for_window_drag_,
      wl::EventDispatchPolicy::kImmediate,
      /*allow_release_of_unpressed_button=*/true);
  pointer_grabber_for_window_drag_ = nullptr;
}

bool WaylandDataDragController::CanDispatchEvent(const PlatformEvent& event) {
  return state_ != State::kIdle;
}

uint32_t WaylandDataDragController::DispatchEvent(const PlatformEvent& event) {
  DCHECK_NE(state_, State::kIdle);

  // Drag session start may be triggered asynchronously, eg: dragging web
  // contents, which might lead to race conditions where mouse button release is
  // processed at compositor-side, sent to the client and processed just after
  // the start_drag request is issued. In such cases, the compositor may ignore
  // the request, and protocol-wise there is no explicit mechanism for clients
  // to be notified about it (eg: an error event), and the only way of detecting
  // that, for now, is to monitor wl_pointer events here and abort the session
  // if it comes in.
  if (event->type() == ET_MOUSE_RELEASED) {
    OnDataSourceFinish(data_source_.get(), /*completed=*/false);
  }

  return POST_DISPATCH_PERFORM_DEFAULT;
}

}  // namespace ui
