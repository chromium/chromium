// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_clipboard.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(OZONE_PLATFORM_X11)
#include "base/command_line.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/ui_base_switches.h"
#endif

namespace ui {

namespace {

// The amount of time to wait for a request to complete before aborting it.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(1);

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
      PlatformClipboard::DataMap* data_map,
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
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override {}
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
        base::BindRepeating(&AsyncClipboardOzone::UpdateClipboardSequenceNumber,
                            weak_factory_.GetWeakPtr());
    platform_clipboard_->SetSequenceNumberUpdateCb(
        std::move(update_sequence_cb));
  }

  ~AsyncClipboardOzone() = default;

  bool IsSelectionBufferAvailable() const {
    return platform_clipboard_->IsSelectionBufferAvailable();
  }

  base::span<uint8_t> ReadClipboardDataAndWait(ClipboardBuffer buffer,
                                               const std::string& mime_type) {
    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      auto it = offered_data_[buffer].find(mime_type);
      if (it == offered_data_[buffer].end())
        return {};
      return base::make_span(it->second->front(), it->second->size());
    }

    if (auto data = Read(buffer, mime_type))
      return base::make_span(data->front(), data->size());
    return {};
  }

  std::vector<std::string> RequestMimeTypes(ClipboardBuffer buffer) {
    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_[buffer])
        mime_types.push_back(item.first);
      return mime_types;
    }

    return GetMimeTypes(buffer);
  }

  void OfferData(ClipboardBuffer buffer) {
    Offer(buffer, std::move(data_to_offer_));
    UpdateClipboardSequenceNumber(buffer);
  }

  void Clear(ClipboardBuffer buffer) {
    data_to_offer_.clear();
    OfferData(buffer);
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  void InsertData(std::vector<uint8_t> data,
                  const std::set<std::string>& mime_types) {
    auto wrapped_data = scoped_refptr<base::RefCountedBytes>(
        base::RefCountedBytes::TakeVector(&data));
    for (const auto& mime_type : mime_types) {
      DCHECK_EQ(data_to_offer_.count(mime_type), 0U);
      data_to_offer_[mime_type] = wrapped_data;
    }
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  uint64_t GetSequenceNumber(ClipboardBuffer buffer) {
    return clipboard_sequence_number_[buffer];
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

      // TODO(crbug.com/913422): this is known to be dangerous, and may cause
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

    void FinishWithOptional(const base::Optional<Result>& result) {
      Finish(result.value_or(Result{}));
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
    PlatformClipboard::DataMap data_map;
    platform_clipboard_->RequestClipboardData(
        buffer, mime_type, &data_map,
        base::BindOnce(&ReadRequest::FinishWithOptional, request.GetWeakPtr()));
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

  void UpdateClipboardSequenceNumber(ClipboardBuffer buffer) {
    ++clipboard_sequence_number_[buffer];
  }

  // Clipboard data accumulated for writing.
  PlatformClipboard::DataMap data_to_offer_;

  // Clipboard data that had been offered most recently.  Used as a cache to
  // read data if we still own it.
  base::flat_map<ClipboardBuffer, PlatformClipboard::DataMap> offered_data_;

  // Provides communication to a system clipboard under ozone level.
  PlatformClipboard* const platform_clipboard_ = nullptr;

  base::flat_map<ClipboardBuffer, uint64_t> clipboard_sequence_number_;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncClipboardOzone);
};

// Uses the factory in the clipboard_linux otherwise.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if !(defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
// Clipboard factory method.
Clipboard* Clipboard::Create() {
// linux-chromeos uses non-backed clipboard by default, but supports ozone x11
// with flag --use-system-clipbboard.
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(OZONE_PLATFORM_X11)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseSystemClipboard)) {
    return new ClipboardNonBacked;
  }
#endif
  return new ClipboardOzone;
}
#endif

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

void ClipboardOzone::OnPreShutdown() {}

DataTransferEndpoint* ClipboardOzone::GetSource(ClipboardBuffer buffer) const {
  auto it = data_src_.find(buffer);
  return it == data_src_.end() ? nullptr : it->second.get();
}

uint64_t ClipboardOzone::GetSequenceNumber(ClipboardBuffer buffer) const {
  return async_clipboard_ozone_->GetSequenceNumber(buffer);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
bool ClipboardOzone::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  return base::Contains(available_types, format.GetName());
}

