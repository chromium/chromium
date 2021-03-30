// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_drag_controller.h"

#include <cstdint>

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/platform/wayland/common/data_util.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_shm_buffer.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

int DndActionsToDragOperations(uint32_t actions) {
  int operations = DragDropTypes::DRAG_NONE;
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    operations |= DragDropTypes::DRAG_COPY;
  if (actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    operations |= DragDropTypes::DRAG_MOVE;
  return operations;
}

uint32_t DragOperationsToDndActions(int operations) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  if (operations & DragDropTypes::DRAG_COPY)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  if (operations & DragDropTypes::DRAG_MOVE)
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  return dnd_actions;
}

const SkBitmap* GetDragImage(const OSExchangeData& data) {
  const SkBitmap* icon_bitmap = data.provider().GetDragImage().bitmap();
  return icon_bitmap && !icon_bitmap->empty() ? icon_bitmap : nullptr;
}

}  // namespace

WaylandDataDragController::WaylandDataDragController(
    WaylandConnection* connection,
    WaylandDataDeviceManager* data_device_manager)
    : connection_(connection),
      data_device_manager_(data_device_manager),
      data_device_(data_device_manager->GetDevice()),
      window_manager_(connection->wayland_window_manager()) {
  DCHECK(connection_);
  DCHECK(window_manager_);
  DCHECK(data_device_manager_);
  DCHECK(data_device_);
}

WaylandDataDragController::~WaylandDataDragController() = default;

void WaylandDataDragController::StartSession(const OSExchangeData& data,
                                             int operation) {
  DCHECK_EQ(state_, State::kIdle);
  DCHECK(!origin_window_);

  origin_window_ = window_manager_->GetCurrentFocusedWindow();
  if (!origin_window_) {
    LOG(ERROR) << "Failed to get focused window.";
    return;
  }

  // Create new new data source and offers |data|.
  if (!data_source_)
    data_source_ = data_device_manager_->CreateSource(this);
  Offer(data, operation);

  // Create drag icon surface (if any) and store the data to be exchanged.
  icon_bitmap_ = GetDragImage(data);
  if (icon_bitmap_) {
    icon_surface_ = connection_->CreateSurface();
    wl_surface_set_buffer_scale(icon_surface_.get(),
                                origin_window_->buffer_scale());
  }
  data_ = std::make_unique<OSExchangeData>(data.provider().Clone());

  // Starts the wayland drag session setting |this| object as delegate.
  state_ = State::kStarted;
  data_device_->StartDrag(*data_source_, *origin_window_, icon_surface_.get(),
                          this);

  window_manager_->AddObserver(this);
}

// Sessions initiated from Chromium, will have |data_source_| set. In which
// case, |data_| is expected to be non-null as well.
bool WaylandDataDragController::IsDragSource() const {
  DCHECK(!data_source_ || data_);
  return !!data_source_;
}

void WaylandDataDragController::DrawIcon() {
  if (!icon_bitmap_)
    return;

  DCHECK(!icon_bitmap_->empty());
  gfx::Size size(icon_bitmap_->width(), icon_bitmap_->height());

  if (!shm_buffer_ || shm_buffer_->size() != size) {
    shm_buffer_ = std::make_unique<WaylandShmBuffer>(connection_->shm(), size);
    if (!shm_buffer_->IsValid()) {
      LOG(ERROR) << "Failed to create drag icon buffer.";
      return;
    }
  }
  wl::DrawBitmap(*icon_bitmap_, shm_buffer_.get());
  wl_surface_attach(icon_surface_.get(), shm_buffer_->get(), 0, 0);
  wl_surface_damage(icon_surface_.get(), 0, 0, size.width(), size.height());
  wl_surface_commit(icon_surface_.get());
}

void WaylandDataDragController::OnDragOffer(
    std::unique_ptr<WaylandDataOffer> offer) {
  DCHECK(!data_offer_);
  data_offer_ = std::move(offer);
}

void WaylandDataDragController::OnDragEnter(WaylandWindow* window,
                                            const gfx::PointF& location,
                                            uint32_t serial) {
  DCHECK(window);
  DCHECK(data_offer_);
  window_ = window;

  // TODO(crbug.com/1004715): Set mime type the client can accept.  Now it sets
  // all mime types offered because current implementation doesn't decide
  // action based on mime type.
  unprocessed_mime_types_.clear();
  for (auto mime : data_offer_->mime_types()) {
    unprocessed_mime_types_.push_back(mime);
    data_offer_->Accept(serial, mime);
  }

  if (IsDragSource()) {
    // If the DND session was initiated from a Chromium window, |data_| already
    // holds the data to be exchanged, so we don't need to read it through
    // Wayland and can just copy it here.
    DCHECK(data_);
    PropagateOnDragEnter(
        location, std::make_unique<OSExchangeData>(data_->provider().Clone()));
  } else {
    // Otherwise, we are about to accept data dragged from another application.
    // Reading the data may take some time so set |state_| to |kTrasferring|,
    // which will defer sending OnDragEnter to the client until the data
    // is ready.
    state_ = State::kTransferring;
    received_data_ = std::make_unique<OSExchangeData>(
        std::make_unique<OSExchangeDataProviderNonBacked>());
    last_drag_location_ = location;
    HandleUnprocessedMimeTypes(base::TimeTicks::Now());
  }
}

void WaylandDataDragController::OnDragMotion(const gfx::PointF& location) {
  if (!window_)
    return;

  if (state_ == State::kTransferring) {
    last_drag_location_ = location;
    return;
  }

  DCHECK(data_offer_);
  int available_operations =
      DndActionsToDragOperations(data_offer_->source_actions());
  int client_operations = window_->OnDragMotion(location, available_operations);

  data_offer_->SetActions(DragOperationsToDndActions(client_operations));
}

