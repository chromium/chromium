// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_x11.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/x/x11_clipboard_helper.h"
#include "ui/gfx/codec/png_codec.h"

namespace ui {

ClipboardX11::ClipboardX11()
    : x_clipboard_helper_(std::make_unique<XClipboardHelper>(
          base::BindRepeating(&ClipboardX11::OnSelectionChanged,
                              base::Unretained(this)))) {
  DCHECK(CalledOnValidThread());
}

ClipboardX11::~ClipboardX11() {
  DCHECK(CalledOnValidThread());
}

void ClipboardX11::OnPreShutdown() {
  DCHECK(CalledOnValidThread());
  x_clipboard_helper_->StoreCopyPasteDataAndWait();
}

DataTransferEndpoint* ClipboardX11::GetSource(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  auto it = data_src_.find(buffer);
  return it == data_src_.end() ? nullptr : it->second.get();
}

uint64_t ClipboardX11::GetSequenceNumber(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  return buffer == ClipboardBuffer::kCopyPaste ? clipboard_sequence_number_
                                               : primary_sequence_number_;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardX11::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  return x_clipboard_helper_->IsFormatAvailable(buffer, format);
}

void ClipboardX11::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));
  x_clipboard_helper_->Clear(buffer);
  data_src_[buffer].reset();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  types->clear();
  auto available_types = x_clipboard_helper_->GetAvailableTypes(buffer);
  for (const auto& mime_type : available_types) {
    // Special handling for chromium/x-web-custom-data. We must read the data
    // and deserialize it to find the list of mime types to report.
    if (mime_type == ClipboardFormatType::GetWebCustomDataType().GetName()) {
      auto data(x_clipboard_helper_->Read(
          buffer, x_clipboard_helper_->GetAtomsForFormat(
                      ClipboardFormatType::GetWebCustomDataType())));
      if (data.IsValid())
        ReadCustomDataTypes(data.GetData(), data.GetSize(), types);
    } else {
      types->push_back(base::UTF8ToUTF16(mime_type));
    }
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
std::vector<std::u16string>
ClipboardX11::ReadAvailablePlatformSpecificFormatNames(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  std::vector<std::u16string> format_names;
  for (const auto& name : x_clipboard_helper_->GetAvailableAtomNames(buffer))
    format_names.push_back(base::UTF8ToUTF16(name));
  return format_names;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadText(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  SelectionData data(
      x_clipboard_helper_->Read(buffer, x_clipboard_helper_->GetTextAtoms()));
  if (data.IsValid()) {
    std::string text = data.GetText();
    *result = base::UTF8ToUTF16(text);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadAsciiText(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kText);

  SelectionData data(
      x_clipboard_helper_->Read(buffer, x_clipboard_helper_->GetTextAtoms()));
  if (data.IsValid())
    result->assign(data.GetText());
}

// TODO(estade): handle different charsets.
// TODO(port): set *src_url.
// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadHTML(ClipboardBuffer buffer,
                            const DataTransferEndpoint* data_dst,
                            std::u16string* markup,
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

  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetHtmlType())));
  if (data.IsValid()) {
    *markup = data.GetHtml();

    *fragment_start = 0;
    DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
    *fragment_end = static_cast<uint32_t>(markup->length());
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadSvg(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kSvg);

  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetSvgType())));
  if (data.IsValid()) {
    std::string markup;
    data.AssignTo(&markup);
    *result = base::UTF8ToUTF16(markup);
  }
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadRTF(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kRtf);

  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetRtfType())));
  if (data.IsValid())
    data.AssignTo(result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadPng(ClipboardBuffer buffer,
                           const DataTransferEndpoint* data_dst,
                           ReadPngCallback callback) const {
  // TODO(crbug.com/1201018): Implement this.
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadImage(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             ReadImageCallback callback) const {
  DCHECK(IsSupportedClipboardBuffer(buffer));
  RecordRead(ClipboardFormatMetric::kImage);
  std::move(callback).Run(ReadImageInternal(buffer));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadCustomData(ClipboardBuffer buffer,
                                  const std::u16string& type,
                                  const DataTransferEndpoint* data_dst,
                                  std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kCustomData);

  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetWebCustomDataType())));
  if (data.IsValid())
    ReadCustomDataForType(data.GetData(), data.GetSize(), type, result);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadFilenames(ClipboardBuffer buffer,
                                 const DataTransferEndpoint* data_dst,
                                 std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kFilenames);

  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetFilenamesType())));
  std::string uri_list;
  if (data.IsValid())
    data.AssignTo(&uri_list);
  *result = ui::URIListToFileInfos(uri_list);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadBookmark(const DataTransferEndpoint* data_dst,
                                std::u16string* title,
                                std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(erg): This was left NOTIMPLEMENTED() in the gtk port too.
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::ReadData(const ClipboardFormatType& format,
                            const DataTransferEndpoint* data_dst,
                            std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);

  SelectionData data(x_clipboard_helper_->Read(
      ClipboardBuffer::kCopyPaste,
      x_clipboard_helper_->GetAtomsForFormat(format)));
  if (data.IsValid())
    data.AssignTo(result);
}

