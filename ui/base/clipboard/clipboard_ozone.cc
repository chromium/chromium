// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_clipboard.h"

#if defined(OS_CHROMEOS) && BUILDFLAG(OZONE_PLATFORM_X11)
#include "base/command_line.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/ui_base_switches.h"
#endif

namespace ui {

namespace {

// The amount of time to wait for a request to complete before aborting it.
constexpr base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(10);

// Depending on the backend, the platform clipboard may or may not be
// available.  Should it be absent, we provide a dummy one.  It always calls
// back immediately with empty data, and denies ownership of any buffer.
class StubPlatformClipboard : public PlatformClipboard {
 public:
  StubPlatformClipboard() = default;
  ~StubPlatformClipboard() override = default;

  // PlatformClipboard:
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override {
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
  bool IsSelectionOwner(ClipboardBuffer buffer) override { return false; }
  void SetSequenceNumberUpdateCb(
      PlatformClipboard::SequenceNumberUpdateCb cb) override {}
  bool IsSelectionBufferAvailable() const override { return false; }
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

    Request request(RequestType::kRead);
    request.requested_mime_type = mime_type;
    PerformRequestAndWaitForResult(buffer, &request);

    offered_data_[buffer] = request.data_map;
    auto it = offered_data_[buffer].find(mime_type);
    if (it == offered_data_[buffer].end())
      return {};
    return base::make_span(it->second->front(), it->second->size());
  }

  std::vector<std::string> RequestMimeTypes(ClipboardBuffer buffer) {
    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_[buffer])
        mime_types.push_back(item.first);
      return mime_types;
    }

    Request request(RequestType::kGetMime);
    PerformRequestAndWaitForResult(buffer, &request);
    return request.mime_types;
  }

  void OfferData(ClipboardBuffer buffer) {
    Request request(RequestType::kOffer);
    request.data_map = data_to_offer_;
    offered_data_[buffer] = std::move(data_to_offer_);
    PerformRequestAndWaitForResult(buffer, &request);

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
  enum class RequestType {
    kRead = 0,
    kOffer = 1,
    kGetMime = 2,
  };

  // Holds request data to process inquiries from the ClipboardOzone.
  struct Request {
    explicit Request(RequestType request_type) : type(request_type) {}
    ~Request() = default;

    // Describes the type of the request.
    RequestType type;

    // A closure that is used to signal the request is processed.
    base::OnceClosure finish_closure;

    // Used for kRead and kOffer requests. It contains either data offered by
    // Chromium to a system clipboard or a read data offered by the system
    // clipboard.
    PlatformClipboard::DataMap data_map;

    // Identifies which mime type the client is interested to read from the
    // system clipboard during kRead requests.
    std::string requested_mime_type;

    // A vector of mime types returned as a result to a kGetMime request to get
    // available mime types.
    std::vector<std::string> mime_types;
  };

  void PerformRequestAndWaitForResult(ClipboardBuffer buffer,
                                      Request* request) {
    DCHECK(request);
    DCHECK(!abort_timer_.IsRunning());
    DCHECK(!pending_request_);

    pending_request_ = request;
    switch (pending_request_->type) {
      case (RequestType::kRead):
        DispatchReadRequest(buffer, request);
        break;
      case (RequestType::kOffer):
        DispatchOfferRequest(buffer, request);
        break;
      case (RequestType::kGetMime):
        DispatchGetMimeRequest(buffer, request);
        break;
    }

    if (!pending_request_)
      return;

    // TODO(https://crbug.com/913422): the implementation is known to be
    // dangerous, and may cause blocks in ui thread. But base::Clipboard was
    // designed to have synchronous APIs rather than asynchronous ones that at
    // least two system clipboards on X11 and Wayland provide.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    request->finish_closure = run_loop.QuitClosure();

    // Set a timeout timer after which the request will be aborted.
    abort_timer_.Start(FROM_HERE, kRequestTimeout, this,
                       &AsyncClipboardOzone::AbortStalledRequest);
    run_loop.Run();
  }

  void AbortStalledRequest() {
    if (pending_request_ && pending_request_->finish_closure)
      std::move(pending_request_->finish_closure).Run();
  }

  void DispatchReadRequest(ClipboardBuffer buffer, Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnTextRead,
                                   weak_factory_.GetWeakPtr());
    platform_clipboard_->RequestClipboardData(
        buffer, request->requested_mime_type, &request->data_map,
        std::move(callback));
  }

