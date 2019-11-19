// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_aura.h"

#include <stdint.h>

#include <limits>
#include <list>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/buildflags.h"

namespace ui {

namespace {

const size_t kMaxClipboardSize = 1;

// Clipboard data format used by AuraClipboard.
enum class AuraClipboardFormat {
  kText = 1 << 0,
  kHtml = 1 << 1,
  kRtf = 1 << 2,
  kBookmark = 1 << 3,
  kBitmap = 1 << 4,
  kCustom = 1 << 5,
  kWeb = 1 << 6,
};

// ClipboardData contains data copied to the Clipboard for a variety of formats.
// It mostly just provides APIs to cleanly access and manipulate this data.
class ClipboardData {
 public:
  ClipboardData()
      : web_smart_paste_(false),
        format_(0) {}

  virtual ~ClipboardData() = default;

  // Bitmask of AuraClipboardFormat types.
  int format() const { return format_; }

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) {
    text_ = text;
    format_ |= static_cast<int>(AuraClipboardFormat::kText);
  }

  const std::string& markup_data() const { return markup_data_; }
  void set_markup_data(const std::string& markup_data) {
    markup_data_ = markup_data;
    format_ |= static_cast<int>(AuraClipboardFormat::kHtml);
  }

  const std::string& rtf_data() const { return rtf_data_; }
  void SetRTFData(const std::string& rtf_data) {
    rtf_data_ = rtf_data;
    format_ |= static_cast<int>(AuraClipboardFormat::kRtf);
  }

  const std::string& url() const { return url_; }
  void set_url(const std::string& url) {
    url_ = url;
    format_ |= static_cast<int>(AuraClipboardFormat::kHtml);
  }

  const std::string& bookmark_title() const { return bookmark_title_; }
  void set_bookmark_title(const std::string& bookmark_title) {
    bookmark_title_ = bookmark_title;
    format_ |= static_cast<int>(AuraClipboardFormat::kBookmark);
  }

  const std::string& bookmark_url() const { return bookmark_url_; }
  void set_bookmark_url(const std::string& bookmark_url) {
    bookmark_url_ = bookmark_url;
    format_ |= static_cast<int>(AuraClipboardFormat::kBookmark);
  }

  const SkBitmap& bitmap() const { return bitmap_; }
  void SetBitmapData(const SkBitmap& bitmap) {
    if (!skia::SkBitmapToN32OpaqueOrPremul(bitmap, &bitmap_)) {
      NOTREACHED() << "Unable to convert bitmap for clipboard";
      return;
    }
    format_ |= static_cast<int>(AuraClipboardFormat::kBitmap);
  }

  const std::string& custom_data_format() const { return custom_data_format_; }
  const std::string& custom_data_data() const { return custom_data_data_; }
  void SetCustomData(const std::string& data_format,
                     const std::string& data_data) {
    if (data_data.size() == 0) {
      custom_data_data_.clear();
      custom_data_format_.clear();
      return;
    }
    custom_data_data_ = data_data;
    custom_data_format_ = data_format;
    format_ |= static_cast<int>(AuraClipboardFormat::kCustom);
  }

  bool web_smart_paste() const { return web_smart_paste_; }
  void set_web_smart_paste(bool web_smart_paste) {
    web_smart_paste_ = web_smart_paste;
    format_ |= static_cast<int>(AuraClipboardFormat::kWeb);
  }

 private:
  // Plain text in UTF8 format.
  std::string text_;

  // HTML markup data in UTF8 format.
  std::string markup_data_;
  std::string url_;

  // RTF data.
  std::string rtf_data_;

  // Bookmark title in UTF8 format.
  std::string bookmark_title_;
  std::string bookmark_url_;

  // Filenames.
  std::vector<std::string> files_;

  // Bitmap images.
  SkBitmap bitmap_;

  // Data with custom format.
  std::string custom_data_format_;
  std::string custom_data_data_;

  // WebKit smart paste data.
  bool web_smart_paste_;

  int format_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardData);
};

}  // namespace

// Platform clipboard implementation for Aura. This handles things like format
// conversion, versioning of clipboard items etc. The goal is to roughly provide
// a substitute to platform clipboards on other platforms such as GtkClipboard
// on gtk or winapi clipboard on win.
class AuraClipboard {
 public:
  AuraClipboard() : sequence_number_(0) {
  }

  ~AuraClipboard() = default;

