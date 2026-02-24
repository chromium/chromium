// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "base/types/optional_util.h"
#include "base/types/variant_util.h"
#include "build/build_config.h"
#include "clipboard_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

namespace {

// The amount of time to wait for a request to complete before aborting it.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(1);

// Checks if DLP rules allow the clipboard read.
bool IsReadAllowed(std::optional<DataTransferEndpoint> data_src,
                   const DataTransferEndpoint* data_dst,
                   const base::span<uint8_t> data) {
  DataTransferPolicyController* policy_controller =
      DataTransferPolicyController::Get();

  if (!policy_controller || !data_src.has_value() || data.empty()) {
    return true;
  }

  bool is_allowed = policy_controller->IsClipboardReadAllowed(
      data_src, data_dst, data.size());
  return is_allowed;
}

std::vector<std::u16string> GetStandardFormatsFromMimeTypes(
    const std::vector<std::string>& mime_types) {
  std::vector<std::u16string> types;
  for (const auto& mime_type : mime_types) {
    if (mime_type == ClipboardFormatType::HtmlType().GetName() ||
        mime_type == ClipboardFormatType::SvgType().GetName() ||
        mime_type == ClipboardFormatType::RtfType().GetName() ||
        mime_type == ClipboardFormatType::BitmapType().GetName() ||
        mime_type == ClipboardFormatType::FilenamesType().GetName()) {
      types.push_back(base::UTF8ToUTF16(mime_type));
      continue;
    }
    // `WriteText` uses the following mime types for text, so if those types are
    // available, we add kMimeTypePlainText to the list.
    if ((mime_type == ClipboardFormatType::PlainTextType().GetName() ||
         mime_type == kMimeTypeLinuxText || mime_type == kMimeTypeLinuxString ||
         mime_type == kMimeTypeUtf8PlainText ||
         mime_type == kMimeTypeLinuxUtf8String) &&
        !std::ranges::contains(types, kMimeTypePlainText16)) {
      types.push_back(kMimeTypePlainText16);
      continue;
    }
  }
  return types;
}

// Depending on the backend, the platform clipboard may or may not be
// available.  Should it be absent, we provide a dummy one.  It always calls
// back immediately with empty data. It starts without ownership of any buffers
// but will take and keep ownership after a call to OfferClipboardData(). By
// taking ownership, we allow ClipboardOzone to return existing data in
// ReadClipboardDataAndWait().
class StubPlatformClipboard : public PlatformClipboard {
 public:
  StubPlatformClipboard() = default;
  ~StubPlatformClipboard() override = default;

  // PlatformClipboard:
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override {
    is_owner_[buffer] = true;
    std::move(callback).Run();
  }
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::RequestDataClosure callback) override {
    std::move(callback).Run({});
  }
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override {
    std::move(callback).Run({});
  }
  bool IsSelectionOwner(ClipboardBuffer buffer) override {
    return is_owner_[buffer];
  }
  void SetClipboardDataChangedCallback(
      PlatformClipboard::ClipboardDataChangedCallback cb) override {}
  bool IsSelectionBufferAvailable() const override { return false; }

 private:
  base::flat_map<ClipboardBuffer, bool> is_owner_;
};

}  // namespace

// A helper class that uses a request pattern to asynchronously communicate
// with the ozone::PlatformClipboard and fetch clipboard data with the
// specified MIME types.
class ClipboardOzone::AsyncClipboardOzone {
 public:
  explicit AsyncClipboardOzone(PlatformClipboard* platform_clipboard)
      : platform_clipboard_(platform_clipboard), weak_factory_(this) {
    DCHECK(platform_clipboard_);

    // Set a callback to listen to requests to increase the clipboard sequence
    // number.
    auto update_sequence_cb =
        base::BindRepeating(&AsyncClipboardOzone::OnClipboardDataChanged,
                            weak_factory_.GetWeakPtr());
    platform_clipboard_->SetClipboardDataChangedCallback(
        std::move(update_sequence_cb));
  }
  AsyncClipboardOzone(const AsyncClipboardOzone&) = delete;
  AsyncClipboardOzone& operator=(const AsyncClipboardOzone&) = delete;
  ~AsyncClipboardOzone() = default;