  void DispatchOfferRequest(ClipboardBuffer buffer, Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnOfferDone,
                                   weak_factory_.GetWeakPtr());
    platform_clipboard_->OfferClipboardData(buffer, request->data_map,
                                            std::move(callback));
  }

  void DispatchGetMimeRequest(ClipboardBuffer buffer, Request* request) {
    auto callback = base::BindOnce(&AsyncClipboardOzone::OnGotMimeTypes,
                                   weak_factory_.GetWeakPtr());
    platform_clipboard_->GetAvailableMimeTypes(buffer, std::move(callback));
  }

  void OnTextRead(const base::Optional<PlatformClipboard::Data>& data) {
    // |data| is already set in request's data_map, so just finish request
    // processing.
    CompleteRequest();
  }

  void OnOfferDone() { CompleteRequest(); }

  void OnGotMimeTypes(const std::vector<std::string>& mime_types) {
    pending_request_->mime_types = std::move(mime_types);
    CompleteRequest();
  }

  void CompleteRequest() {
    if (!pending_request_)
      return;
    abort_timer_.Stop();
    if (pending_request_->finish_closure)
      std::move(pending_request_->finish_closure).Run();
    pending_request_ = nullptr;
  }

  void UpdateClipboardSequenceNumber(ClipboardBuffer buffer) {
    ++clipboard_sequence_number_[buffer];
  }

  // Clipboard data accumulated for writing.
  PlatformClipboard::DataMap data_to_offer_;

  // Clipboard data that had been offered most recently.  Used as a cache to
  // read data if we still own it.
  base::flat_map<ClipboardBuffer, PlatformClipboard::DataMap> offered_data_;

  // A current pending request being processed.
  Request* pending_request_ = nullptr;

  // Aborts |pending_request| after Request::timeout.
  base::RepeatingTimer abort_timer_;

  // Provides communication to a system clipboard under ozone level.
  PlatformClipboard* const platform_clipboard_ = nullptr;

  base::flat_map<ClipboardBuffer, uint64_t> clipboard_sequence_number_;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncClipboardOzone);
};

// Uses the factory in the clipboard_linux otherwise.
#if defined(OS_CHROMEOS) || !defined(OS_LINUX)
// Clipboard factory method.
Clipboard* Clipboard::Create() {
// linux-chromeos uses non-backed clipboard by default, but supports ozone x11
// with flag --use-system-clipbboard.
#if defined(OS_CHROMEOS) && BUILDFLAG(OZONE_PLATFORM_X11)
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

uint64_t ClipboardOzone::GetSequenceNumber(ClipboardBuffer buffer) const {
  return async_clipboard_ozone_->GetSequenceNumber(buffer);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
bool ClipboardOzone::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const ClipboardDataEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  return base::Contains(available_types, format.GetName());
}

void ClipboardOzone::Clear(ClipboardBuffer buffer) {
  async_clipboard_ozone_->Clear(buffer);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const ClipboardDataEndpoint* data_dst,
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
    const ClipboardDataEndpoint* data_dst) const {
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
                              const ClipboardDataEndpoint* data_dst,
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
                                   const ClipboardDataEndpoint* data_dst,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeText);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadHTML(ClipboardBuffer buffer,
                              const ClipboardDataEndpoint* data_dst,
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
                             const ClipboardDataEndpoint* data_dst,
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
                             const ClipboardDataEndpoint* data_dst,
                             std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kRtf);

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataAndWait(buffer, kMimeTypeRTF);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadImage(ClipboardBuffer buffer,
                               const ClipboardDataEndpoint* data_dst,
                               ReadImageCallback callback) const {
  RecordRead(ClipboardFormatMetric::kImage);
  std::move(callback).Run(ReadImageInternal(buffer));
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadCustomData(ClipboardBuffer buffer,
                                    const base::string16& type,
                                    const ClipboardDataEndpoint* data_dst,
                                    base::string16* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kCustomData);

  auto custom_data = async_clipboard_ozone_->ReadClipboardDataAndWait(
      buffer, kMimeTypeWebCustomData);
  ReadCustomDataForType(custom_data.data(), custom_data.size(), type, result);
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadBookmark(const ClipboardDataEndpoint* data_dst,
                                  base::string16* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  NOTIMPLEMENTED();
}

// TODO(crbug.com/1103194): |data_dst| should be supported.
void ClipboardOzone::ReadData(const ClipboardFormatType& format,
                              const ClipboardDataEndpoint* data_dst,
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
    std::unique_ptr<ClipboardDataEndpoint> data_src) {
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
}

// TODO(crbug.com/1103194): |data_src| should be supported
void ClipboardOzone::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<ClipboardDataEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DispatchPlatformRepresentations(std::move(platform_representations));

  async_clipboard_ozone_->OfferData(buffer);
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
