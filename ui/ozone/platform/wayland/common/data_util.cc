// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/common/data_util.h"

#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace wl {

namespace {

using ui::OSExchangeData;
using ui::PlatformClipboard;

constexpr ui::FilenameToURLPolicy kFilenameToURLPolicy =
    ui::FilenameToURLPolicy::CONVERT_FILENAMES;

// Converts mime type string to OSExchangeData::Format, if supported, otherwise
// 0 is returned.
int MimeTypeToFormat(const std::string& mime_type) {
  if (mime_type == ui::kMimeTypeText || mime_type == ui::kMimeTypeTextUtf8)
    return OSExchangeData::STRING;
  if (mime_type == ui::kMimeTypeURIList)
    return OSExchangeData::FILE_NAME;
  if (mime_type == ui::kMimeTypeMozillaURL)
    return OSExchangeData::URL;
  if (mime_type == ui::kMimeTypeHTML)
    return OSExchangeData::HTML;
  return 0;
}

// Converts raw data to either narrow or wide string.
template <typename StringType>
StringType BytesTo(PlatformClipboard::Data bytes) {
  using ValueType = typename StringType::value_type;
  if (bytes->size() % sizeof(ValueType) != 0U) {
    // This is suspicious.
    LOG(WARNING)
        << "Data is possibly truncated, or a wrong conversion is requested.";
  }

  StringType result(bytes->front_as<ValueType>(),
                    bytes->size() / sizeof(ValueType));
  return result;
}

void AddString(PlatformClipboard::Data data, OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data->data().empty())
    return;

  os_exchange_data->SetString(base::UTF8ToUTF16(BytesTo<std::string>(data)));
}

void AddHtml(PlatformClipboard::Data data, OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data->data().empty())
    return;

  os_exchange_data->SetHtml(base::UTF8ToUTF16(BytesTo<std::string>(data)),
                            GURL());
}

// Parses |data| as if it had text/uri-list format.  Its brief spec is:
// 1.  Any lines beginning with the '#' character are comment lines.
// 2.  Non-comment lines shall be URIs (URNs or URLs).
// 3.  Lines are terminated with a CRLF pair.
// 4.  URL encoding is used.
void AddFiles(PlatformClipboard::Data data, OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  std::string data_as_string = BytesTo<std::string>(data);

  const auto lines = base::SplitString(
      data_as_string, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<ui::FileInfo> filenames;
  for (const auto& line : lines) {
    if (line.empty() || line[0] == '#')
      continue;
    GURL url(line);
    if (!url.is_valid() || !url.SchemeIsFile()) {
      LOG(WARNING) << "Invalid URI found: " << line;
      continue;
    }

    std::string url_path = url.path();
    url::RawCanonOutputT<base::char16> unescaped;
    url::DecodeURLEscapeSequences(url_path.data(), url_path.size(),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &unescaped);

    std::string path8;
    base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &path8);
    const base::FilePath path(path8);
    filenames.push_back({path, path.BaseName()});
  }
  if (filenames.empty())
    return;

  os_exchange_data->SetFilenames(filenames);
}

// Parses |data| as if it had text/x-moz-url format, which is basically
// two lines separated with newline, where the first line is the URL and
// the second one is page title.  The unpleasant feature of text/x-moz-url is
// that the URL has UTF-16 encoding.
void AddUrl(PlatformClipboard::Data data, OSExchangeData* os_exchange_data) {
  DCHECK(os_exchange_data);

  if (data->data().empty())
    return;

  base::string16 data_as_string16 = BytesTo<base::string16>(data);

  const auto lines =
      base::SplitString(data_as_string16, base::ASCIIToUTF16("\r\n"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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

  os_exchange_data->SetURL(url, lines[1]);
}

}  // namespace

bool IsMimeTypeSupported(const std::string& mime_type) {
  return MimeTypeToFormat(mime_type) != 0;
}

bool ContainsMimeType(const OSExchangeData& exchange_data,
                      const std::string& mime_type) {
  DCHECK(IsMimeTypeSupported(mime_type));
  return exchange_data.HasAnyFormat(MimeTypeToFormat(mime_type), {});
}

void AddToOSExchangeData(PlatformClipboard::Data data,
                         const std::string& mime_type,
                         OSExchangeData* exchange_data) {
  DCHECK(data);
  DCHECK(IsMimeTypeSupported(mime_type));
  DCHECK(exchange_data);
  int format = MimeTypeToFormat(mime_type);
  switch (format) {
    case OSExchangeData::STRING:
      AddString(data, exchange_data);
      break;
    case OSExchangeData::HTML:
      AddHtml(data, exchange_data);
      break;
    case OSExchangeData::URL:
      AddUrl(data, exchange_data);
      break;
    case OSExchangeData::FILE_NAME:
      AddFiles(data, exchange_data);
      break;
  }
}

bool ExtractOSExchangeData(const OSExchangeData& exchange_data,
                           const std::string& mime_type,
                           std::string* out_content) {
  DCHECK(out_content);
  DCHECK(IsMimeTypeSupported(mime_type));

  if (mime_type == ui::kMimeTypeMozillaURL &&
      exchange_data.HasURL(kFilenameToURLPolicy)) {
    GURL url;
    base::string16 title;
    exchange_data.GetURLAndTitle(kFilenameToURLPolicy, &url, &title);
    out_content->append(url.spec());
    return true;
  }
  if (mime_type == ui::kMimeTypeHTML && exchange_data.HasHtml()) {
    base::string16 data;
    GURL base_url;
    exchange_data.GetHtml(&data, &base_url);
    out_content->append(base::UTF16ToUTF8(data));
    return true;
  }
  if (exchange_data.HasString()) {
    base::string16 data;
    exchange_data.GetString(&data);
    out_content->append(base::UTF16ToUTF8(data));
    return true;
  }
  return false;
}

}  // namespace wl