  void OnPreShutdown() { platform_clipboard_ = nullptr; }

  bool IsSelectionBufferAvailable() const {
    return platform_clipboard_->IsSelectionBufferAvailable();
  }

  std::vector<std::string> RequestMimeTypes(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return {};

    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_[buffer])
        mime_types.push_back(item.first);
      return mime_types;
    }

    return GetMimeTypes(buffer);
  }

  void PrepareForWriting() { data_to_offer_.clear(); }

  void OfferData(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return;
    Offer(buffer, std::move(data_to_offer_));
  }

  void Clear(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return;
    data_to_offer_.clear();
    OfferData(buffer);
  }

  void InsertData(std::vector<uint8_t> data,
                  const std::set<std::string>& mime_types) {
    auto wrapped_data =
        base::MakeRefCounted<base::RefCountedBytes>(std::move(data));
    for (const auto& mime_type : mime_types) {
      DCHECK_EQ(data_to_offer_.count(mime_type), 0U);
      data_to_offer_[mime_type] = wrapped_data;
    }
  }

  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const {
    return buffer == ClipboardBuffer::kCopyPaste ? clipboard_sequence_number_
                                                 : selection_sequence_number_;
  }

  void GetAvailableMimeTypesAsync(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) {
    if (buffer == ClipboardBuffer::kSelection &&
        !IsSelectionBufferAvailable()) {
      std::move(callback).Run({});
      return;
    }

    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      std::vector<std::string> mime_types;
      mime_types.reserve(offered_data_[buffer].size());
      for (const auto& item : offered_data_[buffer]) {
        mime_types.push_back(item.first);
      }
      std::move(callback).Run(mime_types);
      return;
    }

    platform_clipboard_->GetAvailableMimeTypes(buffer, std::move(callback));
  }

  void ReadClipboardDataAsync(ClipboardBuffer buffer,
                              const std::string& mime_type,
                              PlatformClipboard::RequestDataClosure callback) {
    if (buffer == ClipboardBuffer::kSelection &&
        !IsSelectionBufferAvailable()) {
      std::move(callback).Run(nullptr);
      return;
    }

    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      auto it = offered_data_[buffer].find(mime_type);
      if (it == offered_data_[buffer].end()) {
        std::move(callback).Run(nullptr);
        return;
      }
      std::move(callback).Run(it->second);
      return;
    }

    platform_clipboard_->RequestClipboardData(buffer, mime_type,
                                              std::move(callback));
  }

  void ReadSourceAsync(
      ClipboardBuffer buffer,
      base::OnceCallback<void(std::optional<DataTransferEndpoint>)> callback) {
    ReadClipboardDataAsync(
        buffer, kMimeTypeSourceUrl,
        base::BindOnce(&AsyncClipboardOzone::OnReadSourceAsync,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  std::optional<DataTransferEndpoint> ReadSourceAndWait(
      ClipboardBuffer buffer) {
    auto data = ReadClipboardDataAndWait(buffer, kMimeTypeSourceUrl);
    if (data.empty()) {
      return std::nullopt;
    }

    GURL url(std::string(data.begin(), data.end()));
    if (!url.is_valid()) {
      return std::nullopt;
    }

    return DataTransferEndpoint(std::move(url));
  }

  base::span<uint8_t> ReadClipboardDataAndWait(ClipboardBuffer buffer,
                                               const std::string& mime_type) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return {};

    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      auto it = offered_data_[buffer].find(mime_type);
      if (it == offered_data_[buffer].end())
        return {};
      return base::span(it->second->as_vector());
    }

    if (auto data = Read(buffer, mime_type))
      return base::span(data->as_vector());

    return {};
  }

 private:
  // Request<Result> encapsulates a clipboard request and provides a sync-like
  // way to perform it, whereas Result is the operation return type. Request
  // instances are created by factory functions (e.g: Read, Offer, GetMimeTypes,
  // etc), which are supposed to know how to bind them to actual calls to the
  // underlying platform clipboard instance. Factory functions usually create
  // them as local vars (ie: stack memory), and bind them to weak references,
  // through GetWeakPtr(), plumbing them into Finish* functions, which prevents
  // use-after-free issues in case, for example, a platform clipboard callback
  // would run with an already destroyed request instance.
  template <typename Result>
  class Request {
   public:
    enum class State { kStarted, kDone, kAborted };

    // Blocks until the request is done or aborted. The |result_| is returned if
    // the request succeeds, otherwise an empty value is returned.
    Result TakeResultSync() {
      // For a variety of reasons, it might already be done at this point,
      // depending on the platform clipboard implementation and the specific
      // request (e.g: cached values, sync request, etc).
      if (state_ == State::kDone)
        return std::move(result_);

      DCHECK_EQ(state_, State::kStarted);

      // TODO(crbug.com/40605786): this is known to be dangerous, and may cause
      // blocks in ui thread. But ui::Clipboard was designed with synchronous
      // APIs rather than asynchronous ones, which platform clipboards can
      // provide. E.g: X11 and Wayland.
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      quit_closure_ = run_loop.QuitClosure();

      // Set a timeout timer after which the request will be aborted.
      base::OneShotTimer abort_timer;
      abort_timer.Start(FROM_HERE, kRequestTimeout,
                        base::BindOnce(&Request::Abort, GetWeakPtr()));

      run_loop.Run();
      return std::move(result_);
    }

    void Finish(const Result& result) {
      DCHECK_EQ(state_, State::kStarted);
      state_ = State::kDone;
      result_ = result;
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
    }

    base::WeakPtr<Request<Result>> GetWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }

   private:
    void Abort() {
      DCHECK_EQ(state_, State::kStarted);
      Finish(Result{});
      state_ = State::kAborted;
    }

    // Keeps track of the request state.
    State state_ = State::kStarted;

    // Holds the sync loop quit closure.
    base::OnceClosure quit_closure_;

    // Stores the request result.
    Result result_ = {};

    base::WeakPtrFactory<Request<Result>> weak_factory_{this};
  };

  std::vector<std::string> GetMimeTypes(ClipboardBuffer buffer) {
    using MimeTypesRequest = Request<std::vector<std::string>>;
    MimeTypesRequest request;
    platform_clipboard_->GetAvailableMimeTypes(
        buffer,
        base::BindOnce(&MimeTypesRequest::Finish, request.GetWeakPtr()));
    return request.TakeResultSync();
  }

  PlatformClipboard::Data Read(ClipboardBuffer buffer,
                               const std::string& mime_type) {
    using ReadRequest = Request<PlatformClipboard::Data>;
    ReadRequest request;
    platform_clipboard_->RequestClipboardData(
        buffer, mime_type,
        base::BindOnce(&ReadRequest::Finish, request.GetWeakPtr()));
    return request.TakeResultSync().release();
  }

  void Offer(ClipboardBuffer buffer, PlatformClipboard::DataMap data_map) {
    using OfferRequest = Request<bool>;
    OfferRequest request;
    offered_data_[buffer] = data_map;
    platform_clipboard_->OfferClipboardData(
        buffer, data_map,
        base::BindOnce(&OfferRequest::Finish, request.GetWeakPtr(), true));
    request.TakeResultSync();
  }

  void OnClipboardDataChanged(ClipboardBuffer buffer) {
    DCHECK(buffer == ClipboardBuffer::kCopyPaste ||
           platform_clipboard_->IsSelectionBufferAvailable());
    if (buffer == ClipboardBuffer::kCopyPaste) {
      clipboard_sequence_number_ = ClipboardSequenceNumberToken();
      // Only notify clipboard observers for regular clipboard changes,
      // not primary selection changes (e.g., double-click text selection).
      ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
    } else {
      selection_sequence_number_ = ClipboardSequenceNumberToken();
    }
  }

  void OnReadSourceAsync(
      base::OnceCallback<void(std::optional<DataTransferEndpoint>)> callback,
      const PlatformClipboard::Data& data) {
    if (!data || data->as_vector().empty()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    GURL url(
        std::string(reinterpret_cast<const char*>(data->data()), data->size()));
    if (!url.is_valid()) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    std::move(callback).Run(DataTransferEndpoint(std::move(url)));
  }

  bool IsBufferSupported(ClipboardBuffer buffer) const {
    return buffer == ClipboardBuffer::kCopyPaste ||
           platform_clipboard_->IsSelectionBufferAvailable();
  }

  // Clipboard data accumulated for writing.
  PlatformClipboard::DataMap data_to_offer_;

  // Clipboard data that had been offered most recently.  Used as a cache to
  // read data if we still own it.
  base::flat_map<ClipboardBuffer, PlatformClipboard::DataMap> offered_data_;

  // Provides communication to a system clipboard under ozone level.
  raw_ptr<PlatformClipboard, DanglingUntriaged> platform_clipboard_ = nullptr;

  ClipboardSequenceNumberToken clipboard_sequence_number_;
  ClipboardSequenceNumberToken selection_sequence_number_;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;
};

