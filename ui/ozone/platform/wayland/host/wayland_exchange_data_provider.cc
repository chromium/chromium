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
#include "base/strings/string_view_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_util_linux.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

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
  if (mime_type == ui::kMimeTypePlainText ||
      mime_type == ui::kMimeTypeUtf8PlainText) {
    return OSExchangeData::STRING;
  }
  if (mime_type == ui::kMimeTypeUriList) {
    return OSExchangeData::FILE_NAME;
  }
  if (mime_type == ui::kMimeTypeMozillaUrl) {
    return OSExchangeData::URL;
  }
  if (mime_type == ui::kMimeTypeHtml || mime_type == ui::kMimeTypeUtf8Html) {
    return OSExchangeData::HTML;
  }
  if (!GetApplicationOctetStreamName(mime_type).empty()) {
    return OSExchangeData::FILE_CONTENTS;
  }
  if (mime_type == ui::kMimeTypeDataTransferCustomData) {
    return OSExchangeData::PICKLED_DATA;
  }
#if BUILDFLAG(IS_LINUX)
  if (mime_type == ui::kMimeTypePortalFileTransfer ||
      mime_type == ui::kMimeTypePortalFiles) {
    return OSExchangeData::PICKLED_DATA;
  }
#endif
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

    const base::FilePath path(url::DecodeUrlEscapeSequences(
        url.path(), url::DecodeUrlMode::kUtf8OrIsomorphic));
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

  ClipboardUrlInfo url_info(url, lines[1]);
  provider->SetURLs(base::span_from_ref(url_info));
}

}  // namespace

WaylandExchangeDataProvider::WaylandExchangeDataProvider() = default;

WaylandExchangeDataProvider::~WaylandExchangeDataProvider() = default;

std::unique_ptr<OSExchangeDataProvider> WaylandExchangeDataProvider::Clone()
    const {
  auto clone = std::make_unique<WaylandExchangeDataProvider>();
  CopyData(clone.get());
#if BUILDFLAG(IS_LINUX)
  clone->additional_data_ = additional_data_;
#endif
  return clone;
}

void WaylandExchangeDataProvider::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  OSExchangeDataProviderNonBacked::SetFilenames(filenames);

#if BUILDFLAG(IS_LINUX)
  // Synchronously register files to get the key. This blocks the UI thread
  // briefly but ensures the key is ready for the data offer.
  std::string key = ui::clipboard_util::RegisterFilesWithPortal(filenames);
  if (!key.empty()) {
    additional_data_[kMimeTypePortalFileTransfer] = key;
    additional_data_[kMimeTypePortalFiles] = key;
  }
#endif
}

std::vector<std::string> WaylandExchangeDataProvider::BuildMimeTypesList()
    const {
  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland
  // specs don't say anything like that, but here we follow that common
  // practice: begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (HasFile())
    mime_types.push_back(ui::kMimeTypeUriList);

  if (HasURL(kFilenameToURLPolicy))
    mime_types.push_back(ui::kMimeTypeMozillaUrl);

  if (HasHtml()) {
    mime_types.push_back(ui::kMimeTypeHtml);
  }

  if (HasString()) {
    mime_types.push_back(ui::kMimeTypeUtf8PlainText);
    mime_types.push_back(ui::kMimeTypePlainText);
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

#if BUILDFLAG(IS_LINUX)
  for (const auto& item : additional_data_) {
    mime_types.push_back(item.first);
  }
#endif

  for (auto item : pickle_data())
    mime_types.push_back(item.first.GetName());

  return mime_types;
}

// TODO(crbug.com/40192823): Support custom formats/pickled data.
void WaylandExchangeDataProvider::AddData(PlatformClipboard::Data data,
                                          const std::string& mime_type) {
  DCHECK(data);
  DCHECK(IsMimeTypeSupported(mime_type));

#if BUILDFLAG(IS_LINUX)
  if (mime_type == ui::kMimeTypePortalFileTransfer ||
      mime_type == ui::kMimeTypePortalFiles) {
    additional_data_[mime_type] = base::as_string_view(*data);
    return;
  }
#endif

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
  }
}

// TODO(crbug.com/40192823): Support custom formats/pickled data.
bool WaylandExchangeDataProvider::ExtractData(const std::string& mime_type,
                                              std::string* out_content) const {
  DCHECK(out_content);
  DCHECK(IsMimeTypeSupported(mime_type));

  if (mime_type == ui::kMimeTypeMozillaUrl ||
      mime_type == ui::kMimeTypeUriList) {
    const std::vector<ClipboardUrlInfo> url_infos =
        GetURLs(kFilenameToURLPolicy);
    if (url_infos.empty()) {
      return false;
    }
    // Mozilla format: URL\nTitle\n for each entry
    for (const auto& url_info : url_infos) {
      if (!out_content->empty()) {
        out_content->append("\n");
      }
      out_content->append(url_info.url.spec());
      if (url_info.title.empty()) {
        continue;
      }
      out_content->append("\n");
      out_content->append(base::UTF16ToUTF8(url_info.title));
    }
    return true;
  }
  if ((mime_type == ui::kMimeTypeHtml || mime_type == ui::kMimeTypeUtf8Html) &&
      HasHtml()) {
    const std::optional<ui::OSExchangeData::HtmlInfo>& html_content = GetHtml();
    out_content->append(base::UTF16ToUTF8(html_content->html));
    return true;
  }
  if (mime_type.starts_with(ui::kMimeTypeOctetStream) && HasFileContents()) {
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
#if BUILDFLAG(IS_LINUX)
  auto it = additional_data_.find(mime_type);
  if (it != additional_data_.end()) {
    *out_content = it->second;
    return true;
  }
#endif
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
