// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {

namespace {

using InstanceRegistry = std::set<const ClipboardNonBacked*, std::less<>>;
// Returns the registry which tracks all instances of ClipboardNonBacked in
// existence. This allows us to determine if any arbitrary Clipboard pointer in
// fact points to a ClipboardNonBacked instance. Only if a pointer exists in
// this registry is it safe to cast to ClipboardNonBacked*.
InstanceRegistry* GetInstanceRegistry() {
  static base::NoDestructor<InstanceRegistry> registry;
  return registry.get();
}

// The ClipboardNonBacked instance registry can be accessed by multiple threads.
// Any inspection/modification of the instance registry must only be performed
// while this lock is held.
base::Lock& GetInstanceRegistryLock() {
  static base::NoDestructor<base::Lock> registry_lock;
  return *registry_lock;
}

// Registers the specified |clipboard| as an instance of ClipboardNonBacked.
// Registration should occur during |clipboard| construction and registration
// should be maintained until |clipboard| is destroyed. This allows us to check
// if any arbitrary Clipboard* is safe to cast.
void RegisterInstance(const ClipboardNonBacked* clipboard) {
  base::AutoLock lock(GetInstanceRegistryLock());
  GetInstanceRegistry()->insert(clipboard);
}

// Unregisters the specified |clipboard| as a instance of ClipboardNonBacked.
// This should only be done when destroying |clipboard|.
void UnregisterInstance(const ClipboardNonBacked* clipboard) {
  base::AutoLock lock(GetInstanceRegistryLock());
  GetInstanceRegistry()->erase(clipboard);
}

// Checks if |clipboard| is registered as an instance of ClipboardNonBacked.
// Only if this method returns true is it safe to cast |clipboard| to
// ClipboardNonBacked*.
bool IsRegisteredInstance(const Clipboard* clipboard) {
  base::AutoLock lock(GetInstanceRegistryLock());
  return base::Contains(*GetInstanceRegistry(), clipboard);
}

}  // namespace

// Simple, internal implementation of a clipboard, handling things like format
// conversion, sequence numbers, etc.
class ClipboardInternal {
 public:
  ClipboardInternal() = default;
  ClipboardInternal(const ClipboardInternal&) = delete;
  ClipboardInternal& operator=(const ClipboardInternal&) = delete;
  ~ClipboardInternal() = default;

  void Clear() {
    sequence_number_ = ClipboardSequenceNumberToken();
    data_.reset();
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  const ClipboardSequenceNumberToken& sequence_number() const {
    return sequence_number_;
  }

  // Returns the current clipboard data, which may be nullptr if nothing has
  // been written since the last Clear().
  const ClipboardData* GetData() const { return data_.get(); }

  // Returns true if the data on top of the clipboard stack has format |format|
  // or another format that can be converted to |format|.
  bool IsFormatAvailable(ClipboardInternalFormat format) const {
    if (format == ClipboardInternalFormat::kText) {
      return HasFormat(ClipboardInternalFormat::kText) ||
             HasFormat(ClipboardInternalFormat::kBookmark);
    }
    return HasFormat(format);
  }

  // Reads text from the ClipboardData.
  void ReadText(std::u16string* result) const {
    std::string utf8_result;
    ReadAsciiText(&utf8_result);
    *result = base::UTF8ToUTF16(utf8_result);
  }

  // Reads ASCII text from the ClipboardData.
  void ReadAsciiText(std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!data)
      return;
    if (HasFormat(ClipboardInternalFormat::kText))
      *result = data->text();
    else if (HasFormat(ClipboardInternalFormat::kHtml))
      *result = data->markup_data();
    else if (HasFormat(ClipboardInternalFormat::kBookmark))
      *result = data->bookmark_url();
  }

  // Reads HTML from the ClipboardData.
  void ReadHTML(std::u16string* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const {
    markup->clear();
    if (src_url)
      src_url->clear();
    *fragment_start = 0;
    *fragment_end = 0;

    if (!HasFormat(ClipboardInternalFormat::kHtml))
      return;

    const ClipboardData* data = GetData();
    *markup = base::UTF8ToUTF16(data->markup_data());
    *src_url = data->url();

    *fragment_start = 0;
    DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
    *fragment_end = static_cast<uint32_t>(markup->length());
  }

  // Reads SVG from the ClipboardData.
  void ReadSvg(std::u16string* markup) const {
    markup->clear();

    if (!HasFormat(ClipboardInternalFormat::kSvg))
      return;

    const ClipboardData* data = GetData();
    *markup = base::UTF8ToUTF16(data->svg_data());

    DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
  }

  // Reads RTF from the ClipboardData.
  void ReadRTF(std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(ClipboardInternalFormat::kRtf))
      return;

    *result = data->rtf_data();
  }

