// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_sequence_number_token.h"
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
        base::BindOnce(&ClipboardData::EncodeBitmapData,
                       std::move(maybe_bitmap.value())),
        base::BindOnce(&ClipboardInternal::DidEncodePng,
                       weak_factory_.GetMutableWeakPtr(), sequence_number()));
  }

  // Reads data of type |type| from the ClipboardData.
  void ReadCustomData(const std::u16string& type,
                      std::u16string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(ClipboardInternalFormat::kCustom))
      return;

    ReadCustomDataForType(data->custom_data_data().c_str(),
                          data->custom_data_data().size(), type, result);
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

  void ReadData(const std::string& type, std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(ClipboardInternalFormat::kCustom) ||
        type != data->custom_data_format())
      return;
    *result = data->custom_data_data();
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
                     absl::optional<ClipboardInternalFormat> format) const {
    DataTransferPolicyController* policy_controller =
        DataTransferPolicyController::Get();
    auto* data = GetData();
    if (!policy_controller || !data)
      return true;
    return policy_controller->IsClipboardReadAllowed(data->source(), data_dst,
                                                     data->size(format));
  }

  int NumImagesEncodedForTesting() { return num_images_encoded_for_testing_; }

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
  static void CommitToClipboard(
      ClipboardInternal* clipboard,
      std::unique_ptr<DataTransferEndpoint> data_src) {
    ClipboardData* data = GetCurrentData();
#if BUILDFLAG(IS_CHROMEOS)
    data->set_commit_time(base::Time::Now());
#endif  // BUILDFLAG(IS_CHROMEOS)
    data->set_source(std::move(data_src));
    clipboard->WriteData(TakeCurrentData());
  }

  static void WriteText(const char* text_data, size_t text_len) {
    ClipboardData* data = GetCurrentData();
    data->set_text(std::string(text_data, text_len));
  }

  static void WriteHTML(const char* markup_data,
                        size_t markup_len,
                        const char* url_data,
                        size_t url_len) {
    ClipboardData* data = GetCurrentData();
    data->set_markup_data(std::string(markup_data, markup_len));
    data->set_url(std::string(url_data, url_len));
  }

  static void WriteSvg(const char* markup_data, size_t markup_len) {
    ClipboardData* data = GetCurrentData();
    data->set_svg_data(std::string(markup_data, markup_len));
  }

  static void WriteRTF(const char* rtf_data, size_t rtf_len) {
    ClipboardData* data = GetCurrentData();
    data->SetRTFData(std::string(rtf_data, rtf_len));
  }

  static void WriteFilenames(std::vector<ui::FileInfo> filenames) {
    ClipboardData* data = GetCurrentData();
    data->set_filenames(std::move(filenames));
  }

  static void WriteBookmark(const char* title_data,
                            size_t title_len,
                            const char* url_data,
                            size_t url_len) {
    ClipboardData* data = GetCurrentData();
    data->set_bookmark_title(std::string(title_data, title_len));
    data->set_bookmark_url(std::string(url_data, url_len));
  }

  static void WriteWebSmartPaste() {
    ClipboardData* data = GetCurrentData();
    data->set_web_smart_paste(true);
  }

  static void WriteBitmap(const SkBitmap& bitmap) {
    ClipboardData* data = GetCurrentData();
    data->SetBitmapData(bitmap);
  }

  static void WriteData(const std::string& format,
                        const char* data_data,
                        size_t data_len) {
    ClipboardData* data = GetCurrentData();
    data->SetCustomData(format, std::string(data_data, data_len));
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
ClipboardNonBacked::ClipboardNonBacked()
    : clipboard_internal_(std::make_unique<ClipboardInternal>()) {
  DCHECK(CalledOnValidThread());
  RegisterInstance(this);
}

ClipboardNonBacked::~ClipboardNonBacked() {
  DCHECK(CalledOnValidThread());
  UnregisterInstance(this);
}

const ClipboardData* ClipboardNonBacked::GetClipboardData(
    DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst, absl::nullopt))
    return nullptr;

  return clipboard_internal_->GetData();
}

std::unique_ptr<ClipboardData> ClipboardNonBacked::WriteClipboardData(
    std::unique_ptr<ClipboardData> data) {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->WriteData(std::move(data));
}

void ClipboardNonBacked::OnPreShutdown() {}

DataTransferEndpoint* ClipboardNonBacked::GetSource(
    ClipboardBuffer buffer) const {
  const ClipboardData* data = clipboard_internal_->GetData();
  return data ? data->source() : nullptr;
}

const ClipboardSequenceNumberToken& ClipboardNonBacked::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->sequence_number();
}

int ClipboardNonBacked::NumImagesEncodedForTesting() const {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->NumImagesEncodedForTesting();  // IN-TEST
}

