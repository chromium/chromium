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
#include "base/strings/string_view_util.h"
#include "build/build_config.h"
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

#if BUILDFLAG(IS_LINUX)
#include "base/strings/string_util.h"
#include "ui/base/clipboard/clipboard_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"
#endif

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

  // Retrieves the mime types list currently available to be read.
  virtual std::vector<std::string> ReadMimeTypes() = 0;

  // Asynchronously reads and returns clipboard content with |mime_type| format.
  virtual void Read(const std::string& mime_type,
                    ui::PlatformClipboard::RequestDataClosure callback) = 0;

  // Asynchronously reads and returns file transfer portal content.
  virtual void ReadFileTransfer(
      ui::PlatformClipboard::RequestDataClosure callback) = 0;

  // Synchronously stores and announces |data| as available from this clipboard.
  // Portals are registered asynchronously internally.
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

  void Read(const std::string& mime_type,
            ui::PlatformClipboard::RequestDataClosure callback) final {
    GetDevice()->RequestSelectionData(GetMimeTypeForRequest(mime_type),
                                      std::move(callback));
  }

  void ReadFileTransfer(
      ui::PlatformClipboard::RequestDataClosure callback) final {
#if BUILDFLAG(IS_LINUX)
    // Prefer portal types
    std::string mime_type;
    auto available_types = GetDevice()->GetAvailableMimeTypes();
    for (const auto& type : available_types) {
      if (type == ui::kMimeTypePortalFileTransfer) {
        mime_type = ui::kMimeTypePortalFileTransfer;
        break;
      }
      if (type == ui::kMimeTypePortalFiles) {
        mime_type = ui::kMimeTypePortalFiles;
        // Keep looking in case kMimeTypePortalFileTransfer is also there
      }
    }

    if (!mime_type.empty()) {
      GetDevice()->RequestSelectionData(
          mime_type, base::BindOnce(
                         [](base::WeakPtr<ClipboardImpl> self,
                            ui::PlatformClipboard::RequestDataClosure callback,
                            const ui::PlatformClipboard::Data& data) {
                           if (self) {
                             self->OnPortalKeyRead(std::move(callback), data);
                           } else {
                             std::move(callback).Run(nullptr);
                           }
                         },
                         weak_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }
#endif
    std::move(callback).Run(nullptr);
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
      FinishWrite(GetSerial());
    }

    NotifyClipboardChanged();
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
      if (data.first == ui::kMimeTypePlainText) {
        mime_types.push_back(ui::kMimeTypeUtf8PlainText);
      }
    }

#if BUILDFLAG(IS_LINUX)
    if (offered_data_.contains(ui::kMimeTypeUriList)) {
      if (!offered_data_.contains(ui::kMimeTypePortalFileTransfer)) {
        mime_types.push_back(ui::kMimeTypePortalFileTransfer);
      }
      if (!offered_data_.contains(ui::kMimeTypePortalFiles)) {
        mime_types.push_back(ui::kMimeTypePortalFiles);
      }
    }
#endif

    return mime_types;
  }

  std::string GetMimeTypeForRequest(const std::string& mime_type) {
    if (mime_type != ui::kMimeTypePlainText) {
      return mime_type;
    }
    // Prioritize unicode for text data.
    for (const auto& t : GetDevice()->GetAvailableMimeTypes()) {
      if (t == ui::kMimeTypeUtf8PlainText || t == ui::kMimeTypeLinuxString ||
          t == ui::kMimeTypeLinuxUtf8String || t == ui::kMimeTypeLinuxText) {
        return t;
      }
    }
    return mime_type;
  }

  void HandleNewSelectionOffer(ui::WaylandDataOfferBase* offer) const {
    if (IsSelectionOwner())
      return;

    NotifyClipboardChanged();
  }