#if defined(USE_OZONE)
bool ClipboardX11::IsSelectionBufferAvailable() const {
  return true;
}
#endif  // defined(USE_OZONE)

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::WritePortableRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  x_clipboard_helper_->CreateNewClipboardData();
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
  x_clipboard_helper_->TakeOwnershipOfSelection(buffer);

  if (buffer == ClipboardBuffer::kCopyPaste) {
    auto text_iter = objects.find(PortableFormat::kText);
    if (text_iter != objects.end()) {
      x_clipboard_helper_->CreateNewClipboardData();
      const ObjectMapParams& params_vector = text_iter->second;
      if (params_vector.size()) {
        const ObjectMapParam& char_vector = params_vector[0];
        if (char_vector.size())
          WriteText(&char_vector.front(), char_vector.size());
      }
      x_clipboard_helper_->TakeOwnershipOfSelection(
          ClipboardBuffer::kSelection);
    }
  }

  data_src_[buffer] = std::move(data_src);
}

// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardX11::WritePlatformRepresentations(
    ClipboardBuffer buffer,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsSupportedClipboardBuffer(buffer));

  x_clipboard_helper_->CreateNewClipboardData();
  DispatchPlatformRepresentations(std::move(platform_representations));
  x_clipboard_helper_->TakeOwnershipOfSelection(buffer);
  data_src_[buffer] = std::move(data_src);
}

void ClipboardX11::WriteText(const char* text_data, size_t text_len) {
  std::string str(text_data, text_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  x_clipboard_helper_->InsertMapping(kMimeTypeText, mem);
  x_clipboard_helper_->InsertMapping(kMimeTypeLinuxText, mem);
  x_clipboard_helper_->InsertMapping(kMimeTypeLinuxString, mem);
  x_clipboard_helper_->InsertMapping(kMimeTypeLinuxUtf8String, mem);
}

void ClipboardX11::WriteHTML(const char* markup_data,
                             size_t markup_len,
                             const char* url_data,
                             size_t url_len) {
  // TODO(estade): We need to expand relative links with |url_data|.
  static const char* html_prefix =
      "<meta http-equiv=\"content-type\" "
      "content=\"text/html; charset=utf-8\">";
  std::string data = html_prefix;
  data += std::string(markup_data, markup_len);
  // Some programs expect '\0'-terminated data. See http://crbug.com/42624
  data += '\0';

  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&data));
  x_clipboard_helper_->InsertMapping(kMimeTypeHTML, mem);
}

void ClipboardX11::WriteSvg(const char* markup_data, size_t markup_len) {
  std::string str(markup_data, markup_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&str));

  x_clipboard_helper_->InsertMapping(kMimeTypeSvg, mem);
}

void ClipboardX11::WriteRTF(const char* rtf_data, size_t data_len) {
  WriteData(ClipboardFormatType::GetRtfType(), rtf_data, data_len);
}

void ClipboardX11::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  std::string uri_list = ui::FileInfosToURIList(filenames);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedString::TakeString(&uri_list));
  x_clipboard_helper_->InsertMapping(
      ClipboardFormatType::GetFilenamesType().GetName(), mem);
}

void ClipboardX11::WriteBookmark(const char* title_data,
                                 size_t title_len,
                                 const char* url_data,
                                 size_t url_len) {
  // Write as a mozilla url (UTF16: URL, newline, title).
  std::u16string url = base::UTF8ToUTF16(std::string(url_data, url_len) + "\n");
  std::u16string title =
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<unsigned char> data;
  AddString16ToVector(url, &data);
  AddString16ToVector(title, &data);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&data));

  x_clipboard_helper_->InsertMapping(kMimeTypeMozillaURL, mem);
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardX11::WriteWebSmartPaste() {
  std::string empty;
  x_clipboard_helper_->InsertMapping(
      kMimeTypeWebkitSmartPaste,
      scoped_refptr<base::RefCountedMemory>(
          base::RefCountedString::TakeString(&empty)));
}

void ClipboardX11::WriteBitmap(const SkBitmap& bitmap) {
  // Encode the bitmap as a PNG for transport.
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output)) {
    x_clipboard_helper_->InsertMapping(
        kMimeTypePNG, base::RefCountedBytes::TakeVector(&output));
  }
}

void ClipboardX11::WriteData(const ClipboardFormatType& format,
                             const char* data_data,
                             size_t data_len) {
  std::vector<unsigned char> bytes(data_data, data_data + data_len);
  scoped_refptr<base::RefCountedMemory> mem(
      base::RefCountedBytes::TakeVector(&bytes));
  x_clipboard_helper_->InsertMapping(format.GetName(), mem);
}

SkBitmap ClipboardX11::ReadImageInternal(ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

  // TODO(https://crbug.com/443355): Since now that ReadImage() is async,
  // refactor the code to keep a callback with the request, and invoke the
  // callback when the request is satisfied.
  SelectionData data(x_clipboard_helper_->Read(
      buffer, x_clipboard_helper_->GetAtomsForFormat(
                  ClipboardFormatType::GetBitmapType())));
  if (data.IsValid()) {
    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(data.GetData(), data.GetSize(), &bitmap))
      return SkBitmap(bitmap);
  }

  return SkBitmap();
}

void ClipboardX11::OnSelectionChanged(ClipboardBuffer buffer) {
  if (buffer == ClipboardBuffer::kCopyPaste)
    clipboard_sequence_number_++;
  else
    primary_sequence_number_++;
  ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
}

}  // namespace ui
