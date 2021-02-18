// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_non_backed.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "build/chromeos_buildflags.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/buildflags.h"

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

  ~ClipboardInternal() = default;

  void Clear() {
    ++sequence_number_;
    data_.reset();
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  uint64_t sequence_number() const { return sequence_number_; }

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
  void ReadText(base::string16* result) const {
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
  void ReadHTML(base::string16* markup,
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
  void ReadSvg(base::string16* markup) const {
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

  // Reads image from the ClipboardData.
  SkBitmap ReadImage() const {
    SkBitmap img;
    if (!HasFormat(ClipboardInternalFormat::kBitmap))
      return img;

    // A shallow copy should be fine here, but just to be safe...
    const SkBitmap& clipboard_bitmap = GetData()->bitmap();
    if (img.tryAllocPixels(clipboard_bitmap.info())) {
      clipboard_bitmap.readPixels(img.info(), img.getPixels(), img.rowBytes(),
                                  0, 0);
    }
    return img;
  }

  // Reads data of type |type| from the ClipboardData.
  void ReadCustomData(const base::string16& type,
                      base::string16* result) const {
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
  void ReadBookmark(base::string16* title, std::string* url) const {
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
    ++sequence_number_;
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
    return previous_data;
  }

  bool IsReadAllowed(const DataTransferEndpoint* data_dst) const {
    DataTransferPolicyController* policy_controller =
        DataTransferPolicyController::Get();
    auto* data = GetData();
    if (!policy_controller || !data)
      return true;
    return policy_controller->IsClipboardReadAllowed(data->source(), data_dst);
  }

 private:
  // True if the ClipboardData has format |format|.
  bool HasFormat(ClipboardInternalFormat format) const {
    const ClipboardData* data = GetData();
    return data ? data->format() & static_cast<int>(format) : false;
  }

  // Current ClipboardData.
  std::unique_ptr<ClipboardData> data_;

  // Sequence number uniquely identifying clipboard state.
  uint64_t sequence_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClipboardInternal);
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

// linux-chromeos uses non-backed clipboard by default, but supports ozone x11
// with flag --use-system-clipbboard.
#if !BUILDFLAG(IS_CHROMEOS_ASH) || !BUILDFLAG(OZONE_PLATFORM_X11)
// Clipboard factory method.
Clipboard* Clipboard::Create() {
  return new ClipboardNonBacked;
}
#endif

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

  if (!clipboard_internal_->IsReadAllowed(data_dst))
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

uint64_t ClipboardNonBacked::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->sequence_number();
}

bool ClipboardNonBacked::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return false;

  if (format == ClipboardFormatType::GetPlainTextType() ||
      format == ClipboardFormatType::GetUrlType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kText);
  if (format == ClipboardFormatType::GetHtmlType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kHtml);
  if (format == ClipboardFormatType::GetSvgType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kSvg);
  if (format == ClipboardFormatType::GetRtfType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kRtf);
  if (format == ClipboardFormatType::GetBitmapType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kBitmap);
  if (format == ClipboardFormatType::GetWebKitSmartPasteType())
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kWeb);
  // Only support filenames if chrome://flags#clipboard-filenames is enabled.
  if (format == ClipboardFormatType::GetFilenamesType() &&
      base::FeatureList::IsEnabled(features::kClipboardFilenames))
    return clipboard_internal_->IsFormatAvailable(
        ClipboardInternalFormat::kFilenames);
  const ClipboardData* data = clipboard_internal_->GetData();
  return data && data->custom_data_format() == format.GetName();
}

void ClipboardNonBacked::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  clipboard_internal_->Clear();
}

void ClipboardNonBacked::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  types->clear();
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetPlainTextType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer, data_dst))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetHtmlType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer, data_dst))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetRtfType().GetName()));
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  if (IsFormatAvailable(ClipboardFormatType::GetFilenamesType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeURIList));

  if (clipboard_internal_->IsFormatAvailable(
          ClipboardInternalFormat::kCustom) &&
      clipboard_internal_->GetData()) {
    ReadCustomDataTypes(
        clipboard_internal_->GetData()->custom_data_data().c_str(),
        clipboard_internal_->GetData()->custom_data_data().size(), types);
  }
}

std::vector<base::string16>
ClipboardNonBacked::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  std::vector<base::string16> types;

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return types;

  // Includes all non-pickled AvailableTypes.
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst)) {
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetPlainTextType().GetName()));
  }
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer, data_dst)) {
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetHtmlType().GetName()));
  }
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer, data_dst)) {
    types.push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetRtfType().GetName()));
  }
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer,
                        data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  }

  return types;
}

void ClipboardNonBacked::ReadText(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  base::string16* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

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

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kText);
  clipboard_internal_->ReadAsciiText(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadHTML(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  base::string16* markup,
                                  std::string* src_url,
                                  uint32_t* fragment_start,
                                  uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kHtml);
  clipboard_internal_->ReadHTML(markup, src_url, fragment_start, fragment_end);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadSvg(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 base::string16* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kSvg);
  clipboard_internal_->ReadSvg(result);
}

void ClipboardNonBacked::ReadRTF(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kRtf);
  clipboard_internal_->ReadRTF(result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadImage(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   ReadImageCallback callback) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst)) {
    std::move(callback).Run(SkBitmap());
    return;
  }

  RecordRead(ClipboardFormatMetric::kImage);
  std::move(callback).Run(clipboard_internal_->ReadImage());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadCustomData(ClipboardBuffer buffer,
                                        const base::string16& type,
                                        const DataTransferEndpoint* data_dst,
                                        base::string16* result) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

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

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kFilenames);
  *result = clipboard_internal_->ReadFilenames();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

void ClipboardNonBacked::ReadBookmark(const DataTransferEndpoint* data_dst,
                                      base::string16* title,
                                      std::string* url) const {
  DCHECK(CalledOnValidThread());

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

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

  if (!clipboard_internal_->IsReadAllowed(data_dst))
    return;

  RecordRead(ClipboardFormatMetric::kData);
  clipboard_internal_->ReadData(format.GetName(), result);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ClipboardMonitor::GetInstance()->NotifyClipboardDataRead();
#endif
}

bool ClipboardNonBacked::IsSelectionBufferAvailable() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return true;
#endif
}

void ClipboardNonBacked::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
  ClipboardDataBuilder::CommitToClipboard(clipboard_internal_.get(),
                                          std::move(data_src));
}

void ClipboardNonBacked::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  DispatchPlatformRepresentations(std::move(platform_representations));

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
