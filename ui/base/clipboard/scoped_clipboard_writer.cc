// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/scoped_clipboard_writer.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/pickle.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
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
    Clipboard::Data data = Clipboard::WebCustomFormatMapData{
        .data = base::WriteJson(registered_formats_value).value_or(""),
    };
    const size_t index = data.index();
    objects_[index] = Clipboard::ObjectMapParams(std::move(data));
  }

  if (main_frame_url_.is_valid() || frame_url_.is_valid()) {
    auto text_iter = objects_.find(
        base::VariantIndexOfType<Clipboard::Data, Clipboard::TextData>());
    if (text_iter != objects_.end()) {
      const auto& text_data =
          std::get<Clipboard::TextData>(text_iter->second.data);
      Clipboard::GetForCurrentThread()->NotifyCopyWithUrl(
          text_data.data, frame_url_, main_frame_url_);
    }
  }

  if (!objects_.empty() || !raw_objects_.empty() ||
      !platform_representations_.empty()) {
    std::vector<Clipboard::RawData> raw_objects;
    raw_objects.reserve(raw_objects_.size());
    for (auto& raw_object : raw_objects_) {
      raw_objects.emplace_back(std::move(raw_object.second));
    }

    Clipboard::GetForCurrentThread()->WritePortableAndPlatformRepresentations(
        buffer_, objects_, std::move(raw_objects),
        std::move(platform_representations_), std::move(data_src_),
        privacy_types_);
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

void ScopedClipboardWriter::WriteText(std::u16string_view text) {
  RecordWrite(ClipboardFormatMetric::kText);

  Clipboard::Data data = Clipboard::TextData{.data = base::UTF16ToUTF8(text)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteHTML(std::u16string_view markup,
                                      std::string source_url) {
  RecordWrite(ClipboardFormatMetric::kHtml);

  Clipboard::HtmlData html_data;
  html_data.markup = base::UTF16ToUTF8(markup);
  if (!source_url.empty()) {
    html_data.source_url = std::move(source_url);
  }
  Clipboard::Data data(std::move(html_data));
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteSvg(std::u16string_view markup) {
  RecordWrite(ClipboardFormatMetric::kSvg);

  Clipboard::Data data =
      Clipboard::SvgData{.markup = base::UTF16ToUTF8(markup)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteRTF(std::string rtf_data) {
  RecordWrite(ClipboardFormatMetric::kRtf);

  Clipboard::Data data = Clipboard::RtfData{.data = std::move(rtf_data)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteFilenames(std::string uri_list) {
  RecordWrite(ClipboardFormatMetric::kFilenames);
  Clipboard::Data data =
      Clipboard::FilenamesData{.text_uri_list = std::move(uri_list)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteBookmark(std::u16string_view bookmark_title,
                                          std::string url) {
  if (ui::clipboard_util::ShouldSkipBookmark(bookmark_title, url)) {
    return;
  }
  RecordWrite(ClipboardFormatMetric::kBookmark);

  Clipboard::Data data = Clipboard::BookmarkData{
      .title = base::UTF16ToUTF8(bookmark_title), .url = std::move(url)};
  const size_t index = data.index();
  objects_[index] = Clipboard::ObjectMapParams(std::move(data));
}

void ScopedClipboardWriter::WriteHyperlink(std::u16string_view anchor_text,
                                           std::string_view url) {
  if (anchor_text.empty() || url.empty())
    return;

  // Construct the hyperlink.
  const std::string tag =
      base::StrCat({"<a href=\"", base::EscapeForHTML(url), "\">"});
  WriteHTML(base::StrCat({base::UTF8ToUTF16(tag),
                          base::EscapeForHTML(anchor_text), u"</a>"}),
            std::string());
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
  privacy_types_ |= Clipboard::PrivacyTypes::kNoLocalClipboardHistory;
  privacy_types_ |= Clipboard::PrivacyTypes::kNoCloudClipboard;
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
      UNSAFE_TODO(reinterpret_cast<const uint8_t*>(pickle.data()) +
                  pickle.size()));
  raw_objects_.insert({format, std::move(raw_data)});
}

void ScopedClipboardWriter::WriteRawDataForTest(
    const ClipboardFormatType& format,
    std::vector<uint8_t> data) {
  RecordWrite(ClipboardFormatMetric::kCustomData);
  Clipboard::RawData raw_data;
  raw_data.format = format;
  raw_data.data = std::move(data);
  raw_objects_.insert({format, std::move(raw_data)});
}

void ScopedClipboardWriter::WriteData(std::u16string_view format,
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

void ScopedClipboardWriter::Reset() {
  objects_.clear();
  raw_objects_.clear();
  platform_representations_.clear();
  registered_formats_.clear();
  privacy_types_ = 0;
  counter_ = 0;
}

}  // namespace ui