// ClipboardOzone implementation.
ClipboardOzone::ClipboardOzone() {
  auto* platform_clipboard =
      OzonePlatform::GetInstance()->GetPlatformClipboard();

  if (platform_clipboard) {
    async_clipboard_ozone_ =
        std::make_unique<ClipboardOzone::AsyncClipboardOzone>(
            platform_clipboard);
  } else {
    static base::NoDestructor<StubPlatformClipboard> stub_platform_clipboard;
    async_clipboard_ozone_ =
        std::make_unique<ClipboardOzone::AsyncClipboardOzone>(
            stub_platform_clipboard.get());
  }
}

ClipboardOzone::~ClipboardOzone() = default;

void ClipboardOzone::OnPreShutdown() {
  async_clipboard_ozone_->OnPreShutdown();
}

std::optional<DataTransferEndpoint> ClipboardOzone::GetSource(
    ClipboardBuffer buffer) const {
  return async_clipboard_ozone_->ReadSourceAndWait(buffer);
}

const ClipboardSequenceNumberToken& ClipboardOzone::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  return async_clipboard_ozone_->GetSequenceNumber(buffer);
}

bool ClipboardOzone::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  if (!IsReadAllowed(GetSource(buffer), data_dst, base::span<uint8_t>()))
    return false;

  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  return std::ranges::contains(available_types, format.GetName());
}