  // Reads png from the ClipboardData.
  void ReadPng(Clipboard::ReadPngCallback callback) const {
    if (!HasFormat(ClipboardInternalFormat::kPng)) {
      std::move(callback).Run(std::vector<uint8_t>());
      return;
    }

    const ClipboardData* data = GetData();

    // Check whether the clipboard contains an encoded PNG.
    auto maybe_png = data->maybe_png();
    if (maybe_png.has_value()) {
      std::move(callback).Run(std::move(maybe_png.value()));
      return;
    }

    // Check whether the clipboard contains an image which has not yet been
    // encoded to a PNG. If so, encode it on a background thread and return the
    // result asynchronously.
    auto maybe_bitmap = data->GetBitmapIfPngNotEncoded();
    DCHECK(maybe_bitmap.has_value())
        << "We should not be showing that PNG format is on the "
           "clipboard if neither a PNG or bitmap is available.";

    // Creates a new entry if one doesn't exist.
    auto& callbacks_for_image =
        callbacks_awaiting_image_encoding_[sequence_number()];
    callbacks_for_image.push_back(std::move(callback));

    // Encoding of this bitmap to a PNG is already in progress. No need to
    // kick off another encoding operation here. We'll respond to the callback
    // once the in-progress image encoding completes.
    if (callbacks_for_image.size() > 1)
      return;

    png_encoding_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&clipboard_util::EncodeBitmapToPng,
                       std::move(maybe_bitmap.value())),
        base::BindOnce(&ClipboardInternal::DidEncodePng,
                       weak_factory_.GetMutableWeakPtr(), sequence_number()));
  }

  // Reads data of type |type| from the ClipboardData.
  void ReadDataTransferCustomData(const std::u16string& type,
                                  std::u16string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(ClipboardInternalFormat::kCustom))
      return;

    std::optional<std::u16string> maybe_result = ReadCustomDataForType(
        base::as_bytes(base::span(data->GetDataTransferCustomData())), type);
    if (maybe_result) {
      *result = std::move(*maybe_result);
    }
  }

  // Reads filenames from the ClipboardData.
  const std::vector<ui::FileInfo>& ReadFilenames() const {
    return GetData()->filenames();
  }

  // Reads bookmark from the ClipboardData.
  void ReadBookmark(std::u16string* title, std::string* url) const {
    if (title)
      title->clear();
    if (url)
      url->clear();
    if (!HasFormat(ClipboardInternalFormat::kBookmark))
      return;

    const ClipboardData* data = GetData();
    if (title)
      *title = base::UTF8ToUTF16(data->bookmark_title());
    if (url)
      *url = data->bookmark_url();
  }

  void ReadData(const ClipboardFormatType& type, std::string* result) const {
    result->clear();
    if (data_) {
      *result = data_->GetCustomData(type);
    }
  }

  // Writes |data| to the ClipboardData and returns the previous data.
  std::unique_ptr<ClipboardData> WriteData(
      std::unique_ptr<ClipboardData> data) {
    DCHECK(data);
    std::unique_ptr<ClipboardData> previous_data = std::move(data_);
    data_ = std::move(data);
    sequence_number_ = data_->sequence_number_token();
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
    return previous_data;
  }

  bool IsReadAllowed(const DataTransferEndpoint* data_dst,
                     std::optional<ClipboardInternalFormat> format,
                     const std::optional<ClipboardFormatType>&
                         custom_data_format = std::nullopt) const {
    DataTransferPolicyController* policy_controller =
        DataTransferPolicyController::Get();
    auto* data = GetData();
    if (!policy_controller || !data)
      return true;
    return policy_controller->IsClipboardReadAllowed(
        data->source(), data_dst,
        data->CalculateSize(format, custom_data_format));
  }

  int NumImagesEncodedForTesting() const {
    return num_images_encoded_for_testing_;
  }

 private:
  // True if the ClipboardData has format |format|.
  bool HasFormat(ClipboardInternalFormat format) const {
    const ClipboardData* data = GetData();
    return data ? data->format() & static_cast<int>(format) : false;
  }

  void DidEncodePng(ClipboardSequenceNumberToken token,
                    std::vector<uint8_t> png_data) {
    num_images_encoded_for_testing_++;

    if (token == sequence_number()) {
      // Cache the encoded PNG.
      data_->SetPngDataAfterEncoding(png_data);
    }

    auto callbacks = std::move(callbacks_awaiting_image_encoding_.at(token));
    callbacks_awaiting_image_encoding_.erase(token);

    DCHECK(!callbacks.empty());

    for (auto& callback : callbacks) {
      std::move(callback).Run(png_data);
    }
  }

  // Current ClipboardData.
  std::unique_ptr<ClipboardData> data_;

  // Unique identifier for the current clipboard state.
  ClipboardSequenceNumberToken sequence_number_;

  // Repeated image read requests shouldn't invoke multiple encoding operations.
  // These callbacks will all be answered once the corresponding image finishes
  // encoding.
  mutable std::map<ClipboardSequenceNumberToken,
                   std::vector<Clipboard::ReadPngCallback>>
      callbacks_awaiting_image_encoding_;
  // Keeps track of how many encoding operations actually complete.
  int num_images_encoded_for_testing_ = 0;

  // Runner used to asynchronously encode bitmaps into PNGs if the clipboard
  // only contains a bitmap.
  scoped_refptr<base::SequencedTaskRunner> png_encoding_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE});

  base::WeakPtrFactory<ClipboardInternal> weak_factory_{this};
};

