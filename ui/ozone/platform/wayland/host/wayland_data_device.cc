// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_data_device.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace ui {

namespace {

constexpr OSExchangeData::FilenameToURLPolicy kFilenameToURLPolicy =
    OSExchangeData::FilenameToURLPolicy::CONVERT_FILENAMES;

// Converts raw data to either narrow or wide string.
template <typename StringType>
StringType BytesTo(const PlatformClipboard::Data& bytes) {
  if (bytes.size() % sizeof(typename StringType::value_type) != 0U) {
    // This is suspicious.
    LOG(WARNING)
        << "Data is possibly truncated, or a wrong conversion is requested.";
  }

  StringType result;
  result.assign(reinterpret_cast<typename StringType::const_pointer>(&bytes[0]),
                bytes.size() / sizeof(typename StringType::value_type));
  return result;
}

// Returns actions possible with the given source and drag'n'drop actions.
// Also converts enums: input params are wl_data_device_manager_dnd_action but
// the result is ui::DragDropTypes.
int GetPossibleActions(uint32_t source_actions, uint32_t dnd_action) {
  // If drag'n'drop action is set, use it but check for ASK action (see below).
  uint32_t action = dnd_action != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE
                        ? dnd_action
                        : source_actions;

  // We accept any action except ASK (see below).
  int operation = DragDropTypes::DRAG_NONE;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
    operation |= DragDropTypes::DRAG_COPY;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
    operation |= DragDropTypes::DRAG_MOVE;
  if (action & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) {
    // This is very rare and non-standard.  Chromium doesn't set this when
    // anything is dragged from it, neither it provides any UI for asking
    // the user about the desired drag'n'drop action when data is dragged
    // from an external source.
    // We are safe with not adding anything here.  However, keep NOTIMPLEMENTED
    // for an (unlikely) event of this being hit in distant future.
    NOTIMPLEMENTED_LOG_ONCE();
  }
  return operation;
}

void AddString(const PlatformClipboard::Data& data,
               OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data.empty())
    return;

  os_exchange_data->SetString(base::UTF8ToUTF16(BytesTo<std::string>(data)));
}

void AddHtml(const PlatformClipboard::Data& data,
             OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data.empty())
    return;

  os_exchange_data->SetHtml(base::UTF8ToUTF16(BytesTo<std::string>(data)),
                            GURL());
}

// Parses |data| as if it had text/uri-list format.  Its brief spec is:
// 1.  Any lines beginning with the '#' character are comment lines.
// 2.  Non-comment lines shall be URIs (URNs or URLs).
// 3.  Lines are terminated with a CRLF pair.
// 4.  URL encoding is used.
void AddFiles(const PlatformClipboard::Data& data,
              OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  std::string data_as_string = BytesTo<std::string>(data);

  const auto lines = base::SplitString(
      data_as_string, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<FileInfo> filenames;
  for (const auto& line : lines) {
    if (line.empty() || line[0] == '#')
      continue;
    GURL url(line);
    if (!url.is_valid() || !url.SchemeIsFile()) {
      LOG(WARNING) << "Invalid URI found: " << line;
      continue;
    }

    std::string url_path = url.path();
    url::RawCanonOutputT<base::char16> unescaped;
    url::DecodeURLEscapeSequences(url_path.data(), url_path.size(),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &unescaped);

    std::string path8;
    base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &path8);
    const base::FilePath path(path8);
    filenames.push_back({path, path.BaseName()});
  }
  if (filenames.empty())
    return;

  os_exchange_data->SetFilenames(filenames);
}

// Parses |data| as if it had text/x-moz-url format, which is basically
// two lines separated with newline, where the first line is the URL and
// the second one is page title.  The unpleasant feature of text/x-moz-url is
// that the URL has UTF-16 encoding.
void AddUrl(const PlatformClipboard::Data& data,
            OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data.empty())
    return;

  base::string16 data_as_string16 = BytesTo<base::string16>(data);

  const auto lines =
      base::SplitString(data_as_string16, base::ASCIIToUTF16("\r\n"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (lines.size() != 2U) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; it must contain "
                 << "exactly 2 lines but has " << lines.size() << " instead.";
    return;
  }
  GURL url(lines[0]);
  if (!url.is_valid()) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; the first line "
                 << "must contain a valid URL but it doesn't.";
    return;
  }

  os_exchange_data->SetURL(url, lines[1]);
}

