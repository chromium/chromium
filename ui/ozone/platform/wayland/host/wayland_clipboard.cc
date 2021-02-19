// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/optional.h"
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
  virtual ~Clipboard() = default;

  // Synchronously retrieves the mime types list currently available to be read.
  virtual std::vector<std::string> ReadMimeTypes() = 0;

  // Asynchronously reads clipboard content with |mime_type| format. The result
  // data is expected to arrive through OnSelectionDataReceived() callback.
  virtual bool Read(const std::string& mime_type,
                    ui::PlatformClipboard::DataMap* data_map,
                    ui::PlatformClipboard::RequestDataClosure callback) = 0;

  // Synchronously stores and announces |data| as available from this clipboard.
  virtual void Write(const ui::PlatformClipboard::DataMap* data) = 0;

  // Tells if this clipboard instance is the current selection owner.
  virtual bool IsSelectionOwner() const = 0;

  // Sets the callback in charge of updating the clipboard sequence number.
  virtual void SetSequenceNumberUpdateCb(
      ui::PlatformClipboard::SequenceNumberUpdateCb callback) = 0;
};

// Templated wl::Clipboard implementation. Whereas DataSource is the data source
// class capable of creating data offers upon clipboard writes and communicates
// events through DataSource::Delegate, and DataDevice is its device counterpart
// providing read and write access to the underlying data selection-related
// protocol objects. See *_data_{source,device}.h for more details.
template <typename Manager,
          typename DataSource = typename Manager::DataSource,
          typename DataDevice = typename Manager::DataDevice>
class ClipboardImpl final : public Clipboard,
                            public DataSource::Delegate,
                            public DataDevice::SelectionDelegate {
 public:
  explicit ClipboardImpl(Manager* manager, ui::ClipboardBuffer buffer)
      : manager_(manager), buffer_(buffer) {
    GetDevice()->set_selection_delegate(this);
  }
  ~ClipboardImpl() final { GetDevice()->set_selection_delegate(nullptr); }

  ClipboardImpl(const ClipboardImpl&) = delete;
  ClipboardImpl& operator=(const ClipboardImpl&) = delete;

  // TODO(crbug.com/1165466): Support nested clipboard requests.
  bool Read(const std::string& mime_type,
            ui::PlatformClipboard::DataMap* data_map,
            ui::PlatformClipboard::RequestDataClosure callback) final {
    DCHECK(data_map);
    received_data_ = data_map;
    read_clipboard_closure_ = std::move(callback);

    if (GetDevice()->RequestSelectionData(mime_type))
      return true;
    SetData(base::MakeRefCounted<base::RefCountedBytes>(), mime_type);
    return false;
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
  void Write(const ui::PlatformClipboard::DataMap* data) final {
    if (!data || data->empty()) {
      offered_data_.clear();
      source_.reset();
    } else {
      offered_data_ = *data;
      source_ = manager_->CreateSource(this);
      source_->Offer(GetMimeTypes());
      GetDevice()->SetSelectionSource(source_.get());
    }
  }

  bool IsSelectionOwner() const final { return !!source_; }

  void SetSequenceNumberUpdateCb(
      ui::PlatformClipboard::SequenceNumberUpdateCb callback) final {
    CHECK(update_sequence_cb_.is_null())
        << "The callback can be installed only once.";
    update_sequence_cb_ = callback;
  }

 private:
  DataDevice* GetDevice() { return manager_->GetDevice(); }

  std::vector<std::string> GetMimeTypes() {
    std::vector<std::string> mime_types;
    for (const auto& data : offered_data_) {
      mime_types.push_back(data.first);
      if (data.first == ui::kMimeTypeText)
        mime_types.push_back(ui::kMimeTypeTextUtf8);
    }
    return mime_types;
  }

  void SetData(ui::PlatformClipboard::Data contents,
               const std::string& mime_type) {
    if (!received_data_)
      return;

    DCHECK(contents);
    (*received_data_)[mime_type] = contents;

    if (!read_clipboard_closure_.is_null()) {
      auto it = received_data_->find(mime_type);
      DCHECK(it != received_data_->end());
      std::move(read_clipboard_closure_).Run(it->second);
    }
    received_data_ = nullptr;
  }

  // WaylandDataDeviceBase::SelectionDelegate:
  void OnSelectionOffer(ui::WaylandDataOfferBase* offer) final {
    if (!update_sequence_cb_.is_null())
      update_sequence_cb_.Run(buffer_);

    if (!offer)
      SetData({}, {});
  }

  void OnSelectionDataReceived(const std::string& mime_type,
                               ui::PlatformClipboard::Data contents) final {
    SetData(contents, mime_type);
  }

  // WaylandDataSource::Delegate:
  void OnDataSourceFinish(bool completed) override {
    if (!completed)
      Write(nullptr);
  }

  void OnDataSourceSend(const std::string& mime_type,
                        std::string* contents) override {
    DCHECK(contents);
    auto it = offered_data_.find(mime_type);
    if (it == offered_data_.end() && mime_type == ui::kMimeTypeTextUtf8)
      it = offered_data_.find(ui::kMimeTypeText);
    if (it != offered_data_.end())
      contents->assign(it->second->data().begin(), it->second->data().end());
  }

  // The device manager used to access data device and create data sources.
  Manager* const manager_;

  // The clipboard buffer managed by this |this|.
  const ui::ClipboardBuffer buffer_;

  // The current data source used to offer clipboard data.
  std::unique_ptr<DataSource> source_;

  // The data currently stored in a given clipboard buffer.
  ui::PlatformClipboard::DataMap offered_data_;

  // Holds a temporary instance of the client's clipboard content
  // so that we can asynchronously write to it.
  ui::PlatformClipboard::DataMap* received_data_ = nullptr;

  // Stores the callback to be invoked upon data reading from clipboard.
  ui::PlatformClipboard::RequestDataClosure read_clipboard_closure_;

  // Notifies when clipboard sequence must change. Can be empty if not set.
  ui::PlatformClipboard::SequenceNumberUpdateCb update_sequence_cb_;
};

}  // namespace wl

