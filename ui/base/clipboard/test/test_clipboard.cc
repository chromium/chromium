// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/test/test_clipboard.h"

#include <stddef.h>
#include <memory>
#include <utility>
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"

namespace ui {

namespace {
bool IsReadAllowed(const DataTransferEndpoint* src,
                   const DataTransferEndpoint* dst) {
  auto* policy_controller = DataTransferPolicyController::Get();
  if (!policy_controller)
    return true;
  return policy_controller->IsClipboardReadAllowed(src, dst);
}
}  // namespace

TestClipboard::TestClipboard()
    : default_store_buffer_(ClipboardBuffer::kCopyPaste) {}

TestClipboard::~TestClipboard() = default;

TestClipboard* TestClipboard::CreateForCurrentThread() {
  base::AutoLock lock(Clipboard::ClipboardMapLock());
  auto* clipboard = new TestClipboard;
  (*Clipboard::ClipboardMapPtr())[base::PlatformThread::CurrentId()] =
      base::WrapUnique(clipboard);
  return clipboard;
}

void TestClipboard::SetLastModifiedTime(const base::Time& time) {
  last_modified_time_ = time;
}

void TestClipboard::OnPreShutdown() {}

DataTransferEndpoint* TestClipboard::GetSource(ClipboardBuffer buffer) const {
  return GetStore(buffer).GetDataSource();
}

uint64_t TestClipboard::GetSequenceNumber(ClipboardBuffer buffer) const {
  return GetStore(buffer).sequence_number;
}

bool TestClipboard::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const ui::DataTransferEndpoint* data_dst) const {
  if (!IsReadAllowed(GetStore(buffer).data_src.get(), data_dst))
    return false;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // The linux clipboard treats the presence of text on the clipboard
  // as the url format being available.
  if (format == ClipboardFormatType::GetUrlType())
    return IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                             data_dst);
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
  const DataStore& store = GetStore(buffer);
  if (format == ClipboardFormatType::GetFilenamesType())
    return !store.filenames.empty();
  return base::Contains(store.data, format);
}

void TestClipboard::Clear(ClipboardBuffer buffer) {
  GetStore(buffer).Clear();
}

void TestClipboard::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<base::string16>* types) const {
  DCHECK(types);
  types->clear();
  if (!IsReadAllowed(GetStore(buffer).data_src.get(), data_dst))
    return;

  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::GetHtmlType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeHTML));

  if (IsFormatAvailable(ClipboardFormatType::GetRtfType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  if (IsFormatAvailable(ClipboardFormatType::GetBitmapType(), buffer, data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypePNG));
  if (IsFormatAvailable(ClipboardFormatType::GetFilenamesType(), buffer,
                        data_dst))
    types->push_back(base::UTF8ToUTF16(kMimeTypeURIList));
}

std::vector<base::string16>
TestClipboard::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const ui::DataTransferEndpoint* data_dst) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return {};

  const auto& data = store.data;
  std::vector<base::string16> types;
  types.reserve(data.size());
  for (const auto& it : data)
    types.push_back(base::UTF8ToUTF16(it.first.GetName()));

  // Some platforms add additional raw types to represent text, or offer them
  // as available formats by automatically converting between them.
  if (IsFormatAvailable(ClipboardFormatType::GetPlainTextType(), buffer,
                        data_dst)) {
#if defined(USE_X11)
    types.push_back(base::ASCIIToUTF16("TEXT"));
    types.push_back(base::ASCIIToUTF16("STRING"));
    types.push_back(base::ASCIIToUTF16("UTF8_STRING"));
#elif defined(OS_WIN)
    types.push_back(base::ASCIIToUTF16("CF_OEMTEXT"));
#elif defined(OS_APPLE)
    types.push_back(base::ASCIIToUTF16("NSStringPboardType"));
#endif
  }

  return types;
}

void TestClipboard::ReadText(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             base::string16* result) const {
  if (!IsReadAllowed(GetStore(buffer).data_src.get(), data_dst))
    return;

  std::string result8;
  ReadAsciiText(buffer, data_dst, &result8);
  *result = base::UTF8ToUTF16(result8);
}

