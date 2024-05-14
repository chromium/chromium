// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_offer_base.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
#include "ui/ozone/platform/wayland/host/wayland_serial_tracker.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/zwp_primary_selection_device_manager.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace wl {

// Internal Wayland Clipboard interface. A wl::Clipboard implementation handles
// a single ui::ClipboardBuffer. With this common interface it is possible to
// seamlessly support different clipboard buffers backed by different underlying
// Wayland protocol objects.
class Clipboard {
 public:
  using ClipboardDataChangedCallback =
      ui::PlatformClipboard::ClipboardDataChangedCallback;

  virtual ~Clipboard() = default;

  // Synchronously retrieves the mime types list currently available to be read.
  virtual std::vector<std::string> ReadMimeTypes() = 0;

  // Synchronously reads and returns clipboard content with |mime_type| format.
  // TODO(crbug.com/40398800): Drop once Clipboard API becomes async.
  virtual ui::PlatformClipboard::Data Read(const std::string& mime_type) = 0;

  // Synchronously stores and announces |data| as available from this clipboard.
  virtual void Write(const ui::PlatformClipboard::DataMap* data) = 0;

  // Tells if this clipboard instance is the current selection owner.
  virtual bool IsSelectionOwner() const = 0;

  // Sets the callback to be executed when clipboard data changes.
  virtual void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback callback) = 0;
};

// Templated wl::Clipboard implementation. Whereas DataSource is the data source
// class capable of creating data offers upon clipboard writes and communicates
// events through DataSource::Delegate, and DataDevice is its device counterpart
// providing read and write access to the underlying data selection-related
// protocol objects. See *_data_{source,device}.h for more details.
template <typename Manager,
          typename DataSource = typename Manager::DataSource,
          typename DataDevice = typename Manager::DataDevice>
class ClipboardImpl final : public Clipboard, public DataSource::Delegate {
 public:
  ClipboardImpl(Manager* manager,
                ui::ClipboardBuffer buffer,
                ui::WaylandConnection* connection)
      : manager_(manager), buffer_(buffer), connection_(connection) {
    GetDevice()->set_selection_offer_callback(base::BindRepeating(
        &ClipboardImpl::HandleNewSelectionOffer, weak_factory_.GetWeakPtr()));
  }
  ~ClipboardImpl() final = default;

  ClipboardImpl(const ClipboardImpl&) = delete;
  ClipboardImpl& operator=(const ClipboardImpl&) = delete;

  ui::PlatformClipboard::Data Read(const std::string& mime_type) final {
    return GetDevice()->ReadSelectionData(GetMimeTypeForRequest(mime_type));
  }

  std::vector<std::string> ReadMimeTypes() final {
    return GetDevice()->GetAvailableMimeTypes();
  }

  // Once this client sends wl_data_source::offer, it is responsible for holding
  // onto its clipboard contents. At future points in time, the wayland server
  // may send a wl_data_source::send event, in which case this client is
  // responsible for writing the clipboard contents into the supplied fd. This
  // client can only drop the clipboard contents when it receives a
  // wl_data_source::cancelled event.
  //
  // This is supposedly responding to an input event, i.e: there is a valid
  // corresponding serial number (provided by wl::SerialTracker). Otherwise,
  // this function will no-op.
  void Write(const ui::PlatformClipboard::DataMap* data) final {
    if (!data || data->empty()) {
      offered_data_.clear();
      source_.reset();
    } else {
      offered_data_ = *data;
      source_ = manager_->CreateSource(this);
      source_->Offer(GetOfferedMimeTypes());

      // TODO(nickdiego): This function should just no-op if no serial is found
      // (ie: no recent input event has been processed yet), though several unit
      // and browser tests do not satisfy this precondition so would fail [1].
      // Revisit this once those tests are fixed.
      //
      // [1] https://chromium-review.googlesource.com/c/chromium/src/+/3527605/2
      auto& serial_tracker = connection_->serial_tracker();
      auto serial = serial_tracker.GetSerial({wl::SerialType::kTouchPress,
                                              wl::SerialType::kMousePress,
                                              wl::SerialType::kKeyPress});
      if (serial.has_value()) {
        GetDevice()->SetSelectionSource(source_.get(), serial->value);
      } else {
        LOG(WARNING) << "No serial found for selection.";
      }
    }

    if (!clipboard_changed_callback_.is_null())
      clipboard_changed_callback_.Run(buffer_);
  }

  bool IsSelectionOwner() const final { return !!source_; }

  void SetClipboardDataChangedCallback(
      ClipboardDataChangedCallback callback) final {
    clipboard_changed_callback_ = std::move(callback);
  }

 private:
  DataDevice* GetDevice() { return manager_->GetDevice(); }

  std::vector<std::string> GetOfferedMimeTypes() {
    std::vector<std::string> mime_types;
    for (const auto& data : offered_data_) {
      mime_types.push_back(data.first);
      if (data.first == ui::kMimeTypeText)
        mime_types.push_back(ui::kMimeTypeTextUtf8);
    }
    return mime_types;
  }