void ClipboardOzone::Clear(ClipboardBuffer buffer) {
  async_clipboard_ozone_->Clear(buffer);
}

std::vector<std::u16string> ClipboardOzone::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  return GetStandardFormatsFromMimeTypes(available_types);
}

void ClipboardOzone::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadAvailableTypesCallback callback) const {
  DCHECK(CalledOnValidThread());

  async_clipboard_ozone_->GetAvailableMimeTypesAsync(
      buffer,
      base::BindOnce(&ClipboardOzone::OnReadAvailableTypes,
                     weak_factory_.GetWeakPtr(), buffer, std::move(callback)));
}

void ClipboardOzone::OnReadAvailableTypes(
    ClipboardBuffer buffer,
    ReadAvailableTypesCallback callback,
    const std::vector<std::string>& available_types) const {
  std::vector<std::u16string> types =
      GetStandardFormatsFromMimeTypes(available_types);

  if (std::ranges::contains(
          available_types,
          ClipboardFormatType::DataTransferCustomType().GetName())) {
    async_clipboard_ozone_->ReadClipboardDataAsync(
        buffer, ClipboardFormatType::DataTransferCustomType().GetName(),
        base::BindOnce(&ClipboardOzone::OnReadCustomData,
                       weak_factory_.GetWeakPtr(), std::move(types),
                       std::move(callback)));
  } else {
    std::move(callback).Run(std::move(types));
  }
}

void ClipboardOzone::OnReadCustomData(
    std::vector<std::u16string> types,
    ReadAvailableTypesCallback callback,
    const PlatformClipboard::Data& data) const {
  if (data) {
    ReadCustomDataTypes(data->as_vector(), &types);
  }
  std::move(callback).Run(std::move(types));
}

template <typename Callback, typename ProcessCallback>
void ClipboardOzone::ReadAsync(
    ClipboardBuffer buffer,
    const std::string& mime_type,
    const std::optional<DataTransferEndpoint>& data_dst,
    ClipboardFormatMetric metric,
    Callback callback,
    ProcessCallback process_cb) const {
  async_clipboard_ozone_->ReadClipboardDataAsync(
      buffer, mime_type,
      base::BindOnce(&ClipboardOzone::OnReadAsync<Callback, ProcessCallback>,
                     weak_factory_.GetWeakPtr(), buffer, data_dst, metric,
                     std::move(callback), std::move(process_cb)));
}