// TODO(crbug.com/1103215): |data_dst| should be supported.
void TestClipboard::ReadAsciiText(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::string* result) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  result->clear();
  auto it = store.data.find(ClipboardFormatType::GetPlainTextType());
  if (it != store.data.end())
    *result = it->second;
}

void TestClipboard::ReadHTML(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             base::string16* markup,
                             std::string* src_url,
                             uint32_t* fragment_start,
                             uint32_t* fragment_end) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  markup->clear();
  src_url->clear();
  auto it = store.data.find(ClipboardFormatType::GetHtmlType());
  if (it != store.data.end())
    *markup = base::UTF8ToUTF16(it->second);
  *src_url = store.html_src_url;
  *fragment_start = 0;
  *fragment_end = base::checked_cast<uint32_t>(markup->size());
}

void TestClipboard::ReadSvg(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            base::string16* result) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  result->clear();
  auto it = store.data.find(ClipboardFormatType::GetSvgType());
  if (it != store.data.end())
    *result = base::UTF8ToUTF16(it->second);
}

void TestClipboard::ReadRTF(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  result->clear();
  auto it = store.data.find(ClipboardFormatType::GetRtfType());
  if (it != store.data.end())
    *result = it->second;
}

void TestClipboard::ReadImage(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              ReadImageCallback callback) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst)) {
    std::move(callback).Run(SkBitmap());
    return;
  }
  std::move(callback).Run(store.image);
}

// TODO(crbug.com/1103215): |data_dst| should be supported.
void TestClipboard::ReadCustomData(ClipboardBuffer buffer,
                                   const base::string16& type,
                                   const DataTransferEndpoint* data_dst,
                                   base::string16* result) const {}

void TestClipboard::ReadFilenames(ClipboardBuffer buffer,
                                  const DataTransferEndpoint* data_dst,
                                  std::vector<ui::FileInfo>* result) const {
  const DataStore& store = GetStore(buffer);
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  *result = store.filenames;
}

// TODO(crbug.com/1103215): |data_dst| should be supported.
void TestClipboard::ReadBookmark(const DataTransferEndpoint* data_dst,
                                 base::string16* title,
                                 std::string* url) const {
  const DataStore& store = GetDefaultStore();
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  if (url) {
    auto it = store.data.find(ClipboardFormatType::GetUrlType());
    if (it != store.data.end())
      *url = it->second;
  }
  if (title)
    *title = base::UTF8ToUTF16(store.url_title);
}

void TestClipboard::ReadData(const ClipboardFormatType& format,
                             const DataTransferEndpoint* data_dst,
                             std::string* result) const {
  const DataStore& store = GetDefaultStore();
  if (!IsReadAllowed(store.data_src.get(), data_dst))
    return;

  result->clear();
  auto it = store.data.find(format);
  if (it != store.data.end())
    *result = it->second;
}

base::Time TestClipboard::GetLastModifiedTime() const {
  return last_modified_time_;
}

void TestClipboard::ClearLastModifiedTime() {
  last_modified_time_ = base::Time();
}

#if defined(USE_OZONE)
bool TestClipboard::IsSelectionBufferAvailable() const {
  return true;
}
#endif  // defined(USE_OZONE)

void TestClipboard::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  Clear(buffer);
  default_store_buffer_ = buffer;
  for (const auto& kv : objects)
    DispatchPortableRepresentation(kv.first, kv.second);
  default_store_buffer_ = ClipboardBuffer::kCopyPaste;
  GetStore(buffer).SetDataSource(std::move(data_src));
}

void TestClipboard::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  Clear(buffer);
  default_store_buffer_ = buffer;
  DispatchPlatformRepresentations(std::move(platform_representations));
  default_store_buffer_ = ClipboardBuffer::kCopyPaste;
  GetStore(buffer).SetDataSource(std::move(data_src));
}

void TestClipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  GetDefaultStore().data[ClipboardFormatType::GetPlainTextType()] = text;
#if defined(OS_WIN)
  // Create a dummy entry.
  GetDefaultStore().data[ClipboardFormatType::GetPlainTextAType()];
#endif
  if (IsSupportedClipboardBuffer(ClipboardBuffer::kSelection))
    GetStore(ClipboardBuffer::kSelection)
        .data[ClipboardFormatType::GetPlainTextType()] = text;
  ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
}