  void Clear() {
    sequence_number_++;
    data_list_.clear();
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  uint64_t sequence_number() const {
    return sequence_number_;
  }

  // Returns the data currently on the top of the clipboard stack, nullptr if
  // the clipboard stack is empty.
  const ClipboardData* GetData() const {
    if (data_list_.empty())
      return nullptr;
    return data_list_.front().get();
  }

  // Returns true if the data on top of the clipboard stack has format |format|
  // or another format that can be converted to |format|.
  bool IsFormatAvailable(AuraClipboardFormat format) const {
    switch (format) {
      case AuraClipboardFormat::kText:
        return HasFormat(AuraClipboardFormat::kText) ||
               HasFormat(AuraClipboardFormat::kBookmark);
      default:
        return HasFormat(format);
    }
  }

  // Reads text from the data at the top of clipboard stack.
  void ReadText(base::string16* result) const {
    std::string utf8_result;
    ReadAsciiText(&utf8_result);
    *result = base::UTF8ToUTF16(utf8_result);
  }

  // Reads ASCII text from the data at the top of clipboard stack.
  void ReadAsciiText(std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!data)
      return;
    if (HasFormat(AuraClipboardFormat::kText))
      *result = data->text();
    else if (HasFormat(AuraClipboardFormat::kHtml))
      *result = data->markup_data();
    else if (HasFormat(AuraClipboardFormat::kBookmark))
      *result = data->bookmark_url();
  }

  // Reads HTML from the data at the top of clipboard stack.
  void ReadHTML(base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const {
    markup->clear();
    if (src_url)
      src_url->clear();
    *fragment_start = 0;
    *fragment_end = 0;

    if (!HasFormat(AuraClipboardFormat::kHtml))
      return;

    const ClipboardData* data = GetData();
    *markup = base::UTF8ToUTF16(data->markup_data());
    *src_url = data->url();

    *fragment_start = 0;
    DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
    *fragment_end = static_cast<uint32_t>(markup->length());
  }

  // Reads RTF from the data at the top of clipboard stack.
  void ReadRTF(std::string* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(AuraClipboardFormat::kRtf))
      return;

    *result = data->rtf_data();
  }

  // Reads image from the data at the top of clipboard stack.
  SkBitmap ReadImage() const {
    SkBitmap img;
    if (!HasFormat(AuraClipboardFormat::kBitmap))
      return img;

    // A shallow copy should be fine here, but just to be safe...
    const SkBitmap& clipboard_bitmap = GetData()->bitmap();
    if (img.tryAllocPixels(clipboard_bitmap.info())) {
      clipboard_bitmap.readPixels(img.info(), img.getPixels(), img.rowBytes(),
                                  0, 0);
    }
    return img;
  }

  // Reads data of type |type| from the data at the top of clipboard stack.
  void ReadCustomData(const base::string16& type,
                      base::string16* result) const {
    result->clear();
    const ClipboardData* data = GetData();
    if (!HasFormat(AuraClipboardFormat::kCustom))
      return;

    ui::ReadCustomDataForType(data->custom_data_data().c_str(),
        data->custom_data_data().size(),
        type, result);
  }

  // Reads bookmark from the data at the top of clipboard stack.
  void ReadBookmark(base::string16* title, std::string* url) const {
    if (title)
      title->clear();
    if (url)
      url->clear();
    if (!HasFormat(AuraClipboardFormat::kBookmark))
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
    if (!HasFormat(AuraClipboardFormat::kCustom) ||
        type != data->custom_data_format())
      return;

    *result = data->custom_data_data();
  }

  // Writes |data| to the top of the clipboard stack.
  void WriteData(std::unique_ptr<ClipboardData> data) {
    DCHECK(data);
    AddToListEnsuringSize(std::move(data));
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

 private:
  // True if the data on top of the clipboard stack has format |format|.
  bool HasFormat(AuraClipboardFormat format) const {
    const ClipboardData* data = GetData();
    return data ? data->format() & static_cast<int>(format) : false;
  }

  void AddToListEnsuringSize(std::unique_ptr<ClipboardData> data) {
    DCHECK(data);
    sequence_number_++;
    data_list_.push_front(std::move(data));

    // If the size of list becomes more than the maximum allowed, we delete the
    // last element.
    if (data_list_.size() > kMaxClipboardSize) {
      data_list_.pop_back();
    }
  }

  // Stack containing various versions of ClipboardData.
  std::list<std::unique_ptr<ClipboardData>> data_list_;

  // Sequence number uniquely identifying clipboard state.
  uint64_t sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(AuraClipboard);
};

// Helper class to build a ClipboardData object and write it to clipboard.
class ClipboardDataBuilder {
 public:
  static void CommitToClipboard(AuraClipboard* clipboard) {
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

  static void WriteRTF(const char* rtf_data, size_t rtf_len) {
    ClipboardData* data = GetCurrentData();
    data->SetRTFData(std::string(rtf_data, rtf_len));
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

// linux-chromeos uses aura clipboard by default, but supports ozone x11
// with flag --use-system-clipbboard.
#if !defined(OS_CHROMEOS) || !BUILDFLAG(OZONE_PLATFORM_X11)
// Clipboard factory method.
Clipboard* Clipboard::Create() {
  return new ClipboardAura;
}
#endif

// ClipboardAura implementation.
ClipboardAura::ClipboardAura()
    : clipboard_internal_(std::make_unique<AuraClipboard>()) {
  DCHECK(CalledOnValidThread());
}

ClipboardAura::~ClipboardAura() {
  DCHECK(CalledOnValidThread());
}

void ClipboardAura::OnPreShutdown() {}

uint64_t ClipboardAura::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->sequence_number();
}

bool ClipboardAura::IsFormatAvailable(const ClipboardFormatType& format,
                                      ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  if (ClipboardFormatType::GetPlainTextType().Equals(format) ||
      ClipboardFormatType::GetUrlType().Equals(format))
    return clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kText);
  if (ClipboardFormatType::GetHtmlType().Equals(format))
    return clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kHtml);
  if (ClipboardFormatType::GetRtfType().Equals(format))
    return clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kRtf);
  if (ClipboardFormatType::GetBitmapType().Equals(format))
    return clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kBitmap);
  if (ClipboardFormatType::GetWebKitSmartPasteType().Equals(format))
    return clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kWeb);
  const ClipboardData* data = clipboard_internal_->GetData();
  return data && data->custom_data_format() == format.ToString();
}