void WaylandDataDragController::OnDragLeave() {
  if (state_ == State::kTransferring) {
    // We cannot leave until the transfer is finished.  Postponing.
    is_leave_pending_ = true;
    return;
  }

  if (window_)
    window_->OnDragLeave();

  window_ = nullptr;
  data_offer_.reset();
  is_leave_pending_ = false;
}

void WaylandDataDragController::OnDragDrop() {
  if (!window_)
    return;

  window_->OnDragDrop();

  // Offer must be finished and destroyed here as some compositors may delay to
  // send wl_data_source::finished|cancelled until owning client destroys the
  // drag offer. e.g: Exosphere.
  data_offer_->FinishOffer();
  data_offer_.reset();
}

void WaylandDataDragController::OnDataSourceFinish(bool completed) {
  DCHECK(data_source_);

  if (origin_window_) {
    origin_window_->OnDragSessionClose(data_source_->dnd_action());
    // DnD handlers expect DragLeave to be sent for drag sessions that end up
    // with no data transfer (wl_data_source::cancelled event).
    if (!completed)
      origin_window_->OnDragLeave();
    origin_window_ = nullptr;
  }

  window_manager_->RemoveObserver(this);
  data_source_.reset();
  data_offer_.reset();
  data_.reset();
  data_device_->ResetDragDelegate();
  state_ = State::kIdle;
}

void WaylandDataDragController::OnDataSourceSend(const std::string& mime_type,
                                                 std::string* buffer) {
  DCHECK(data_source_);
  DCHECK(buffer);
  DCHECK(data_);
  if (!wl::ExtractOSExchangeData(*data_, mime_type, buffer)) {
    LOG(WARNING) << "Cannot deliver data of type " << mime_type
                 << " and no text representation is available.";
  }
}

void WaylandDataDragController::OnWindowRemoved(WaylandWindow* window) {
  if (window == window_)
    window_ = nullptr;

  if (window == origin_window_)
    origin_window_ = nullptr;
}

void WaylandDataDragController::Offer(const OSExchangeData& data,
                                      int operation) {
  DCHECK(data_source_);

  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland
  // specs don't say anything like that, but here we follow that common
  // practice: begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (data.HasFile()) {
    mime_types.push_back(kMimeTypeURIList);
  }
  if (data.HasURL(FilenameToURLPolicy::CONVERT_FILENAMES)) {
    mime_types.push_back(kMimeTypeMozillaURL);
  }
  if (data.HasHtml()) {
    mime_types.push_back(kMimeTypeHTML);
  }
  if (data.HasString()) {
    mime_types.push_back(kMimeTypeTextUtf8);
    mime_types.push_back(kMimeTypeText);
  }

  DCHECK(!mime_types.empty());
  data_source_->Offer(mime_types);
  data_source_->SetAction(operation);
}

// Asynchronously requests and reads data for every negotiated/supported mime
// type, one after another, OnMimeTypeDataTransferred calls back into this
// function once it finishes reading data for each mime type, until there is no
// more unprocessed mime types on the |unprocessed_mime_types_| queue. Once this
// process is finished, OnDataTransferFinished is called to deliver the
// |received_data_| to the drop handler.
void WaylandDataDragController::HandleUnprocessedMimeTypes(
    base::TimeTicks start_time) {
  std::string mime_type = GetNextUnprocessedMimeType();
  if (mime_type.empty() || is_leave_pending_ || state_ == State::kIdle) {
    OnDataTransferFinished(start_time, std::move(received_data_));
  } else {
    DCHECK(data_offer_);
    data_device_->RequestData(
        data_offer_.get(), mime_type,
        base::BindOnce(&WaylandDataDragController::OnMimeTypeDataTransferred,
                       weak_factory_.GetWeakPtr(), start_time));
  }
}

void WaylandDataDragController::OnMimeTypeDataTransferred(
    base::TimeTicks start_time,
    PlatformClipboard::Data contents) {
  DCHECK(contents);
  if (!contents->data().empty()) {
    std::string mime_type = unprocessed_mime_types_.front();
    wl::AddToOSExchangeData(contents, mime_type, received_data_.get());
  }
  unprocessed_mime_types_.pop_front();

  // Continue reading data for other negotiated mime types.
  HandleUnprocessedMimeTypes(start_time);
}

void WaylandDataDragController::OnDataTransferFinished(
    base::TimeTicks start_time,
    std::unique_ptr<OSExchangeData> received_data) {
  unprocessed_mime_types_.clear();
  if (state_ == State::kIdle)
    return;

  state_ = State::kIdle;

  // If |is_leave_pending_| is set, it means a 'leave' event was fired while
  // data was on transit (see OnDragLeave for more context).  Sending
  // OnDragEnter to the window makes no sense anymore because the drag is no
  // longer over it.  Reset and exit.
  if (is_leave_pending_) {
    if (data_offer_) {
      data_offer_->FinishOffer();
      data_offer_.reset();
    }
    data_.reset();
    data_device_->ResetDragDelegate();
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
    // Skip unsupported or already processed mime types.
    if (!wl::IsMimeTypeSupported(mime_type) ||
        wl::ContainsMimeType(*received_data_, mime_type)) {
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
  DCHECK(window_);

  window_->OnDragEnter(
      location, std::move(data),
      DndActionsToDragOperations(data_offer_->source_actions()));
  OnDragMotion(location);
}

}  // namespace ui