// Helper class to build a ClipboardData object and write it to clipboard.
class ClipboardDataBuilder {
 public:
  // If |data_src| is nullptr, this means that the data source isn't
  // confidential and the data can be pasted in any document.
  static void CommitToClipboard(ClipboardInternal& clipboard,
                                std::optional<DataTransferEndpoint> data_src) {
    ClipboardData* data = GetCurrentData();
#if BUILDFLAG(IS_CHROMEOS)
    data->set_commit_time(base::Time::Now());
#endif  // BUILDFLAG(IS_CHROMEOS)
    data->set_source(std::move(data_src));
    clipboard.WriteData(TakeCurrentData());
  }

  static void WriteText(std::string_view text) {
    ClipboardData* data = GetCurrentData();
    data->set_text(text);
  }

  static void WriteHTML(std::string_view markup,
                        std::optional<std::string_view> source_url) {
    ClipboardData* data = GetCurrentData();
    data->set_markup_data(markup);
    data->set_url(source_url ? *source_url : std::string());
  }

  static void WriteSvg(std::string_view markup) {
    ClipboardData* data = GetCurrentData();
    data->set_svg_data(markup);
  }

  static void WriteRTF(std::string_view rtf) {
    ClipboardData* data = GetCurrentData();
    data->SetRTFData(rtf);
  }

  static void WriteFilenames(std::vector<ui::FileInfo> filenames) {
    ClipboardData* data = GetCurrentData();
    data->set_filenames(std::move(filenames));
  }

  static void WriteBookmark(std::string_view title, std::string_view url) {
    ClipboardData* data = GetCurrentData();
    data->set_bookmark_title(title);
    data->set_bookmark_url(url);
  }

  static void WriteWebSmartPaste() {
    ClipboardData* data = GetCurrentData();
    data->set_web_smart_paste(true);
  }

  static void WriteBitmap(const SkBitmap& bitmap) {
    ClipboardData* data = GetCurrentData();
    data->SetBitmapData(bitmap);
  }

  static void WriteData(const ClipboardFormatType& format,
                        base::span<const uint8_t> data) {
    ClipboardData* clipboard_data = GetCurrentData();
    clipboard_data->SetCustomData(
        format,
        std::string(reinterpret_cast<const char*>(data.data()), data.size()));
  }

 private:
  static ClipboardData* GetCurrentData() {
    if (!current_data_)
      current_data_ = new ClipboardData;
    return current_data_;
  }

  static std::unique_ptr<ClipboardData> TakeCurrentData() {
    std::unique_ptr<ClipboardData> data = base::WrapUnique(GetCurrentData());
    current_data_ = nullptr;
    return data;
  }
  // This is a raw pointer instead of a std::unique_ptr to avoid adding a
  // static initializer.
  static ClipboardData* current_data_;
};

ClipboardData* ClipboardDataBuilder::current_data_ = nullptr;

// static
ClipboardNonBacked* ClipboardNonBacked::GetForCurrentThread() {
  auto* clipboard = Clipboard::GetForCurrentThread();

  // Ensure type safety. In tests the instance may not be registered.
  if (!IsRegisteredInstance(clipboard))
    return nullptr;

  return static_cast<ClipboardNonBacked*>(clipboard);
}