#if BUILDFLAG(IS_LINUX)
  void OnPortalKeyRead(ui::PlatformClipboard::RequestDataClosure callback,
                       const ui::PlatformClipboard::Data& data) {
    if (!data) {
      std::move(callback).Run(nullptr);
      return;
    }
    ui::clipboard_util::ExtractPathsFromPortalKey(
        base::as_byte_span(*data),
        base::BindOnce(&ClipboardImpl::OnPathsExtracted,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OnPathsExtracted(ui::PlatformClipboard::RequestDataClosure callback,
                        std::vector<std::string> paths) {
    if (paths.empty()) {
      std::move(callback).Run(nullptr);
      return;
    }
    // Convert to uri-list for internal clipboard consumption
    std::string uri_list = ui::clipboard_util::GetUriListFromPaths(paths);
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedBytes>(
        base::as_byte_span(uri_list)));
  }
#endif

  void FinishWrite(std::optional<wl::Serial> serial) {
    source_ = manager_->CreateSource(this);
    source_->Offer(GetOfferedMimeTypes());

    if (serial.has_value()) {
      GetDevice()->SetSelectionSource(source_.get(), serial->value);
    } else {
      LOG(WARNING) << "No serial found for selection.";
    }
  }

  void NotifyClipboardChanged() const {
    if (!clipboard_changed_callback_.is_null()) {
      clipboard_changed_callback_.Run(buffer_);
    }
  }

  std::optional<wl::Serial> GetSerial() {
    return connection_->serial_tracker().GetSerial({wl::SerialType::kTouchPress,
                                                    wl::SerialType::kMousePress,
                                                    wl::SerialType::kKeyPress});
  }

  // WaylandDataSource::Delegate:
  void OnDataSourceFinish(DataSource* source,
                          base::TimeTicks timestamp,
                          bool completed) override {
    if (source == source_.get() && !completed) {
      Write(nullptr);
    }
  }

  void OnDataSourceSend(
      DataSource* source,
      const std::string& mime_type,
      typename DataSource::Delegate::ContentCallback callback) override {
#if BUILDFLAG(IS_LINUX)
    if (mime_type == ui::kMimeTypePortalFileTransfer ||
        mime_type == ui::kMimeTypePortalFiles) {
      auto it = offered_data_.find(ui::kMimeTypeUriList);
      if (it != offered_data_.end()) {
        std::vector<std::string> paths =
            ui::clipboard_util::GetPathsFromUriList(
                base::as_string_view(*it->second));

        ui::clipboard_util::RegisterPathsWithPortal(paths, std::move(callback));
        return;
      }
    }
#endif

    std::string contents;
    auto it = offered_data_.find(mime_type);
    if (it == offered_data_.end() && mime_type == ui::kMimeTypeUtf8PlainText) {
      it = offered_data_.find(ui::kMimeTypePlainText);
    }
    if (it != offered_data_.end()) {
      contents = std::string(base::as_string_view(*it->second));
    }
    std::move(callback).Run(std::move(contents));
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
    const PlatformClipboard::DataMap& data_map) {
  if (auto* clipboard = GetClipboard(buffer))
    clipboard->Write(&data_map);
}

void WaylandClipboard::RequestClipboardData(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    PlatformClipboard::RequestDataClosure callback) {
  auto* clipboard = GetClipboard(buffer);
  if (!clipboard) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (mime_type == kMimeTypeUriList) {
    auto portal_callback = base::BindOnce(
        [](base::WeakPtr<WaylandClipboard> self, ClipboardBuffer buffer,
           const std::string& mime_type,
           PlatformClipboard::RequestDataClosure callback,
           const PlatformClipboard::Data& portal_data) {
          if (portal_data) {
            std::move(callback).Run(portal_data);
            return;
          }
          if (self) {
            auto* cb = self->GetClipboard(buffer);
            cb->Read(mime_type, std::move(callback));
          } else {
            std::move(callback).Run(nullptr);
          }
        },
        weak_factory_.GetWeakPtr(), buffer, mime_type, std::move(callback));
    clipboard->ReadFileTransfer(std::move(portal_callback));
    return;
  }

  clipboard->Read(mime_type, std::move(callback));
}

void WaylandClipboard::GetAvailableMimeTypes(
    ClipboardBuffer buffer,
    PlatformClipboard::GetMimeTypesClosure callback) {
  std::vector<std::string> mime_types;
  if (auto* clipboard = GetClipboard(buffer))
    mime_types = clipboard->ReadMimeTypes();
  std::move(callback).Run(mime_types);
}

void WaylandClipboard::IsSelectionOwner(ClipboardBuffer buffer,
                                        IsSelectionOwnerClosure callback) {
  if (auto* clipboard = GetClipboard(buffer)) {
    std::move(callback).Run(clipboard->IsSelectionOwner());
    return;
  }
  std::move(callback).Run(false);
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

  NOTREACHED() << "Unsupported clipboard buffer: " << static_cast<int>(buffer);
}

}  // namespace ui