void ClipboardOzone::Clear(ClipboardBuffer buffer) {
  async_clipboard_ozone_->Clear(buffer);
  data_src_[buffer].reset();
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  types->clear();

  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  for (auto& mime_type : available_types) {
    // Special handling for chromium/x-web-custom-data.
    // We must read the data and deserialize it to find the list
    // of mime types to report.
    if (mime_type == ClipboardFormatType::GetWebCustomDataType().GetName()) {
      auto data = async_clipboard_ozone_->ReadClipboardDataAndWait(
          buffer, ClipboardFormatType::GetWebCustomDataType().GetName());
      ReadCustomDataTypes(data.data(), data.size(), types);
    } else {
      types->push_back(base::UTF8ToUTF16(mime_type));
    }
  }
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
std::vector<base::string16>
ClipboardOzone::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  std::vector<std::string> mime_types =
      async_clipboard_ozone_->RequestMimeTypes(buffer);
  std::vector<base::string16> types;
  types.reserve(mime_types.size());
  for (auto& mime_type : mime_types)
    types.push_back(base::UTF8ToUTF16(mime_type));
  return types;
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadText(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeText);
  *result = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadAsciiText(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeText);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadHTML(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              base::string16* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kHtml);

  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeHTML);
  *markup = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
  DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadSvg(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kSvg);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeSvg);
  *result = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadRTF(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kRtf);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeRTF);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadImage(ClipboardBuffer buffer,
                               const DataTransferEndpoint* data_dst,
                               ReadImageCallback callback) const {
  RecordRead(ClipboardFormatMetric::kImage);
  std::move(callback).Run(ReadImageInternal(buffer));
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadCustomData(ClipboardBuffer buffer,
                                    const base::string16& type,
                                    const DataTransferEndpoint* data_dst,
                                    base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kCustomData);

  auto custom_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      buffer, kMimeTypeWebCustomData);
  ReadCustomDataForType(custom_data.data(), custom_data.size(), type, result);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadFilenames(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kFilenames);

  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      buffer, kMimeTypeURIList);
  std::string uri_list(clipboard_data.begin(), clipboard_data.end());
  *result = ui::URIListToFileInfos(uri_list);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadBookmark(const DataTransferEndpoint* data_dst,
                                  base::string16* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  NOTIMPLEMENTED();
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadData(const ClipboardFormatType& format,
                              const DataTransferEndpoint* data_dst,
                              std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);

  auto clipboard_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      ClipboardBuffer::kCopyPaste, format.GetName());
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

bool ClipboardOzone::IsSelectionBufferAvailable() const {
  return async_clipboard_ozone_->IsSelectionBufferAvailable();
}

// TODO(crbug.com/1103194): |data_src| should be supported
void ClipboardOzone::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());

  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);

  async_clipboard_ozone_->OfferData(buffer);

  // Just like Non-Backed/X11 implementation does, copy text data from the
  // copy/paste selection to the primary selection.
  if (buffer == ClipboardBuffer::kCopyPaste) {
    auto text_iter = objects.find(PortableFormat::kText);
    if (text_iter != objects.end()) {
      const ObjectMapParams& params_vector = text_iter->second;
      if (!params_vector.empty()) {
        const ObjectMapParam& char_vector = params_vector[0];
        if (!char_vector.empty())
          WriteText(&char_vector.front(), char_vector.size());
      }
      async_clipboard_ozone_->OfferData(ClipboardBuffer::kSelection);
    }
  }

  data_src_[buffer] = std::move(data_src);
}

// TODO(crbug.com/1103194): |data_src| should be supported
void ClipboardOzone::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DispatchPlatformRepresentations(std::move(platform_representations));

  async_clipboard_ozone_->OfferData(buffer);

  data_src_[buffer] = std::move(data_src);
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::vector<uint8_t> data(text_data, text_data + text_len);
  async_clipboard_ozone_->InsertData(
      std::move(data), {kMimeTypeText, kMimeTypeLinuxText, kMimeTypeLinuxString,
                        kMimeTypeTextUtf8, kMimeTypeLinuxUtf8String});
}

void ClipboardOzone::WriteHTML(const char* markup_data,
                               size_t markup_len,
                               const char* url_data,
                               size_t url_len) {
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeHTML});
}

void ClipboardOzone::WriteSvg(const char* markup_data, size_t markup_len) {
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeSvg});
}

void ClipboardOzone::WriteRTF(const char* rtf_data, size_t data_len) {
  std::vector<uint8_t> data(rtf_data, rtf_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeRTF});
}

void ClipboardOzone::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  std::string uri_list = ui::FileInfosToURIList(filenames);
  std::vector<uint8_t> data(uri_list.begin(), uri_list.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeURIList});
}

void ClipboardOzone::WriteBookmark(const char* title_data,
                                   size_t title_len,
                                   const char* url_data,
                                   size_t url_len) {
  // Writes a Mozilla url (UTF16: URL, newline, title)
  base::string16 bookmark =
      base::UTF8ToUTF16(base::StringPiece(url_data, url_len)) +
      base::ASCIIToUTF16("\n") +
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<uint8_t> data(
      reinterpret_cast<const uint8_t*>(bookmark.data()),
      reinterpret_cast<const uint8_t*>(bookmark.data() + bookmark.size()));
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeMozillaURL});
}

void ClipboardOzone::WriteWebSmartPaste() {
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(),
                                     {kMimeTypeWebkitSmartPaste});
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output))
    async_clipboard_ozone_->InsertData(std::move(output), {kMimeTypePNG});
}

void ClipboardOzone::WriteData(const ClipboardFormatType& format,
                               const char* data_data,
                               size_t data_len) {
  std::vector<uint8_t> data(data_data, data_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), {format.GetName()});
}

SkBitmap ClipboardOzone::ReadImageInternal(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypePNG);
  SkBitmap bitmap;
  if (!gfx::PNGCodec::Decode(clipboard_data.data(), clipboard_data.size(),
                             &bitmap))
    return {};
  return SkBitmap(bitmap);
}

}  // namespace ui