  std::string GetMimeTypeForRequest(const std::string& mime_type) {
    if (mime_type != ui::kMimeTypeText)
      return mime_type;
    // Prioritize unicode for text data.
    for (const auto& t : GetDevice()->GetAvailableMimeTypes()) {
      if (t == ui::kMimeTypeTextUtf8 || t == ui::kMimeTypeLinuxString ||
          t == ui::kMimeTypeLinuxUtf8String || t == ui::kMimeTypeLinuxText) {
        return t;
      }
    }
    return mime_type;
  }

  void HandleNewSelectionOffer(ui::WaylandDataOfferBase* offer) const {
    if (IsSelectionOwner())
      return;

    if (!clipboard_changed_callback_.is_null())
      clipboard_changed_callback_.Run(buffer_);
  }

  // WaylandDataSource::Delegate:
  void OnDataSourceFinish(DataSource* source,
                          base::TimeTicks timestamp,
                          bool completed) override {
    if (!completed)
      Write(nullptr);
  }

  void OnDataSourceSend(DataSource* source,
                        const std::string& mime_type,
                        std::string* contents) override {
    DCHECK(contents);
    auto it = offered_data_.find(mime_type);
    if (it == offered_data_.end() && mime_type == ui::kMimeTypeTextUtf8)
      it = offered_data_.find(ui::kMimeTypeText);
    if (it != offered_data_.end()) {
      *contents = base::as_string_view(*it->second);
    }
  }

  // The device manager used to access data device and create data sources.
  const raw_ptr<Manager> manager_;

  // The clipboard buffer managed by this |this|.
  const ui::ClipboardBuffer buffer_;

  // The current data source used to offer clipboard data.
  std::unique_ptr<DataSource> source_;

  // The data currently stored in a given clipboard buffer.
  ui::PlatformClipboard::DataMap offered_data_;

  // Notifies when clipboard data changes. Can be empty if not set.
  ClipboardDataChangedCallback clipboard_changed_callback_;

  const raw_ptr<ui::WaylandConnection> connection_;

  base::WeakPtrFactory<ClipboardImpl> weak_factory_{this};
};

}  // namespace wl

namespace ui {

WaylandClipboard::WaylandClipboard(WaylandConnection* connection,
                                   WaylandDataDeviceManager* manager)
    : connection_(connection),
      copypaste_clipboard_(
          std::make_unique<wl::ClipboardImpl<WaylandDataDeviceManager>>(
              manager,
              ClipboardBuffer::kCopyPaste,
              connection)) {
  DCHECK(manager);
  DCHECK(connection_);
  DCHECK(copypaste_clipboard_);
}

WaylandClipboard::~WaylandClipboard() = default;

void WaylandClipboard::OfferClipboardData(
    ClipboardBuffer buffer,
    const PlatformClipboard::DataMap& data_map,
    PlatformClipboard::OfferDataClosure callback) {
  if (auto* clipboard = GetClipboard(buffer))
    clipboard->Write(&data_map);
  std::move(callback).Run();
}

void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  PlatformClipboard::Data data;
  if (auto* clipboard = GetClipboard(buffer))
    data = clipboard->Read(mime_type);
  std::move(callback).Run(data);
}

void WaylandClipboard::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  std::vector<std::string> mime_types;
  if (auto* clipboard = GetClipboard(buffer))
    mime_types = clipboard->ReadMimeTypes();
  std::move(callback).Run(mime_types);
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  if (auto* clipboard = GetClipboard(buffer))
    return clipboard->IsSelectionOwner();
  return false;
}

void WaylandClipboard::SetClipboardDataChangedCallback(
    ClipboardDataChangedCallback data_changed_callback) {
  copypaste_clipboard_->SetClipboardDataChangedCallback(data_changed_callback);
  if (auto* selection_clipboard = GetClipboard(ClipboardBuffer::kSelection))
    selection_clipboard->SetClipboardDataChangedCallback(data_changed_callback);
}

bool WaylandClipboard::IsSelectionBufferAvailable() const {
  return (connection_->zwp_primary_selection_device_manager() != nullptr) ||
         (connection_->gtk_primary_selection_device_manager() != nullptr);
}

wl::Clipboard* WaylandClipboard::GetClipboard(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return copypaste_clipboard_.get();

  if (buffer == ClipboardBuffer::kSelection) {
    auto* zwp_manager = connection_->zwp_primary_selection_device_manager();
    if (zwp_manager) {
      if (!primary_selection_clipboard_) {
        primary_selection_clipboard_ = std::make_unique<
            wl::ClipboardImpl<ZwpPrimarySelectionDeviceManager>>(
            zwp_manager, ClipboardBuffer::kSelection, connection_);
      }
      return primary_selection_clipboard_.get();
    }
    auto* gtk_manager = connection_->gtk_primary_selection_device_manager();
    if (gtk_manager) {
      if (!primary_selection_clipboard_) {
        primary_selection_clipboard_ = std::make_unique<
            wl::ClipboardImpl<GtkPrimarySelectionDeviceManager>>(
            gtk_manager, ClipboardBuffer::kSelection, connection_);
      }
      return primary_selection_clipboard_.get();
    }
    // Primary selection extension not available.
    return nullptr;
  }

  NOTREACHED_IN_MIGRATION()
      << "Unsupported clipboard buffer: " << static_cast<int>(buffer);
  return nullptr;
}

}  // namespace ui
