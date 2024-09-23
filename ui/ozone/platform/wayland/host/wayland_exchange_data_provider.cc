// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "net/base/mime_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace ui {

namespace {

constexpr FilenameToURLPolicy kFilenameToURLPolicy =
    FilenameToURLPolicy::CONVERT_FILENAMES;

// Returns name parameter in application/octet-stream;name=<...>, or empty
// string if parsing fails.
std::string GetApplicationOctetStreamName(const std::string& mime_type) {
  base::StringPairs params;
  if (net::MatchesMimeType(std::string(ui::kMimeTypeOctetStream), mime_type) &&
      net::ParseMimeType(mime_type, nullptr, &params)) {
    for (const auto& kv : params) {
      if (kv.first == "name") {
        return kv.second;
      }
    }
  }
  return std::string();
}

// Converts mime type string to OSExchangeData::Format, if supported, otherwise
// 0 is returned.
int MimeTypeToFormat(const std::string& mime_type) {
  if (mime_type == ui::kMimeTypeText || mime_type == ui::kMimeTypeTextUtf8)
    return OSExchangeData::STRING;
  if (mime_type == ui::kMimeTypeURIList)
    return OSExchangeData::FILE_NAME;
  if (mime_type == ui::kMimeTypeMozillaURL)
    return OSExchangeData::URL;
  if (mime_type == ui::kMimeTypeHTML || mime_type == ui::kMimeTypeHTMLUtf8) {
    return OSExchangeData::HTML;
  }
  if (!GetApplicationOctetStreamName(mime_type).empty()) {
    return OSExchangeData::FILE_CONTENTS;
  }
  if (mime_type == ui::kMimeTypeDataTransferCustomData) {
    return OSExchangeData::PICKLED_DATA;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (mime_type == ui::kMimeTypeDataTransferEndpoint)
    return OSExchangeData::DATA_TRANSFER_ENDPOINT;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return 0;
}

// Converts raw data to either narrow or wide string.
template <typename StringType>
StringType BytesTo(PlatformClipboard::Data bytes) {
  using ValueType = typename StringType::value_type;
  const size_t bytes_size = bytes->size();
  const size_t rounded_bytes_size =
      bytes_size - (bytes_size % sizeof(ValueType));
  if (bytes_size != rounded_bytes_size) {
    // This is suspicious.
    LOG(WARNING)
        << "Data is possibly truncated, or a wrong conversion is requested.";
  }

  StringType result;
  result.resize(rounded_bytes_size / sizeof(ValueType));
  base::as_writable_byte_span(result).copy_from(
      base::span(*bytes).first(rounded_bytes_size));
  return result;
}

void AddString(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->as_vector().empty()) {
    return;
  }

  provider->SetString(base::UTF8ToUTF16(BytesTo<std::string>(data)));
}

void AddHtml(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->as_vector().empty()) {
    return;
  }

  provider->SetHtml(base::UTF8ToUTF16(BytesTo<std::string>(data)), GURL());
}

// Parses |data| as if it had text/uri-list format.  Its brief spec is:
// 1.  Any lines beginning with the '#' character are comment lines.
// 2.  Non-comment lines shall be URIs (URNs or URLs).
// 3.  Lines are terminated with a CRLF pair.
// 4.  URL encoding is used.
void AddFiles(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  std::string data_as_string = BytesTo<std::string>(data);

  const auto lines = base::SplitString(
      data_as_string, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<FileInfo> filenames;
  for (const auto& line : lines) {
    if (line.empty() || line[0] == '#')
      continue;
    GURL url(line);
    if (!url.is_valid() || !url.SchemeIsFile()) {
      LOG(WARNING) << "Invalid URI found: " << line;
      continue;
    }

    url::RawCanonOutputT<char16_t> unescaped;
    url::DecodeURLEscapeSequences(
        url.path_piece(), url::DecodeURLMode::kUTF8OrIsomorphic, &unescaped);

    const base::FilePath path(base::UTF16ToUTF8(unescaped.view()));
    filenames.emplace_back(path, path.BaseName());
  }
  if (filenames.empty())
    return;

  provider->SetFilenames(filenames);
}

void AddFileContents(const std::string& filename,
                     PlatformClipboard::Data data,
                     OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (filename.empty()) {
    return;
  }

  provider->SetFileContents(base::FilePath(filename),
                            BytesTo<std::string>(data));
}

// Parses |data| as if it had text/x-moz-url format, which is basically
// two lines separated with newline, where the first line is the URL and
// the second one is page title.  The unpleasant feature of text/x-moz-url is
// that the URL has UTF-16 encoding.
void AddUrl(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->as_vector().empty()) {
    return;
  }

  std::u16string data_as_string16 = BytesTo<std::u16string>(data);

  const auto lines =
      base::SplitString(data_as_string16, u"\r\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (lines.size() != 2U) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; it must contain "
                 << "exactly 2 lines but has " << lines.size() << " instead.";
    return;
  }
  GURL url(lines[0]);
  if (!url.is_valid()) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; the first line "
                 << "must contain a valid URL but it doesn't.";
    return;
  }

  provider->SetURL(url, lines[1]);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Parses |data| as if it was an encoded custom mime type DataTransferEndpoint.