namespace ui {

WaylandClipboard::WaylandClipboard(WaylandConnection* connection,
                                   WaylandDataDeviceManager* manager)
    : connection_(connection),
      copypaste_clipboard_(
          std::make_unique<wl::ClipboardImpl<WaylandDataDeviceManager>>(
              manager,
              ClipboardBuffer::kCopyPaste)) {
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

// TODO(crbug.com/1165466): Support nested clipboard requests.
void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  if (auto* clipboard = GetClipboard(buffer))
    clipboard->Read(mime_type, data_map, std::move(callback));
  else
    std::move(callback).Run(base::nullopt);
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  if (auto* clipboard = GetClipboard(buffer))
    return clipboard->IsSelectionOwner();
  return false;
}

void WaylandClipboard::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  copypaste_clipboard_->SetSequenceNumberUpdateCb(cb);
  if (auto* selection_clipboard = GetClipboard(ClipboardBuffer::kSelection))
    selection_clipboard->SetSequenceNumberUpdateCb(cb);
}

void WaylandClipboard::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  std::vector<std::string> mime_types;
  if (auto* clipboard = GetClipboard(buffer))
    mime_types = clipboard->ReadMimeTypes();
  std::move(callback).Run(mime_types);
}

bool WaylandClipboard::IsSelectionBufferAvailable() const {
  return (connection_->zwp_primary_selection_device_manager() != nullptr) ||
         (connection_->gtk_primary_selection_device_manager() != nullptr);
}

wl::Clipboard* WaylandClipboard::GetClipboard(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return copypaste_clipboard_.get();

  if (buffer == ClipboardBuffer::kSelection) {
    if (auto* manager = connection_->zwp_primary_selection_device_manager()) {
      if (!primary_selection_clipboard_) {
        primary_selection_clipboard_ = std::make_unique<
            wl::ClipboardImpl<ZwpPrimarySelectionDeviceManager>>(
            manager, ClipboardBuffer::kSelection);
      }
      return primary_selection_clipboard_.get();
    } else if (auto* manager =
                   connection_->gtk_primary_selection_device_manager()) {
      if (!primary_selection_clipboard_) {
        primary_selection_clipboard_ = std::make_unique<
            wl::ClipboardImpl<GtkPrimarySelectionDeviceManager>>(
            manager, ClipboardBuffer::kSelection);
      }
      return primary_selection_clipboard_.get();
    }
    // Primary selection extension not available.
    return nullptr;
  }

  NOTREACHED() << "Unsupported clipboard buffer: " << static_cast<int>(buffer);
  return nullptr;
}

}  // namespace ui