void ClipboardAura::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  clipboard_internal_->Clear();
}

void ClipboardAura::ReadAvailableTypes(ClipboardBuffer buffer,
                                       std::vector<base::string16>* types,
                                       bool* contains_filenames) const {
  DCHECK(CalledOnValidThread());
  if (!types || !contains_filenames) {
    NOTREACHED();
    return;
  }

  types->clear();
  *contains_filenames = false;
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetPlainTextType().ToString()));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetHtmlType().ToString()));
  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer))
    types->push_back(
        base::UTF8ToUTF16(ClipboardFormatType::GetRtfType().ToString()));
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));

  if (clipboard_internal_->IsFormatAvailable(AuraClipboardFormat::kCustom) &&
      clipboard_internal_->GetData()) {
    ui::ReadCustomDataTypes(
        clipboard_internal_->GetData()->custom_data_data().c_str(),
        clipboard_internal_->GetData()->custom_data_data().size(), types);
  }
}

void ClipboardAura::ReadText(ClipboardBuffer buffer,
                             base::string16* result) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadText(result);
}

void ClipboardAura::ReadAsciiText(ClipboardBuffer buffer,
                                  std::string* result) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadAsciiText(result);
}

void ClipboardAura::ReadHTML(ClipboardBuffer buffer,
                             base::string16* markup,
                             std::string* src_url,
                             uint32_t* fragment_start,
                             uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadHTML(markup, src_url, fragment_start, fragment_end);
}

void ClipboardAura::ReadRTF(ClipboardBuffer buffer, std::string* result) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadRTF(result);
}

SkBitmap ClipboardAura::ReadImage(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return clipboard_internal_->ReadImage();
}

void ClipboardAura::ReadCustomData(ClipboardBuffer buffer,
                                   const base::string16& type,
                                   base::string16* result) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadCustomData(type, result);
}

void ClipboardAura::ReadBookmark(base::string16* title,
                                 std::string* url) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadBookmark(title, url);
}

void ClipboardAura::ReadData(const ClipboardFormatType& format,
                             std::string* result) const {
  DCHECK(CalledOnValidThread());
  clipboard_internal_->ReadData(format.ToString(), result);
}

void ClipboardAura::WritePortableRepresentations(ClipboardBuffer buffer,
                                                 const ObjectMap& objects) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
  ClipboardDataBuilder::CommitToClipboard(clipboard_internal_.get());
}

void ClipboardAura::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  DispatchPlatformRepresentations(std::move(platform_representations));

  ClipboardDataBuilder::CommitToClipboard(clipboard_internal_.get());
}

void ClipboardAura::WriteText(const char* text_data, size_t text_len) {
  ClipboardDataBuilder::WriteText(text_data, text_len);
}

void ClipboardAura::WriteHTML(const char* markup_data,
                              size_t markup_len,
                              const char* url_data,
                              size_t url_len) {
  ClipboardDataBuilder::WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardAura::WriteRTF(const char* rtf_data, size_t data_len) {
  ClipboardDataBuilder::WriteRTF(rtf_data, data_len);
}

void ClipboardAura::WriteBookmark(const char* title_data,
                                  size_t title_len,
                                  const char* url_data,
                                  size_t url_len) {
  ClipboardDataBuilder::WriteBookmark(title_data, title_len, url_data, url_len);
}

void ClipboardAura::WriteWebSmartPaste() {
  ClipboardDataBuilder::WriteWebSmartPaste();
}

void ClipboardAura::WriteBitmap(const SkBitmap& bitmap) {
  ClipboardDataBuilder::WriteBitmap(bitmap);
}

void ClipboardAura::WriteData(const ClipboardFormatType& format,
                              const char* data_data,
                              size_t data_len) {
  ClipboardDataBuilder::WriteData(format.ToString(), data_data, data_len);
}

}  // namespace ui