bool ClipboardNonBacked::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  if (format == ClipboardFormatType::PlainTextType() ||
      format == ClipboardFormatType::UrlType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kText) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kText);
  if (format == ClipboardFormatType::HtmlType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kHtml) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kHtml);
  if (format == ClipboardFormatType::SvgType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kSvg) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kSvg);
  if (format == ClipboardFormatType::RtfType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kRtf) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kRtf);
  if (format == ClipboardFormatType::PngType() ||
      format == ClipboardFormatType::BitmapType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kPng) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kPng);
  if (format == ClipboardFormatType::WebKitSmartPasteType())
    return clipboard_internal_->IsReadAllowed(data_dst,
                                              ClipboardInternalFormat::kWeb) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kWeb);
  if (format == ClipboardFormatType::FilenamesType())
    return clipboard_internal_->IsReadAllowed(
               data_dst, ClipboardInternalFormat::kFilenames) &&
           clipboard_internal_->IsFormatAvailable(
               ClipboardInternalFormat::kFilenames);
  const ClipboardData* data = clipboard_internal_->GetData();
  return data && data->custom_data_format() == format.GetName();
}

void ClipboardNonBacked::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  clipboard_internal_->Clear();
}

std::vector<std::u16string> ClipboardNonBacked::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer, data_dst))
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::PlainTextType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst))
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::HtmlType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst))
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::SvgType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst))
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::RtfType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::BitmapType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  if (IsFormatAvailable(ClipboardFormatType::FilenamesType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeURIList));
  return types;
}

void ClipboardNonBacked::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  if (!clipboard_internal_->IsReadAllowed(data_dst, absl::nullopt))
    return;

  types->clear();
  *types = GetStandardFormats(buffer, data_dst);

  if (clipboard_internal_->IsFormatAvailable(
          ClipboardInternalFormat::kCustom) &&
      clipboard_internal_->GetData()) {
    ReadCustomDataTypes(
        clipboard_internal_->GetData()->custom_data_data().c_str(),
        clipboard_internal_->GetData()->custom_data_data().size(), types);
  }
}

void ClipboardNonBacked::ReadText(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kText))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kText);
  clipboard_internal_->ReadText(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadAsciiText(ClipboardBuffer buffer,
                                       const DataTransferEndpoint* data_dst,
                                       std::string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kText))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kText);
  clipboard_internal_->ReadAsciiText(result);

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

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kHtml))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kHtml);
  clipboard_internal_->ReadHTML(markup, src_url, fragment_start, fragment_end);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadSvg(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kSvg))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kSvg);
  clipboard_internal_->ReadSvg(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadRTF(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kRtf))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kRtf);
  clipboard_internal_->ReadRTF(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadPng(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kPng)) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kPng);
  clipboard_internal_->ReadPng(std::move(callback));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadCustomData(ClipboardBuffer buffer,
                                        const std::u16string& type,
                                        const DataTransferEndpoint* data_dst,
                                        std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kCustom))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kCustomData);
  clipboard_internal_->ReadCustomData(type, result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadFilenames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kFilenames))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kFilenames);
  *result = clipboard_internal_->ReadFilenames();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadBookmark(const DataTransferEndpoint* data_dst,
                                      std::u16string* title,
                                      std::string* url) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst,
                                          ClipboardInternalFormat::kBookmark))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kBookmark);
  clipboard_internal_->ReadBookmark(title, url);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadData(const ClipboardFormatType& format,
                                  const DataTransferEndpoint* data_dst,
                                  std::string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst, absl::nullopt))
    return;

#if BUILDFLAG(IS_CHROMEOS)
  RecordTimeIntervalBetweenCommitAndRead(clipboard_internal_->GetData());
#endif  // BUILDFLAG(IS_CHROMEOS)

  RecordRead(ClipboardFormatMetric::kData);
  clipboard_internal_->ReadData(format.GetName(), result);

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
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);

  ClipboardDataBuilder::CommitToClipboard(clipboard_internal_.get(),
                                          std::move(data_src));
}

void ClipboardNonBacked::WriteText(const char* text_data, size_t text_len) {
  ClipboardDataBuilder::WriteText(text_data, text_len);
}

void ClipboardNonBacked::WriteHTML(const char* markup_data,
                                   size_t markup_len,
                                   const char* url_data,
                                   size_t url_len) {
  ClipboardDataBuilder::WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardNonBacked::WriteUnsanitizedHTML(const char* markup_data,
                                              size_t markup_len,
                                              const char* url_data,
                                              size_t url_len) {
  ClipboardDataBuilder::WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardNonBacked::WriteSvg(const char* markup_data, size_t markup_len) {
  ClipboardDataBuilder::WriteSvg(markup_data, markup_len);
}

void ClipboardNonBacked::WriteRTF(const char* rtf_data, size_t data_len) {
  ClipboardDataBuilder::WriteRTF(rtf_data, data_len);
}

void ClipboardNonBacked::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  ClipboardDataBuilder::WriteFilenames(std::move(filenames));
}

void ClipboardNonBacked::WriteBookmark(const char* title_data,
                                       size_t title_len,
                                       const char* url_data,
                                       size_t url_len) {
  ClipboardDataBuilder::WriteBookmark(title_data, title_len, url_data, url_len);
}

void ClipboardNonBacked::WriteWebSmartPaste() {
  ClipboardDataBuilder::WriteWebSmartPaste();
}

void ClipboardNonBacked::WriteBitmap(const SkBitmap& bitmap) {
  ClipboardDataBuilder::WriteBitmap(bitmap);
}

void ClipboardNonBacked::WriteData(const ClipboardFormatType& format,
                                   const char* data_data,
                                   size_t data_len) {
  ClipboardDataBuilder::WriteData(format.GetName(), data_data, data_len);
}

}  // namespace ui