// ClipboardNonBacked implementation.
ClipboardNonBacked::ClipboardNonBacked() {
  DCHECK(CalledOnValidThread());
  RegisterInstance(this);

  // Unfortunately we cannot call Clipboard::IsSupportedClipboardBuffer()
  // from here because some components (like Ozone) are not yet initialized,
  // so create internal clipboards for platform supported clipboard buffers.
  constexpr ClipboardBuffer kClipboardBuffers[] = {
    ClipboardBuffer::kCopyPaste,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA)
    ClipboardBuffer::kSelection,
#endif
#if BUILDFLAG(IS_MAC)
    ClipboardBuffer::kDrag,
#endif
  };
  for (ClipboardBuffer buffer : kClipboardBuffers) {
    internal_clipboards_[buffer] = std::make_unique<ClipboardInternal>();
  }
}

ClipboardNonBacked::~ClipboardNonBacked() {
  DCHECK(CalledOnValidThread());
  UnregisterInstance(this);
}

const ClipboardData* ClipboardNonBacked::GetClipboardData(
    DataTransferEndpoint* data_dst,
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);
  if (!clipboard_internal.IsReadAllowed(data_dst, std::nullopt)) {
    return nullptr;
  }

  return clipboard_internal.GetData();
}

std::unique_ptr<ClipboardData> ClipboardNonBacked::WriteClipboardData(
    std::unique_ptr<ClipboardData> data,
    ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  return GetInternalClipboard(buffer).WriteData(std::move(data));
}

void ClipboardNonBacked::OnPreShutdown() {}

std::optional<DataTransferEndpoint> ClipboardNonBacked::GetSource(
    ClipboardBuffer buffer) const {
  const ClipboardData* data = GetInternalClipboard(buffer).GetData();
  return data ? data->source() : std::nullopt;
}

const ClipboardSequenceNumberToken& ClipboardNonBacked::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return GetInternalClipboard(buffer).sequence_number();
}

int ClipboardNonBacked::NumImagesEncodedForTesting(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);
  return clipboard_internal.NumImagesEncodedForTesting();  // IN-TEST
}

namespace {
bool IsReadAllowedAndAvailableFormat(
    const ClipboardInternal& clipboard_internal,
    const DataTransferEndpoint* data_dst,
    ClipboardInternalFormat format) {
  return clipboard_internal.IsReadAllowed(data_dst, format) &&
         clipboard_internal.IsFormatAvailable(format);
}
}  // namespace

bool ClipboardNonBacked::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (format == ClipboardFormatType::PlainTextType() ||
      format == ClipboardFormatType::UrlType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kText);
  }

  if (format == ClipboardFormatType::HtmlType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kHtml);
  }

  if (format == ClipboardFormatType::SvgType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kSvg);
  }

  if (format == ClipboardFormatType::RtfType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kRtf);
  }

  if (format == ClipboardFormatType::PngType() ||
      format == ClipboardFormatType::BitmapType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kPng);
  }

  if (format == ClipboardFormatType::WebKitSmartPasteType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kWeb);
  }

  if (format == ClipboardFormatType::FilenamesType()) {
    return IsReadAllowedAndAvailableFormat(clipboard_internal, data_dst,
                                           ClipboardInternalFormat::kFilenames);
  }

  const ClipboardData* data = clipboard_internal.GetData();
  return data && data->HasCustomDataFormat(format);
}

void ClipboardNonBacked::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  GetInternalClipboard(buffer).Clear();
}

std::vector<std::u16string> ClipboardNonBacked::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeText));
  }
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  }
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeSvg));
  }
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  }
  if (IsFormatAvailable(ClipboardFormatType::BitmapType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  }
  if (IsFormatAvailable(ClipboardFormatType::FilenamesType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeURIList));
  }
  return types;
}

void ClipboardNonBacked::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst, std::nullopt)) {
    return;
  }

  types->clear();
  *types = GetStandardFormats(buffer, data_dst);

  if (clipboard_internal.IsFormatAvailable(ClipboardInternalFormat::kCustom) &&
      clipboard_internal.GetData()) {
    ReadCustomDataTypes(
        base::as_bytes(base::span(
            clipboard_internal.GetData()->GetDataTransferCustomData())),
        types);
  }
}