void TestClipboard::WriteHTML(const char* markup_data,
                              size_t markup_len,
                              const char* url_data,
                              size_t url_len) {
  base::string16 markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  GetDefaultStore().data[ClipboardFormatType::GetHtmlType()] =
      base::UTF16ToUTF8(markup);
  GetDefaultStore().html_src_url = std::string(url_data, url_len);
}

void TestClipboard::WriteSvg(const char* markup_data, size_t markup_len) {
  base::string16 markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  GetDefaultStore().data[ClipboardFormatType::GetSvgType()] =
      base::UTF16ToUTF8(markup);
}

void TestClipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  GetDefaultStore().data[ClipboardFormatType::GetRtfType()] =
      std::string(rtf_data, data_len);
}

void TestClipboard::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  GetDefaultStore().filenames = std::move(filenames);
}

void TestClipboard::WriteBookmark(const char* title_data,
                                  size_t title_len,
                                  const char* url_data,
                                  size_t url_len) {
  GetDefaultStore().data[ClipboardFormatType::GetUrlType()] =
      std::string(url_data, url_len);
  GetDefaultStore().url_title = std::string(title_data, title_len);
}

void TestClipboard::WriteWebSmartPaste() {
  // Create a dummy entry.
  GetDefaultStore().data[ClipboardFormatType::GetWebKitSmartPasteType()];
}

void TestClipboard::WriteBitmap(const SkBitmap& bitmap) {
  // We expect callers to sanitize `bitmap` to be N32 color type, to avoid
  // out-of-bounds issues due to unexpected bits-per-pixel while copying the
  // bitmap's pixel buffer. This DCHECK is to help alert us if we've missed
  // something.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  // Create a dummy entry.
  GetDefaultStore().data[ClipboardFormatType::GetBitmapType()];
  GetDefaultStore().image = bitmap;
  ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
}

void TestClipboard::WriteData(const ClipboardFormatType& format,
                              const char* data_data,
                              size_t data_len) {
  GetDefaultStore().data[format] = std::string(data_data, data_len);
}

TestClipboard::DataStore::DataStore() = default;

TestClipboard::DataStore::DataStore(const DataStore& other) {
  sequence_number = other.sequence_number;
  data = other.data;
  url_title = other.url_title;
  html_src_url = other.html_src_url;
  image = other.image;
  data_src = other.data_src ? std::make_unique<DataTransferEndpoint>(
                                  DataTransferEndpoint(*(other.data_src)))
                            : nullptr;
}

TestClipboard::DataStore& TestClipboard::DataStore::operator=(
    const DataStore& other) {
  sequence_number = other.sequence_number;
  data = other.data;
  url_title = other.url_title;
  html_src_url = other.html_src_url;
  image = other.image;
  data_src = other.data_src ? std::make_unique<DataTransferEndpoint>(
                                  DataTransferEndpoint(*(other.data_src)))
                            : nullptr;
  return *this;
}

TestClipboard::DataStore::~DataStore() = default;

void TestClipboard::DataStore::Clear() {
  data.clear();
  url_title.clear();
  html_src_url.clear();
  image = SkBitmap();
  data_src.reset();
}

void TestClipboard::DataStore::SetDataSource(
    std::unique_ptr<DataTransferEndpoint> data_src) {
  this->data_src = std::move(data_src);
}

DataTransferEndpoint* TestClipboard::DataStore::GetDataSource() const {
  return this->data_src.get();
}

const TestClipboard::DataStore& TestClipboard::GetStore(
    ClipboardBuffer buffer) const {
  CHECK(IsSupportedClipboardBuffer(buffer));
  return stores_[buffer];
}

TestClipboard::DataStore& TestClipboard::GetStore(ClipboardBuffer buffer) {
  CHECK(IsSupportedClipboardBuffer(buffer));
  DataStore& store = stores_[buffer];
  ++store.sequence_number;
  return store;
}

const TestClipboard::DataStore& TestClipboard::GetDefaultStore() const {
  return GetStore(default_store_buffer_);
}

TestClipboard::DataStore& TestClipboard::GetDefaultStore() {
  return GetStore(default_store_buffer_);
}

}  // namespace ui