template <typename Callback, typename ProcessCallback>
void ClipboardOzone::OnReadAsync(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ClipboardFormatMetric metric,
    Callback callback,
    ProcessCallback process_cb,
    const PlatformClipboard::Data& data) const {
  if (!data) {
    RecordRead(metric);
    std::move(process_cb)(std::move(callback), nullptr);
    return;
  }

  async_clipboard_ozone_->ReadSourceAsync(
      buffer, base::BindOnce(
                  &ClipboardOzone::OnReadAsyncSource<Callback, ProcessCallback>,
                  weak_factory_.GetWeakPtr(), data_dst, metric,
                  std::move(callback), std::move(process_cb), data));
}

template <typename Callback, typename ProcessCallback>
void ClipboardOzone::OnReadAsyncSource(
    const std::optional<DataTransferEndpoint>& data_dst,
    ClipboardFormatMetric metric,
    Callback callback,
    ProcessCallback process_cb,
    const PlatformClipboard::Data& data,
    std::optional<DataTransferEndpoint> data_src) const {
  if (!IsReadAllowed(data_src, base::OptionalToPtr(data_dst),
                     data->as_vector())) {
    std::move(process_cb)(std::move(callback), nullptr);
    return;
  }
  RecordRead(metric);
  std::move(process_cb)(std::move(callback), data);
}

void ClipboardOzone::ReadText(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadTextCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(buffer, kMimeTypePlainText, data_dst, ClipboardFormatMetric::kText,
            std::move(callback),
            [](ReadTextCallback callback, const PlatformClipboard::Data& data) {
              std::move(callback).Run(
                  data ? base::UTF8ToUTF16(std::string_view(
                             reinterpret_cast<const char*>(data->data()),
                             data->size()))
                       : u"");
            });
}

void ClipboardOzone::ReadAsciiText(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadAsciiTextCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(
      buffer, kMimeTypePlainText, data_dst, ClipboardFormatMetric::kText,
      std::move(callback),
      [](ReadAsciiTextCallback callback, const PlatformClipboard::Data& data) {
        std::move(callback).Run(data ? std::string(data->begin(), data->end())
                                     : "");
      });
}

void ClipboardOzone::ReadHTML(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadHtmlCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(buffer, kMimeTypeHtml, data_dst, ClipboardFormatMetric::kHtml,
            std::move(callback),
            [](ReadHtmlCallback callback, const PlatformClipboard::Data& data) {
              if (!data) {
                std::move(callback).Run(u"", GURL(), 0, 0);
                return;
              }
              std::u16string markup = base::UTF8ToUTF16(std::string_view(
                  reinterpret_cast<const char*>(data->data()), data->size()));
              uint32_t fragment_end = static_cast<uint32_t>(markup.length());
              std::move(callback).Run(std::move(markup), GURL(), 0,
                                      fragment_end);
            });
}

void ClipboardOzone::ReadSvg(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadSvgCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(buffer, kMimeTypeSvg, data_dst, ClipboardFormatMetric::kSvg,
            std::move(callback),
            [](ReadSvgCallback callback, const PlatformClipboard::Data& data) {
              std::move(callback).Run(
                  data ? base::UTF8ToUTF16(std::string_view(
                             reinterpret_cast<const char*>(data->data()),
                             data->size()))
                       : u"");
            });
}

void ClipboardOzone::ReadRTF(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadRTFCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(buffer, kMimeTypeRtf, data_dst, ClipboardFormatMetric::kRtf,
            std::move(callback),
            [](ReadRTFCallback callback, const PlatformClipboard::Data& data) {
              std::move(callback).Run(
                  data ? std::string(data->begin(), data->end()) : "");
            });
}

void ClipboardOzone::ReadPng(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(buffer, kMimeTypePng, data_dst, ClipboardFormatMetric::kPng,
            std::move(callback),
            [](ReadPngCallback callback, const PlatformClipboard::Data& data) {
              std::move(callback).Run(data ? data->as_vector()
                                           : std::vector<uint8_t>());
            });
}