void ClipboardNonBacked::ReadText(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kText)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kText);
  clipboard_internal.ReadText(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadAsciiText(ClipboardBuffer buffer,
                                       const DataTransferEndpoint* data_dst,
                                       std::string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kText)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kText);
  clipboard_internal.ReadAsciiText(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadHTML(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* markup,
                                  std::string* src_url,
                                  uint32_t* fragment_start,
                                  uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kHtml)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kHtml);
  clipboard_internal.ReadHTML(markup, src_url, fragment_start, fragment_end);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadSvg(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kSvg)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kSvg);
  clipboard_internal.ReadSvg(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadRTF(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kRtf)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kRtf);
  clipboard_internal.ReadRTF(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadPng(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kPng)) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kPng);
  clipboard_internal.ReadPng(std::move(callback));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadDataTransferCustomData(
    ClipboardBuffer buffer,
    const std::u16string& type,
    const DataTransferEndpoint* data_dst,
    std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(
          data_dst, ClipboardInternalFormat::kCustom,
          ClipboardFormatType::DataTransferCustomType())) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kCustomData);
  clipboard_internal.ReadDataTransferCustomData(type, result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadFilenames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kFilenames)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kFilenames);
  *result = clipboard_internal.ReadFilenames();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadBookmark(const DataTransferEndpoint* data_dst,
                                      std::u16string* title,
                                      std::string* url) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal =
      GetInternalClipboard(ClipboardBuffer::kCopyPaste);

  if (!clipboard_internal.IsReadAllowed(data_dst,
                                        ClipboardInternalFormat::kBookmark)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kBookmark);
  clipboard_internal.ReadBookmark(title, url);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadData(const ClipboardFormatType& format,
                                  const DataTransferEndpoint* data_dst,
                                  std::string* result) const {
  DCHECK(CalledOnValidThread());

  const ClipboardInternal& clipboard_internal =
      GetInternalClipboard(ClipboardBuffer::kCopyPaste);

  if (!clipboard_internal.IsReadAllowed(
          data_dst, ClipboardInternalFormat::kCustom, format)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal.GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kData);
  clipboard_internal.ReadData(format, result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

#if BUILDFLAG(IS_OZONE)
bool ClipboardNonBacked::IsSelectionBufferAvailable() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return true;
#endif
}
#endif  // BUILDFLAG(IS_OZONE)

void ClipboardNonBacked::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src,
    uint32_t privacy_types) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  ClipboardInternal& clipboard_internal = GetInternalClipboard(buffer);

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.second);

  ClipboardDataBuilder::CommitToClipboard(
      clipboard_internal, base::OptionalFromPtr(data_src.get()));
}

void ClipboardNonBacked::WriteText(std::string_view text) {
  ClipboardDataBuilder::WriteText(text);
}

void ClipboardNonBacked::WriteHTML(std::string_view markup,
                                   std::optional<std::string_view> source_url) {
  ClipboardDataBuilder::WriteHTML(markup, source_url);
}

void ClipboardNonBacked::WriteSvg(std::string_view markup) {
  ClipboardDataBuilder::WriteSvg(markup);
}

void ClipboardNonBacked::WriteRTF(std::string_view rtf) {
  ClipboardDataBuilder::WriteRTF(rtf);
}

void ClipboardNonBacked::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  ClipboardDataBuilder::WriteFilenames(std::move(filenames));
}

void ClipboardNonBacked::WriteBookmark(std::string_view title,
                                       std::string_view url) {
  ClipboardDataBuilder::WriteBookmark(title, url);
}

void ClipboardNonBacked::WriteWebSmartPaste() {
  ClipboardDataBuilder::WriteWebSmartPaste();
}

void ClipboardNonBacked::WriteBitmap(const SkBitmap& bitmap) {
  ClipboardDataBuilder::WriteBitmap(bitmap);
}

void ClipboardNonBacked::WriteData(const ClipboardFormatType& format,
                                   base::span<const uint8_t> data) {
  ClipboardDataBuilder::WriteData(format, data);
}

void ClipboardNonBacked::WriteClipboardHistory() {
  // TODO(crbug.com/40945200): Add support for this.
}

void ClipboardNonBacked::WriteUploadCloudClipboard() {
  // TODO(crbug.com/40945200): Add support for this.
}

void ClipboardNonBacked::WriteConfidentialDataForPassword() {
  // TODO(crbug.com/40945200): Add support for this.
}

const ClipboardInternal& ClipboardNonBacked::GetInternalClipboard(
    ClipboardBuffer buffer) const {
  return *internal_clipboards_.at(buffer);
}

ClipboardInternal& ClipboardNonBacked::GetInternalClipboard(
    ClipboardBuffer buffer) {
  return *internal_clipboards_.at(buffer);
}

}  // namespace ui
