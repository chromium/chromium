// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_clipboard.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device.h"
#include "ui/ozone/platform/wayland/host/gtk_primary_selection_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device.h"
#include "ui/ozone/platform/wayland/host/wayland_data_device_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source.h"
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
  // data is expected to arrive through WaylandClipboard::SetData().
  // TODO(nickdiego): Decouple DataDevice impls from WaylandClipboard.
  virtual bool Read(const std::string& mime_type) = 0;

  // Synchronously stores and announces |data| as available from this clipboard.
  virtual void Write(const ui::PlatformClipboard::DataMap* data) = 0;

  // Tells if this clipboard instance is the current selection owner.
  virtual bool IsSelectionOwner() const = 0;
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
  explicit ClipboardImpl(Manager* manager) : manager_(manager) {}
  ClipboardImpl(const ClipboardImpl&) = delete;
  ClipboardImpl& operator=(const ClipboardImpl&) = delete;
  virtual ~ClipboardImpl() = default;

  virtual bool Read(const std::string& mime_type) override {
    return GetDevice()->RequestSelectionData(mime_type);
  }

  std::vector<std::string> ReadMimeTypes() override {
    return GetDevice()->GetAvailableMimeTypes();
  }

  virtual void Write(const ui::PlatformClipboard::DataMap* data) override {
    if (!data || data->empty()) {
      data_.clear();
      source_.reset();
    } else {
      data_ = *data;
      source_ = manager_->CreateSource(this);
      source_->Offer(GetMimeTypes());
      GetDevice()->SetSelectionSource(source_.get());
    }
  }

  bool IsSelectionOwner() const override { return !!source_; }

 private:
  DataDevice* GetDevice() { return manager_->GetDevice(); }

  std::vector<std::string> GetMimeTypes() {
    std::vector<std::string> mime_types;
    for (const auto& data : data_) {
      mime_types.push_back(data.first);
      if (data.first == ui::kMimeTypeText)
        mime_types.push_back(ui::kMimeTypeTextUtf8);
    }
    return mime_types;
  }

  // WaylandDataSource::Delegate:
  void OnDataSourceFinish(bool completed) override {
    if (!completed)
      Write(nullptr);
  }

  void OnDataSourceSend(const std::string& mime_type,
                        std::string* contents) override {
    DCHECK(contents);
    auto it = data_.find(mime_type);
    if (it == data_.end() && mime_type == ui::kMimeTypeTextUtf8)
      it = data_.find(ui::kMimeTypeText);
    if (it != data_.end())
      contents->assign(it->second->data().begin(), it->second->data().end());
  }

  // The device manager used to access data device and create data sources.
  Manager* const manager_;

  // The current data source used to offer clipboard data.
  std::unique_ptr<DataSource> source_;

  // The data currently stored in a given clipboard buffer.
  ui::PlatformClipboard::DataMap data_;
};

}  // namespace wl

namespace ui {

WaylandClipboard::WaylandClipboard(WaylandConnection* connection,
                                   WaylandDataDeviceManager* manager)
    : connection_(connection),
      copypaste_clipboard_(
          std::make_unique<wl::ClipboardImpl<WaylandDataDeviceManager>>(
              manager)) {
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
    PlatformClipboard::DataMap* data_map,
    PlatformClipboard::RequestDataClosure callback) {
  DCHECK(data_map);
  data_map_ = data_map;
  read_clipboard_closure_ = std::move(callback);
  auto* clipboard = GetClipboard(buffer);
  if (!clipboard || !clipboard->Read(mime_type)) {
    SetData(scoped_refptr<base::RefCountedBytes>(new base::RefCountedBytes()),
            mime_type);
  }
}

bool WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer) {
  if (auto* clipboard = GetClipboard(buffer))
    return clipboard->IsSelectionOwner();
  return false;
}

void WaylandClipboard::SetSequenceNumberUpdateCb(
    PlatformClipboard::SequenceNumberUpdateCb cb) {
  CHECK(update_sequence_cb_.is_null())
      << " The callback can be installed only once.";
  update_sequence_cb_ = std::move(cb);
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
  return (connection_->primary_selection_device_manager() != nullptr);
}

void WaylandClipboard::SetData(PlatformClipboard::Data contents,
                               const std::string& mime_type) {
  if (!data_map_)
    return;

  DCHECK(contents);
  (*data_map_)[mime_type] = contents;

  if (!read_clipboard_closure_.is_null()) {
    auto it = data_map_->find(mime_type);
    DCHECK(it != data_map_->end());
    std::move(read_clipboard_closure_).Run(it->second);
  }
  data_map_ = nullptr;
}

void WaylandClipboard::UpdateSequenceNumber(ClipboardBuffer buffer) {
  if (!update_sequence_cb_.is_null())
    update_sequence_cb_.Run(buffer);
}

wl::Clipboard* WaylandClipboard::GetClipboard(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    return copypaste_clipboard_.get();

  if (buffer == ClipboardBuffer::kSelection) {
    if (auto* manager = connection_->primary_selection_device_manager()) {
      if (!primary_selection_clipboard_) {
        primary_selection_clipboard_ = std::make_unique<
            wl::ClipboardImpl<GtkPrimarySelectionDeviceManager>>(manager);
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