void AddToOSExchangeData(const PlatformClipboard::Data& data,
                         const std::string& mime_type,
                         OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);
  if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUtf8)) {
    DCHECK(!os_exchange_data->HasString());
    AddString(data, os_exchange_data);
  } else if (mime_type == kMimeTypeHTML) {
    DCHECK(!os_exchange_data->HasHtml());
    AddHtml(data, os_exchange_data);
  } else if (mime_type == kMimeTypeMozillaURL) {
    DCHECK(!os_exchange_data->HasURL(kFilenameToURLPolicy));
    AddUrl(data, os_exchange_data);
  } else if (mime_type == kMimeTypeURIList) {
    DCHECK(!os_exchange_data->HasFile());
    AddFiles(data, os_exchange_data);
  } else {
    LOG(WARNING) << "Unhandled MIME type: " << mime_type;
  }
}

}  // namespace

// static
WaylandDataDevice::WaylandDataDevice(WaylandConnection* connection,
                                     wl_data_device* data_device)
    : internal::WaylandDataDeviceBase(connection), data_device_(data_device) {
  static const struct wl_data_device_listener kDataDeviceListener = {
      WaylandDataDevice::OnDataOffer, WaylandDataDevice::OnEnter,
      WaylandDataDevice::OnLeave,     WaylandDataDevice::OnMotion,
      WaylandDataDevice::OnDrop,      WaylandDataDevice::OnSelection};
  wl_data_device_add_listener(data_device_.get(), &kDataDeviceListener, this);
}

WaylandDataDevice::~WaylandDataDevice() = default;

void WaylandDataDevice::RequestDragData(
    const std::string& mime_type,
    base::OnceCallback<void(const PlatformClipboard::Data&)> callback) {
  base::ScopedFD fd = drag_offer_->Receive(mime_type);
  if (!fd.is_valid()) {
    LOG(ERROR) << "Failed to open file descriptor.";
    return;
  }

  // Ensure there is not pending operation to be performed by the compositor,
  // otherwise read(..) can block awaiting data to be sent to pipe.
  RegisterDeferredReadClosure(base::BindOnce(
      &WaylandDataDevice::ReadDragDataFromFD, base::Unretained(this),
      std::move(fd), std::move(callback)));
  RegisterDeferredReadCallback();
}

void WaylandDataDevice::DeliverDragData(const std::string& mime_type,
                                        std::string* buffer) {
  DCHECK(buffer);
  DCHECK(source_data_);

  if (mime_type == kMimeTypeMozillaURL &&
      source_data_->HasURL(kFilenameToURLPolicy)) {
    GURL url;
    base::string16 title;
    source_data_->GetURLAndTitle(kFilenameToURLPolicy, &url, &title);
    buffer->append(url.spec());
  } else if (mime_type == kMimeTypeHTML && source_data_->HasHtml()) {
    base::string16 data;
    GURL base_url;
    source_data_->GetHtml(&data, &base_url);
    buffer->append(base::UTF16ToUTF8(data));
  } else if (source_data_->HasString()) {
    base::string16 data;
    source_data_->GetString(&data);
    buffer->append(base::UTF16ToUTF8(data));
  } else {
    LOG(WARNING) << "Cannot deliver data of type " << mime_type
                 << " and no text representation is available.";
  }
}

void WaylandDataDevice::StartDrag(wl_data_source* data_source,
                                  const ui::OSExchangeData& data) {
  DCHECK(data_source);

  WaylandWindow* window =
      connection()->wayland_window_manager()->GetCurrentFocusedWindow();
  if (!window) {
    LOG(ERROR) << "Failed to get focused window.";
    return;
  }
  const SkBitmap* icon = PrepareDragIcon(data);
  source_data_ = std::make_unique<ui::OSExchangeData>(data.provider().Clone());
  wl_data_device_start_drag(data_device_.get(), data_source, window->surface(),
                            icon_surface_.get(), connection()->serial());
  if (icon)
    DrawDragIcon(icon);
  connection()->ScheduleFlush();
}

void WaylandDataDevice::ResetSourceData() {
  source_data_.reset();
}

