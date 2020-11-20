// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include <memory>
#include <utility>

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/gfx/geometry/size.h"

// Documentation on the format of the parameters for each clipboard target can
// be found in clipboard.h.
namespace ui {

ScopedClipboardWriter::ScopedClipboardWriter(
    ClipboardBuffer buffer,
    std::unique_ptr<ClipboardDataEndpoint> data_src)
    : buffer_(buffer), data_src_(std::move(data_src)) {}

ScopedClipboardWriter::~ScopedClipboardWriter() {
  static constexpr size_t kMaxRepresentations = 1 << 12;
  DCHECK(objects_.empty() || platform_representations_.empty())
      << "Portable and Platform representations should not be written on the "
         "same write.";
  DCHECK(platform_representations_.size() < kMaxRepresentations);
  if (!objects_.empty()) {
    Clipboard::GetForCurrentThread()->WritePortableRepresentations(
        buffer_, objects_, std::move(data_src_));
  } else if (!platform_representations_.empty()) {
    Clipboard::GetForCurrentThread()->WritePlatformRepresentations(
        buffer_, std::move(platform_representations_), std::move(data_src_));
  }

  if (confidential_)
    Clipboard::GetForCurrentThread()->MarkAsConfidential();
}

void ScopedClipboardWriter::WriteText(const base::string16& text) {
  RecordWrite(ClipboardFormatMetric::kText);
  std::string utf8_text = base::UTF16ToUTF8(text);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(
      Clipboard::ObjectMapParam(utf8_text.begin(), utf8_text.end()));
  objects_[Clipboard::PortableFormat::kText] = parameters;
}

void ScopedClipboardWriter::WriteHTML(const base::string16& markup,
                                      const std::string& source_url) {
  RecordWrite(ClipboardFormatMetric::kHtml);
  std::string utf8_markup = base::UTF16ToUTF8(markup);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(
      Clipboard::ObjectMapParam(utf8_markup.begin(),
                                utf8_markup.end()));
  if (!source_url.empty()) {
    parameters.push_back(Clipboard::ObjectMapParam(source_url.begin(),
                                                   source_url.end()));
  }

  objects_[Clipboard::PortableFormat::kHtml] = parameters;
}

void ScopedClipboardWriter::WriteSvg(const base::string16& markup) {
  RecordWrite(ClipboardFormatMetric::kSvg);
  std::string utf8_markup = base::UTF16ToUTF8(markup);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(
      Clipboard::ObjectMapParam(utf8_markup.begin(), utf8_markup.end()));
  objects_[Clipboard::PortableFormat::kSvg] = parameters;
}

void ScopedClipboardWriter::WriteRTF(const std::string& rtf_data) {
  RecordWrite(ClipboardFormatMetric::kRtf);
  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(rtf_data.begin(),
                                                 rtf_data.end()));
  objects_[Clipboard::PortableFormat::kRtf] = parameters;
}

void ScopedClipboardWriter::WriteBookmark(const base::string16& bookmark_title,
                                          const std::string& url) {
  if (bookmark_title.empty() || url.empty())
    return;
  RecordWrite(ClipboardFormatMetric::kBookmark);

  std::string utf8_markup = base::UTF16ToUTF8(bookmark_title);

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(Clipboard::ObjectMapParam(utf8_markup.begin(),
                                                 utf8_markup.end()));
  parameters.push_back(Clipboard::ObjectMapParam(url.begin(), url.end()));
  objects_[Clipboard::PortableFormat::kBookmark] = parameters;
}

void ScopedClipboardWriter::WriteHyperlink(const base::string16& anchor_text,
                                           const std::string& url) {
  if (anchor_text.empty() || url.empty())
    return;

  // Construct the hyperlink.
  std::string html = "<a href=\"";
  html += net::EscapeForHTML(url);
  html += "\">";
  html += net::EscapeForHTML(base::UTF16ToUTF8(anchor_text));
  html += "</a>";
  WriteHTML(base::UTF8ToUTF16(html), std::string());
}

void ScopedClipboardWriter::WriteWebSmartPaste() {
  RecordWrite(ClipboardFormatMetric::kWebSmartPaste);
  objects_[Clipboard::PortableFormat::kWebkit] = Clipboard::ObjectMapParams();
}

void ScopedClipboardWriter::WriteImage(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing())
    return;
  DCHECK(bitmap.getPixels());
  RecordWrite(ClipboardFormatMetric::kImage);

  // The platform code that sets this bitmap into the system clipboard expects
  // to get N32 32bpp bitmaps. If they get the wrong type and mishandle it, a
  // memcpy of the pixels can cause out-of-bounds issues.
  CHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  bitmap_ = bitmap;
  // TODO(dcheng): This is slightly less horrible than what we used to do, but
  // only very slightly less.
  SkBitmap* bitmap_pointer = &bitmap_;
  Clipboard::ObjectMapParam packed_pointer;
  packed_pointer.resize(sizeof(bitmap_pointer));
  *reinterpret_cast<SkBitmap**>(&*packed_pointer.begin()) = bitmap_pointer;
  Clipboard::ObjectMapParams parameters;
  parameters.push_back(packed_pointer);
  objects_[Clipboard::PortableFormat::kBitmap] = parameters;
}

void ScopedClipboardWriter::MarkAsConfidential() {
  confidential_ = true;
}

void ScopedClipboardWriter::WritePickledData(
    const base::Pickle& pickle,
    const ClipboardFormatType& format) {
  RecordWrite(ClipboardFormatMetric::kCustomData);
  std::string format_string = format.Serialize();
  Clipboard::ObjectMapParam format_parameter(format_string.begin(),
                                             format_string.end());
  Clipboard::ObjectMapParam data_parameter;

  data_parameter.resize(pickle.size());
  memcpy(const_cast<char*>(&data_parameter.front()),
         pickle.data(), pickle.size());

  Clipboard::ObjectMapParams parameters;
  parameters.push_back(format_parameter);
  parameters.push_back(data_parameter);
  objects_[Clipboard::PortableFormat::kData] = parameters;
}

void ScopedClipboardWriter::WriteData(const base::string16& format,
                                      mojo_base::BigBuffer data) {
  RecordWrite(ClipboardFormatMetric::kData);
  platform_representations_.push_back(
      {base::UTF16ToUTF8(format), std::move(data)});
}

void ScopedClipboardWriter::Reset() {
  objects_.clear();
  platform_representations_.clear();
  bitmap_.reset();
  confidential_ = false;
}

}  // namespace ui