// Used to synchronize the drag source metadata between Ash and Lacros.
void AddSource(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->as_vector().empty()) {
    return;
  }

  std::string source_dte = BytesTo<std::string>(data);
  provider->SetSource(ConvertJsonToDataTransferEndpoint(source_dte));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

WaylandExchangeDataProvider::WaylandExchangeDataProvider() = default;

WaylandExchangeDataProvider::~WaylandExchangeDataProvider() = default;

std::unique_ptr<OSExchangeDataProvider> WaylandExchangeDataProvider::Clone()
    const {
  auto clone = std::make_unique<WaylandExchangeDataProvider>();
  CopyData(clone.get());
  return clone;
}

std::vector<std::string> WaylandExchangeDataProvider::BuildMimeTypesList()
    const {
  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland
  // specs don't say anything like that, but here we follow that common
  // practice: begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (HasFile())
    mime_types.push_back(ui::kMimeTypeURIList);

  if (HasURL(kFilenameToURLPolicy))
    mime_types.push_back(ui::kMimeTypeMozillaURL);

  if (HasHtml()) {
    mime_types.push_back(ui::kMimeTypeHTML);
  }

  if (HasString()) {
    mime_types.push_back(ui::kMimeTypeTextUtf8);
    mime_types.push_back(ui::kMimeTypeText);
  }

  if (HasFileContents()) {
    std::optional<FileContentsInfo> file_contents = GetFileContents();

    std::string filename = file_contents->filename.value();
    base::ReplaceChars(filename, "\\", "\\\\", &filename);
    base::ReplaceChars(filename, "\"", "\\\"", &filename);
    const std::string mime_type =
        base::StrCat({ui::kMimeTypeOctetStream, ";name=\"", filename, "\""});
    mime_types.push_back(mime_type);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (GetSource() != nullptr) {
    mime_types.push_back(ui::kMimeTypeDataTransferEndpoint);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  for (auto item : pickle_data())
    mime_types.push_back(item.first.GetName());

  return mime_types;
}

// TODO(crbug.com/40192823): Support custom formats/pickled data.
void WaylandExchangeDataProvider::AddData(PlatformClipboard::Data data,
                                          const std::string& mime_type) {
  DCHECK(data);
  DCHECK(IsMimeTypeSupported(mime_type));
  int format = MimeTypeToFormat(mime_type);
  switch (format) {
    case OSExchangeData::STRING:
      AddString(data, this);
      break;
    case OSExchangeData::HTML:
      AddHtml(data, this);
      break;
    case OSExchangeData::URL:
      AddUrl(data, this);
      break;
    case OSExchangeData::FILE_NAME:
      AddFiles(data, this);
      break;
    case OSExchangeData::FILE_CONTENTS:
      AddFileContents(GetApplicationOctetStreamName(mime_type), data, this);
      break;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    case OSExchangeData::DATA_TRANSFER_ENDPOINT:
      AddSource(data, this);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
}

// TODO(crbug.com/40192823): Support custom formats/pickled data.
bool WaylandExchangeDataProvider::ExtractData(const std::string& mime_type,
                                              std::string* out_content) const {
  DCHECK(out_content);
  DCHECK(IsMimeTypeSupported(mime_type));
  if (std::optional<ui::OSExchangeData::UrlInfo> url_info;
      mime_type == ui::kMimeTypeMozillaURL &&
      (url_info = GetURLAndTitle(kFilenameToURLPolicy)).has_value()) {
    out_content->append(url_info->url.spec());
    return true;
  }
  if ((mime_type == ui::kMimeTypeHTML || mime_type == ui::kMimeTypeHTMLUtf8) &&
      HasHtml()) {
    const std::optional<ui::OSExchangeData::HtmlInfo>& html_content = GetHtml();
    out_content->append(base::UTF16ToUTF8(html_content->html));
    return true;
  }
  if (base::StartsWith(mime_type, ui::kMimeTypeOctetStream) &&
      HasFileContents()) {
    std::optional<FileContentsInfo> file_contents = GetFileContents();
    out_content->append(file_contents->file_contents);
    return true;
  }
  if (mime_type == ui::kMimeTypeDataTransferCustomData &&
      HasCustomFormat(ui::ClipboardFormatType::DataTransferCustomType())) {
    std::optional<base::Pickle> pickle =
        GetPickledData(ui::ClipboardFormatType::DataTransferCustomType());
    *out_content = std::string(reinterpret_cast<const char*>(pickle->data()),
                               pickle->size());
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (mime_type == ui::kMimeTypeDataTransferEndpoint &&
      GetSource() != nullptr) {
    DataTransferEndpoint* data_src = GetSource();
    out_content->append(ConvertDataTransferEndpointToJson(*data_src));
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lastly, attempt to extract string data. Note: Keep this as the last
  // condition otherwise, for data maps that contain both string and custom
  // data, for example, it may result in subtle issues, such as,
  // https://crbug.com/1271311.
  if (std::optional<std::u16string> data = GetString(); data.has_value()) {
    out_content->append(base::UTF16ToUTF8(*data));
    return true;
  }
  return false;
}

bool IsMimeTypeSupported(const std::string& mime_type) {
  return MimeTypeToFormat(mime_type) != 0;
}

}  // namespace ui