void WaylandDataDevice::ReadDragDataFromFD(
    base::ScopedFD fd,
    base::OnceCallback<void(const PlatformClipboard::Data&)> callback) {
  PlatformClipboard::Data contents;
  wl::ReadDataFromFD(std::move(fd), &contents);
  std::move(callback).Run(contents);
}

void WaylandDataDevice::HandleDeferredLeaveIfNeeded() {
  if (!is_leaving_)
    return;

  OnLeave(this, data_device_.get());
}

// static
void WaylandDataDevice::OnDataOffer(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);

  self->connection()->clipboard()->UpdateSequenceNumber(
      ClipboardBuffer::kCopyPaste);

  DCHECK(!self->new_offer_);
  self->new_offer_ = std::make_unique<WaylandDataOffer>(offer);
}

void WaylandDataDevice::OnEnter(void* data,
                                wl_data_device* data_device,
                                uint32_t serial,
                                wl_surface* surface,
                                wl_fixed_t x,
                                wl_fixed_t y,
                                wl_data_offer* offer) {
  WaylandWindow* window =
      static_cast<WaylandWindow*>(wl_surface_get_user_data(surface));
  if (!window) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self->new_offer_);
  DCHECK(!self->drag_offer_);
  self->drag_offer_ = std::move(self->new_offer_);
  self->window_ = window;

  // TODO(crbug.com/1004715): Set mime type the client can accept.  Now it sets
  // all mime types offered because current implementation doesn't decide
  // action based on mime type.
  self->unprocessed_mime_types_.clear();
  for (auto mime : self->drag_offer_->mime_types()) {
    self->unprocessed_mime_types_.push_back(mime);
    self->drag_offer_->Accept(serial, mime);
  }

  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));

  // If |source_data_| is set, it means that dragging is started from the
  // same window and it's not needed to read data through Wayland.
  std::unique_ptr<OSExchangeData> dragged_data;
  if (!self->IsDraggingExternalData())
    dragged_data = std::make_unique<OSExchangeData>(
        self->source_data_->provider().Clone());
  self->window_->OnDragEnter(
      point, std::move(dragged_data),
      GetPossibleActions(self->drag_offer_->source_actions(),
                         self->drag_offer_->dnd_action()));
}

void WaylandDataDevice::OnMotion(void* data,
                                 wl_data_device* data_device,
                                 uint32_t time,
                                 wl_fixed_t x,
                                 wl_fixed_t y) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  gfx::PointF point(wl_fixed_to_double(x), wl_fixed_to_double(y));
  int client_operation = self->window_->OnDragMotion(
      point, time,
      GetPossibleActions(self->drag_offer_->source_actions(),
                         self->drag_offer_->dnd_action()));
  self->SetOperation(client_operation);
}

void WaylandDataDevice::OnDrop(void* data, wl_data_device* data_device) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }
  if (self->IsDraggingExternalData()) {
    // We are about to accept data dragged from another application.
    // Reading all the data may take some time so we set
    // |is_handling_dropped_data_| that will postpone handling of OnLeave
    // until reading is completed.
    self->is_handling_dropped_data_ = true;
    self->received_data_ = std::make_unique<OSExchangeData>(
        std::make_unique<OSExchangeDataProviderAura>());
    self->HandleUnprocessedMimeTypes();
  } else {
    // If the drag session had been started internally by chromium,
    // |source_data_| already holds the data, and it is already forwarded to the
    // delegate through OnDragEnter, so here we short-cut the data transfer by
    // sending nullptr.
    self->HandleReceivedData(nullptr);
  }
}

void WaylandDataDevice::OnLeave(void* data, wl_data_device* data_device) {
  // While reading data, it could get OnLeave event. We don't handle OnLeave
  // event directly if |is_handling_dropped_data_| is set.
  auto* self = static_cast<WaylandDataDevice*>(data);
  if (!self->window_) {
    LOG(ERROR) << "Failed to get window.";
    return;
  }

  if (self->is_handling_dropped_data_) {
    self->is_leaving_ = true;
    return;
  }

  self->window_->OnDragLeave();
  self->window_ = nullptr;
  self->drag_offer_.reset();
  self->is_handling_dropped_data_ = false;
  self->is_leaving_ = false;
}

