// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/clipboard/scoped_clipboard_writer.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/pickle.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/variant_util.h"
#include "base/values.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_util.h"
#include "ui/gfx/geometry/size.h"

// Documentation on the format of the parameters for each clipboard target can
// be found in clipboard.h.
namespace ui {

ScopedClipboardWriter::ScopedClipboardWriter(
    ClipboardBuffer buffer,
    std::unique_ptr<DataTransferEndpoint> data_src)
    : buffer_(buffer), data_src_(std::move(data_src)) {}

ScopedClipboardWriter::~ScopedClipboardWriter() {
  static constexpr size_t kMaxRepresentations = 1 << 12;
  DCHECK(platform_representations_.size() < kMaxRepresentations);
  // If the metadata format type is not empty then create a JSON payload and
  // write to the clipboard.
  if (!registered_formats_.empty()) {
    base::Value::Dict registered_formats_value;
    for (const auto& item : registered_formats_)
      registered_formats_value.Set(item.first, item.second);
    std::string custom_format_json;
    base::JSONWriter::Write(registered_formats_value, &custom_format_json);
    Clipboard::Data data = Clipboard::WebCustomFormatMapData{
        .data = std::move(custom_format_json),
    };
    const size_t index = data.index();
    objects_[index] = Clipboard::ObjectMapParams(std::move(data));
  }

  if (main_frame_url_.is_valid() || frame_url_.is_valid()) {
    auto text_iter = objects_.find(
        base::VariantIndexOfType<Clipboard::Data, Clipboard::TextData>());
    if (text_iter != objects_.end()) {
      const auto& text_data =
          absl::get<Clipboard::TextData>(text_iter->second.data);
      Clipboard::GetForCurrentThread()->NotifyCopyWithUrl(
          text_data.data, frame_url_, main_frame_url_);
    }
  }

  if (!objects_.empty() || !platform_representations_.empty()) {
    Clipboard::GetForCurrentThread()->WritePortableAndPlatformRepresentations(
        buffer_, objects_, std::move(platform_representations_),
        std::move(data_src_), privacy_types_);
  }
}

void ScopedClipboardWriter::SetDataSource(
    std::unique_ptr<DataTransferEndpoint> data_src) {
  data_src_ = std::move(data_src);
}

void ScopedClipboardWriter::SetDataSourceURL(const GURL& main_frame,
                                             const GURL& frame_url) {
  main_frame_url_ = main_frame;
  frame_url_ = frame_url;
}

void ScopedClipboardWriter::WriteText(const std::u16string& text) {
  RecordWrite(ClipboardFormatMetric::kText);

  Clipboard::Data data = Clipboard::TextData{.data = base::UTF16ToUTF8(text)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteHTML(const std::u16string& markup,
                                      const std::string& source_url) {
  RecordWrite(ClipboardFormatMetric::kHtml);

  Clipboard::HtmlData html_data;
  html_data.markup = base::UTF16ToUTF8(markup);
  if (!source_url.empty()) {
    html_data.source_url = source_url;
  }
  Clipboard::Data data(std::move(html_data));
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteSvg(const std::u16string& markup) {
  RecordWrite(ClipboardFormatMetric::kSvg);

  Clipboard::Data data =
      Clipboard::SvgData{.markup = base::UTF16ToUTF8(markup)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteRTF(const std::string& rtf_data) {
  RecordWrite(ClipboardFormatMetric::kRtf);

  Clipboard::Data data = Clipboard::RtfData{.data = rtf_data};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteFilenames(const std::string& uri_list) {
  RecordWrite(ClipboardFormatMetric::kFilenames);
  Clipboard::Data data = Clipboard::FilenamesData{.text_uri_list = uri_list};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteBookmark(const std::u16string& bookmark_title,
                                          const std::string& url) {
  if (ui::clipboard_util::ShouldSkipBookmark(bookmark_title, url)) {
    return;
  }
  RecordWrite(ClipboardFormatMetric::kBookmark);

  Clipboard::Data data = Clipboard::BookmarkData{
      .title = base::UTF16ToUTF8(bookmark_title), .url = url};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteHyperlink(const std::u16string& anchor_text,
                                           const std::string& url) {
  if (anchor_text.empty() || url.empty())
    return;

  // Construct the hyperlink.
  std::string html = "<a href=\"";
  html += base::EscapeForHTML(url);
  html += "\">";
  html += base::EscapeForHTML(base::UTF16ToUTF8(anchor_text));
  html += "</a>";
  WriteHTML(base::UTF8ToUTF16(html), std::string());
}

void ScopedClipboardWriter::WriteWebSmartPaste() {
  RecordWrite(ClipboardFormatMetric::kWebSmartPaste);
  Clipboard::Data data = Clipboard::WebkitData();
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
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

  Clipboard::Data data = Clipboard::BitmapData{.bitmap = bitmap};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::MarkAsConfidential() {
  privacy_types_ |= Clipboard::PrivacyTypes::kNoDisplay;
}

void ScopedClipboardWriter::MarkAsOffTheRecord() {
  privacy_types_ |= Clipboard::PrivacyTypes::kNoLocalClipboardHistory;
  privacy_types_ |= Clipboard::PrivacyTypes::kNoCloudClipboard;
}

void ScopedClipboardWriter::WritePickledData(
    const base::Pickle& pickle,
    const ClipboardFormatType& format) {
  RecordWrite(ClipboardFormatMetric::kCustomData);
  Clipboard::RawData raw_data;
  raw_data.format = format;
  raw_data.data = std::vector<uint8_t>(
      reinterpret_cast<const uint8_t*>(pickle.data()),
      reinterpret_cast<const uint8_t*>(pickle.data()) + pickle.size());
  Clipboard::Data data = std::move(raw_data);
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteData(const std::u16string& format,
                                      mojo_base::BigBuffer data) {
  RecordWrite(ClipboardFormatMetric::kData);
  // Windows / X11 clipboards enter an unrecoverable state after registering
  // some amount of unique formats, and there's no way to un-register these
  // formats. For these clipboards, use a conservative limit to avoid
  // registering too many formats, as:
  // (1) Other native applications may also register clipboard formats.
  // (2) Malicious sites can write more than the hard limit defined on
  // Windows(16k). (3) Chrome also registers other clipboard formats.
  //
  // There will be a custom format map which contains a JSON payload that will
  // have a mapping of custom format MIME type to web custom format.
  // There can only be 100 custom format per write and it will be
  // registered when the web authors request for a custom format.
  if (counter_ >= ui::kMaxRegisteredClipboardFormats)
    return;
  std::string format_in_ascii = base::UTF16ToASCII(format);
  if (registered_formats_.find(format_in_ascii) == registered_formats_.end()) {
    std::string web_custom_format_string =
        ClipboardFormatType::WebCustomFormatName(counter_);
    registered_formats_[format_in_ascii] = web_custom_format_string;
    counter_++;
    platform_representations_.push_back(
        {web_custom_format_string, std::move(data)});
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ScopedClipboardWriter::WriteEncodedDataTransferEndpointForTesting(
    const std::string& json) {
  Clipboard::Data data =
      Clipboard::EncodedDataTransferEndpointData{.data = json};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ScopedClipboardWriter::Reset() {
  objects_.clear();
  platform_representations_.clear();
  registered_formats_.clear();
  privacy_types_ = 0;
  counter_ = 0;
}

}  // namespace ui