void ClipboardOzone::ReadDataTransferCustomData(
    ClipboardBuffer buffer,
    const std::u16string& type,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadDataTransferCustomDataCallback callback) const {
  DCHECK(CalledOnValidThread());

  ReadAsync(buffer, kMimeTypeDataTransferCustomData, data_dst,
            ClipboardFormatMetric::kCustomData, std::move(callback),
            [type](ReadDataTransferCustomDataCallback callback,
                   const PlatformClipboard::Data& data) {
              if (data) {
                if (std::optional<std::u16string> maybe_data =
                        ReadCustomDataForType(data->as_vector(), type);
                    maybe_data) {
                  std::move(callback).Run(std::move(*maybe_data));
                  return;
                }
              }
              std::move(callback).Run(u"");
            });
}

void ClipboardOzone::ReadFilenames(
    ClipboardBuffer buffer,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadFilenamesCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(
      buffer, kMimeTypeUriList, data_dst, ClipboardFormatMetric::kFilenames,
      std::move(callback),
      [](ReadFilenamesCallback callback, const PlatformClipboard::Data& data) {
        std::move(callback).Run(data ? ui::URIListToFileInfos(std::string(
                                           data->begin(), data->end()))
                                     : std::vector<ui::FileInfo>());
      });
}

void ClipboardOzone::ReadBookmark(
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadBookmarkCallback callback) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
  std::move(callback).Run(u"", GURL());
}

void ClipboardOzone::ReadData(
    const ClipboardFormatType& format,
    const std::optional<DataTransferEndpoint>& data_dst,
    ReadDataCallback callback) const {
  DCHECK(CalledOnValidThread());
  ReadAsync(ClipboardBuffer::kCopyPaste, format.GetName(), data_dst,
            ClipboardFormatMetric::kData, std::move(callback),
            [](ReadDataCallback callback, const PlatformClipboard::Data& data) {
              std::move(callback).Run(
                  data ? std::string(data->begin(), data->end()) : "");
            });
}

void ClipboardOzone::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  if (!IsReadAllowed(GetSource(buffer), data_dst, base::span<uint8_t>()))
    return;

  types->clear();

  for (const auto& mime_type : GetStandardFormats(buffer, data_dst)) {
    types->push_back(mime_type);
  }
  // Special handling for chromium/x-web-custom-data.
  // We must read the data and deserialize it to find the list
  // of mime types to report.
  if (IsFormatAvailable(ClipboardFormatType::DataTransferCustomType(), buffer,
                        data_dst)) {
    auto data = async_clipboard_ozone_->ReadClipboardDataAndWait(
        buffer, ClipboardFormatType::DataTransferCustomType().GetName());
    ReadCustomDataTypes(data, types);
  }
}

void ClipboardOzone::ReadText(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      buffer, kMimeTypePlainText);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kText);
  *result = base::UTF8ToUTF16(std::string_view(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

void ClipboardOzone::ReadAsciiText(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      buffer, kMimeTypePlainText);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data)) {
    return;
  }

  RecordRead(ClipboardFormatMetric::kText);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::ReadBookmark(const DataTransferEndpoint* data_dst,
                                  std::u16string* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  // |data_dst| should be supported for DLP when ReadBookmark() is implemented.
  NOTIMPLEMENTED();
}