// static
void WaylandDataDevice::OnSelection(void* data,
                                    wl_data_device* data_device,
                                    wl_data_offer* offer) {
  auto* self = static_cast<WaylandDataDevice*>(data);
  DCHECK(self);

  // 'offer' will be null to indicate that the selection is no longer valid,
  // i.e. there is no longer clipboard data available to paste.
  if (!offer) {
    self->ResetDataOffer();

    // Clear Clipboard cache.
    self->connection()->clipboard()->SetData({}, {});
    return;
  }

  DCHECK(self->new_offer_);
  self->set_data_offer(std::move(self->new_offer_));

  self->data_offer()->EnsureTextMimeTypeIfNeeded();
}

const SkBitmap* WaylandDataDevice::PrepareDragIcon(const OSExchangeData& data) {
  const SkBitmap* icon_bitmap = data.provider().GetDragImage().bitmap();
  if (!icon_bitmap || icon_bitmap->empty())
    return nullptr;
  icon_surface_.reset(wl_compositor_create_surface(connection()->compositor()));
  DCHECK(icon_surface_);
  return icon_bitmap;
}

void WaylandDataDevice::DrawDragIcon(const SkBitmap* icon_bitmap) {
  DCHECK(icon_bitmap);
  DCHECK(!icon_bitmap->empty());
  gfx::Size size(icon_bitmap->width(), icon_bitmap->height());

  if (!shm_buffer_ || shm_buffer_->size() != size) {
    shm_buffer_ = std::make_unique<WaylandShmBuffer>(connection()->shm(), size);
    if (!shm_buffer_->IsValid()) {
      LOG(ERROR) << "Failed to create drag icon buffer.";
      return;
    }
  }
  wl::DrawBitmap(*icon_bitmap, shm_buffer_.get());

  wl_surface* surface = icon_surface_.get();
  wl_surface_attach(surface, shm_buffer_->get(), 0, 0);
  wl_surface_damage(surface, 0, 0, size.width(), size.height());
  wl_surface_commit(surface);
}

void WaylandDataDevice::HandleUnprocessedMimeTypes() {
  std::string mime_type = SelectNextMimeType();
  if (mime_type.empty()) {
    HandleReceivedData(std::move(received_data_));
  } else {
    RequestDragData(mime_type,
                    base::BindOnce(&WaylandDataDevice::OnDragDataReceived,
                                   base::Unretained(this)));
  }
}

void WaylandDataDevice::OnDragDataReceived(
    const PlatformClipboard::Data& contents) {
  if (!contents.empty()) {
    AddToOSExchangeData(contents, unprocessed_mime_types_.front(),
                        received_data_.get());
  }

  unprocessed_mime_types_.pop_front();

  // Continue reading data for other negotiated mime types.
  HandleUnprocessedMimeTypes();
}

void WaylandDataDevice::HandleReceivedData(
    std::unique_ptr<ui::OSExchangeData> received_data) {
  unprocessed_mime_types_.clear();

  window_->OnDragDrop(std::move(received_data));
  drag_offer_->FinishOffer();
  is_handling_dropped_data_ = false;
  HandleDeferredLeaveIfNeeded();
}

std::string WaylandDataDevice::SelectNextMimeType() {
  while (!unprocessed_mime_types_.empty()) {
    const std::string& mime_type = unprocessed_mime_types_.front();
    if ((mime_type == kMimeTypeText || mime_type == kMimeTypeTextUtf8) &&
        !received_data_->HasString()) {
      return mime_type;
    }
    if (mime_type == kMimeTypeURIList && !received_data_->HasFile()) {
      return mime_type;
    }
    if (mime_type == kMimeTypeMozillaURL &&
        !received_data_->HasURL(kFilenameToURLPolicy)) {
      return mime_type;
    }
    if (mime_type == kMimeTypeHTML && !received_data_->HasHtml()) {
      return mime_type;
    }
    unprocessed_mime_types_.pop_front();
  }
  return {};
}

void WaylandDataDevice::SetOperation(const int operation) {
  uint32_t dnd_actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
  uint32_t preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  if (operation & DragDropTypes::DRAG_COPY) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  }

  if (operation & DragDropTypes::DRAG_MOVE) {
    dnd_actions |= WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    if (preferred_action == WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE)
      preferred_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
  }
  drag_offer_->SetAction(dnd_actions, preferred_action);
}

}  // namespace ui
