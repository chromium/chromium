// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

namespace ui {

OSExchangeDataProviderNonBacked::OSExchangeDataProviderNonBacked() = default;

OSExchangeDataProviderNonBacked::~OSExchangeDataProviderNonBacked() = default;

std::unique_ptr<OSExchangeDataProvider> OSExchangeDataProviderNonBacked::Clone()
    const {
  auto clone = std::make_unique<OSExchangeDataProviderNonBacked>();
  CopyData(clone.get());
  return clone;
}

void OSExchangeDataProviderNonBacked::MarkRendererTaintedFromOrigin(
    const url::Origin& origin) {
  tainted_by_renderer_origin_ = origin;
}

bool OSExchangeDataProviderNonBacked::IsRendererTainted() const {
  return tainted_by_renderer_origin_.has_value();
}

std::optional<url::Origin>
OSExchangeDataProviderNonBacked::GetRendererTaintedOrigin() const {
  // Platform-specific implementations of OSExchangeDataProvider do not
  // roundtrip opaque origins, so match that behavior here.
  if (tainted_by_renderer_origin_ && tainted_by_renderer_origin_->opaque()) {
    return url::Origin();
  }
  return tainted_by_renderer_origin_;
}

void OSExchangeDataProviderNonBacked::MarkAsFromPrivileged() {
  is_from_privileged_ = true;
}

bool OSExchangeDataProviderNonBacked::IsFromPrivileged() const {
  return is_from_privileged_;
}

void OSExchangeDataProviderNonBacked::SetString(std::u16string_view data) {
  if (HasString())
    return;

  string_ = std::u16string(data);
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderNonBacked::SetURLs(
    base::span<const ClipboardUrlInfo> url_infos) {
  if (url_infos.empty()) {
    return;
  }
  const auto& url_info = url_infos.front();
  url_ = url_info.url;
  title_ = url_info.title;
  formats_ |= OSExchangeData::URL;

  SetString(base::UTF8ToUTF16(url_.spec()));
}

void OSExchangeDataProviderNonBacked::SetFilename(const base::FilePath& path) {
  filenames_.clear();
  filenames_.push_back(FileInfo(path, base::FilePath()));
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderNonBacked::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  filenames_ = filenames;
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderNonBacked::SetPickledData(
    const ClipboardFormatType& format,
    const base::Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

std::optional<std::u16string> OSExchangeDataProviderNonBacked::GetString()
    const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (HasFile()) {
    // Various Linux file managers both pass a list of file:// URIs and set the
    // string representation to the URI. We explicitly don't want to return use
    // this representation.
    return std::nullopt;
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if ((formats_ & OSExchangeData::STRING) == 0)
    return std::nullopt;
  return string_;
}

std::vector<ClipboardUrlInfo> OSExchangeDataProviderNonBacked::GetURLs(
    FilenameToURLPolicy policy) const {
  std::vector<ClipboardUrlInfo> url_infos;
  if ((formats_ & OSExchangeData::URL) == 0) {
    if (std::optional<GURL> plaintext_url = GetPlainTextURL();
        plaintext_url.has_value()) {
      DCHECK(plaintext_url->is_valid());
      url_infos.push_back(
          ClipboardUrlInfo{plaintext_url.value(), std::u16string()});
    }
  } else {
    if (url_.is_valid()) {
      url_infos.push_back(ClipboardUrlInfo{url_, title_});
    }
  }

  if (policy == FilenameToURLPolicy::CONVERT_FILENAMES) {
    if (std::optional<std::vector<FileInfo>> fileinfos = GetFilenames();
        fileinfos.has_value()) {
      for (const auto& fileinfo : fileinfos.value()) {
        url_infos.push_back(
            ClipboardUrlInfo{net::FilePathToFileURL(fileinfo.path), u""});
      }
    }
  }

  return url_infos;
}

std::optional<std::vector<FileInfo>>
OSExchangeDataProviderNonBacked::GetFilenames() const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return std::nullopt;

  return filenames_;
}

std::optional<base::Pickle> OSExchangeDataProviderNonBacked::GetPickledData(
    const ClipboardFormatType& format) const {
  const auto i = pickle_data_.find(format);
  if (i == pickle_data_.end()) {
    return std::nullopt;
  }

  return i->second;
}

bool OSExchangeDataProviderNonBacked::HasString() const {
  return (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderNonBacked::HasURL(FilenameToURLPolicy policy) const {
  if ((formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL().has_value() ||
         (policy == FilenameToURLPolicy::CONVERT_FILENAMES &&
          GetFileURL(nullptr));
}

bool OSExchangeDataProviderNonBacked::HasFile() const {
  return (formats_ & OSExchangeData::FILE_NAME) != 0;
}

bool OSExchangeDataProviderNonBacked::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return pickle_data_.contains(format);
}

void OSExchangeDataProviderNonBacked::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  file_contents_filename_ = filename;
  file_contents_ = file_contents;
}

std::optional<OSExchangeDataProvider::FileContentsInfo>
OSExchangeDataProviderNonBacked::GetFileContents() const {
  if (file_contents_filename_.empty()) {
    return std::nullopt;
  }
  return FileContentsInfo{.filename = file_contents_filename_,
                          .file_contents = file_contents_};
}

bool OSExchangeDataProviderNonBacked::HasFileContents() const {
  return !file_contents_filename_.empty();
}

void OSExchangeDataProviderNonBacked::SetHtml(const std::u16string& html,
                                              const GURL& base_url) {
  formats_ |= OSExchangeData::HTML;
  html_ = html;
  base_url_ = base_url;
}

std::optional<OSExchangeData::HtmlInfo>
OSExchangeDataProviderNonBacked::GetHtml() const {
  if (!HasHtml()) {
    return std::nullopt;
  }

  return HtmlInfo{
      .html = html_,
      .base_url = base_url_,
  };
}

bool OSExchangeDataProviderNonBacked::HasHtml() const {
  return ((formats_ & OSExchangeData::HTML) != 0);
}

void OSExchangeDataProviderNonBacked::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderNonBacked::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderNonBacked::GetDragImageOffset() const {
  return drag_image_offset_;
}

bool OSExchangeDataProviderNonBacked::GetFileURL(GURL* url) const {
  if (!HasFile()) {
    return false;
  }

  base::FilePath file_path = filenames_[0].path;
  GURL test_url = net::FilePathToFileURL(file_path);
  if (!test_url.is_valid()) {
    return false;
  }
  if (url) {
    *url = std::move(test_url);
  }
  return true;
}

std::optional<GURL> OSExchangeDataProviderNonBacked::GetPlainTextURL() const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return std::nullopt;

  GURL test_url(string_);
  if (!test_url.is_valid()) {
    return std::nullopt;
  }

  if (base::FeatureList::IsEnabled(
          features::kDragDropOnlySynthesizeHttpOrHttpsUrlsFromText) &&
      IsRendererTainted() && !test_url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  return test_url;
}

void OSExchangeDataProviderNonBacked::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {
  source_ = std::move(data_source);
}

DataTransferEndpoint* OSExchangeDataProviderNonBacked::GetSource() const {
  return source_.get();
}

void OSExchangeDataProviderNonBacked::CopyData(
    OSExchangeDataProviderNonBacked* provider) const {
  DCHECK(provider);
  provider->formats_ = formats_;
  provider->string_ = string_;
  provider->url_ = url_;
  provider->title_ = title_;
  provider->filenames_ = filenames_;
  provider->pickle_data_ = pickle_data_;
  provider->file_contents_filename_ = file_contents_filename_;
  provider->file_contents_ = file_contents_;
  provider->html_ = html_;
  provider->base_url_ = base_url_;
  provider->source_ =
      source_ ? std::make_unique<DataTransferEndpoint>(*source_.get())
              : nullptr;
  provider->tainted_by_renderer_origin_ = tainted_by_renderer_origin_;
  provider->is_from_privileged_ = is_from_privileged_;
}

}  // namespace ui