void ClipboardOzone::ReadData(const ClipboardFormatType& format,
                              const DataTransferEndpoint* data_dst,
                              std::string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      ClipboardBuffer::kCopyPaste, format.GetName());

  if (!IsReadAllowed(GetSource(ClipboardBuffer::kCopyPaste), data_dst,
                     clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kData);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

bool ClipboardOzone::IsSelectionBufferAvailable() const {
  return async_clipboard_ozone_->IsSelectionBufferAvailable();
}

void ClipboardOzone::WritePortableTextRepresentation(ClipboardBuffer buffer,
                                                     const ObjectMap& objects) {
  // Just like Non-Backed/X11 implementation does, copy text data from the
  // copy/paste selection to the primary selection.
  if (buffer != ClipboardBuffer::kCopyPaste || !IsSelectionBufferAvailable()) {
    return;
  }

  auto text_iter = objects.find(base::VariantIndexOfType<Data, TextData>());
  if (text_iter == objects.end()) {
    return;
  }

  const auto& text_data = std::get<TextData>(text_iter->second.data);
  if (text_data.data.empty()) {
    return;
  }

  async_clipboard_ozone_->PrepareForWriting();
  WriteText(text_data.data);
  async_clipboard_ozone_->OfferData(ClipboardBuffer::kSelection);
}

void ClipboardOzone::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    const std::vector<RawData>& raw_objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src,
    uint32_t privacy_types) {
  DCHECK(CalledOnValidThread());

  async_clipboard_ozone_->PrepareForWriting();
  DispatchPlatformRepresentations(std::move(platform_representations));

  AddSourceToClipboard(buffer, std::move(data_src));

  for (const auto& object : objects) {
    DispatchPortableRepresentation(object.second);
  }
  for (const auto& raw_object : raw_objects) {
    DispatchPortableRepresentation(raw_object);
  }
  async_clipboard_ozone_->OfferData(buffer);

  WritePortableTextRepresentation(buffer, objects);
}

void ClipboardOzone::WriteText(std::string_view text) {
  std::vector<uint8_t> data(text.begin(), text.end());
  async_clipboard_ozone_->InsertData(
      std::move(data),
      {kMimeTypePlainText, kMimeTypeLinuxText, kMimeTypeLinuxString,
       kMimeTypeUtf8PlainText, kMimeTypeLinuxUtf8String});
}

void ClipboardOzone::WriteHTML(
    std::string_view markup,
    std::optional<std::string_view> /* source_url */) {
  std::vector<uint8_t> data(markup.begin(), markup.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeHtml});
}

void ClipboardOzone::WriteSvg(std::string_view markup) {
  std::vector<uint8_t> data(markup.begin(), markup.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeSvg});
}

void ClipboardOzone::WriteRTF(std::string_view rtf) {
  std::vector<uint8_t> data(rtf.begin(), rtf.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeRtf});
}

void ClipboardOzone::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  std::string uri_list = ui::FileInfosToURIList(filenames);
  std::vector<uint8_t> data(uri_list.begin(), uri_list.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeUriList});
}

void ClipboardOzone::WriteBookmark(std::string_view title,
                                   std::string_view url) {
  // Writes a Mozilla url (UTF16: URL, newline, title)
  std::u16string bookmark =
      base::StrCat({base::UTF8ToUTF16(url) + u"\n" + base::UTF8ToUTF16(title)});

  std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(bookmark.data()),
                            reinterpret_cast<const uint8_t*>(UNSAFE_TODO(
                                bookmark.data() + bookmark.size())));
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeMozillaUrl});
}

void ClipboardOzone::WriteWebSmartPaste() {
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(),
                                     {kMimeTypeWebkitSmartPaste});
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  // Encode the bitmap to a PNG from the UI thread. Unfortunately we can't hop
  // to a background thread to perform the encoding because clipboard writes are
  // (unfortunately) currently synchronous. We could consider making writes
  // async, then encode the image on a background sequence. We could also
  // consider storing the image as a bitmap and only encoding to a PNG on paste
  // (e.g. see https://crrev.com/c/3260985).
  std::vector<uint8_t> png_bytes =
      clipboard_util::EncodeBitmapToPngAcceptJank(bitmap);
  if (!png_bytes.empty()) {
    async_clipboard_ozone_->InsertData(std::move(png_bytes), {kMimeTypePng});
  }
}

void ClipboardOzone::WriteData(const ClipboardFormatType& format,
                               base::span<const uint8_t> data) {
  std::vector<uint8_t> owned_data(data.begin(), data.end());
  async_clipboard_ozone_->InsertData(std::move(owned_data), {format.GetName()});
}

void ClipboardOzone::AddSourceToClipboard(
    const ClipboardBuffer buffer,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  if (data_src && data_src->IsUrlType()) {
    const std::string& string_url = data_src->GetURL()->spec();
    async_clipboard_ozone_->InsertData(
        std::vector<uint8_t>(string_url.begin(), string_url.end()),
        {kMimeTypeSourceUrl});
  }
}

}  // namespace ui
