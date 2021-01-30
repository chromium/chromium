// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "url/gurl.h"

namespace ui {

OSExchangeDataProviderNonBacked::OSExchangeDataProviderNonBacked() = default;

OSExchangeDataProviderNonBacked::~OSExchangeDataProviderNonBacked() = default;

std::unique_ptr<OSExchangeDataProvider> OSExchangeDataProviderNonBacked::Clone()
    const {
  auto clone = std::make_unique<OSExchangeDataProviderNonBacked>();

  clone->formats_ = formats_;
  clone->string_ = string_;
  clone->url_ = url_;
  clone->title_ = title_;
  clone->filenames_ = filenames_;
  clone->pickle_data_ = pickle_data_;
  // We skip copying the drag images.
  clone->html_ = html_;
  clone->base_url_ = base_url_;
  clone->source_ =
      source_ ? std::make_unique<ui::DataTransferEndpoint>(*source_.get())
              : nullptr;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  clone->originated_from_renderer_ = originated_from_renderer_;
#endif

  return clone;
}

void OSExchangeDataProviderNonBacked::MarkOriginatedFromRenderer() {
  // TODO(dcheng): Currently unneeded because ChromeOS Aura correctly separates
  // URL and filename metadata, and does not implement the DownloadURL protocol.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  originated_from_renderer_ = true;
#endif
}

bool OSExchangeDataProviderNonBacked::DidOriginateFromRenderer() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return originated_from_renderer_;
#endif
}

void OSExchangeDataProviderNonBacked::SetString(const base::string16& data) {
  if (HasString())
    return;

  string_ = data;
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderNonBacked::SetURL(const GURL& url,
                                             const base::string16& title) {
  url_ = url;
  title_ = title;
  formats_ |= OSExchangeData::URL;

  SetString(base::UTF8ToUTF16(url.spec()));
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

bool OSExchangeDataProviderNonBacked::GetString(base::string16* data) const {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  if (HasFile()) {
    // Various Linux file managers both pass a list of file:// URIs and set the
    // string representation to the URI. We explicitly don't want to return use
    // this representation.
    return false;
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;
  *data = string_;
  return true;
}

bool OSExchangeDataProviderNonBacked::GetURLAndTitle(
    FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  if ((formats_ & OSExchangeData::URL) == 0) {
    title->clear();
    return GetPlainTextURL(url) ||
           (policy == FilenameToURLPolicy::CONVERT_FILENAMES &&
            GetFileURL(url));
  }

  if (!url_.is_valid())
    return false;

  *url = url_;
  *title = title_;
  return true;
}

bool OSExchangeDataProviderNonBacked::GetFilename(base::FilePath* path) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  DCHECK(!filenames_.empty());
  *path = filenames_[0].path;
  return true;
}

bool OSExchangeDataProviderNonBacked::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  *filenames = filenames_;
  return true;
}

bool OSExchangeDataProviderNonBacked::GetPickledData(
    const ClipboardFormatType& format,
    base::Pickle* data) const {
  const auto i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderNonBacked::HasString() const {
  return (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderNonBacked::HasURL(FilenameToURLPolicy policy) const {
  if ((formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL(nullptr) ||
         (policy == FilenameToURLPolicy::CONVERT_FILENAMES &&
          GetFileURL(nullptr));
}

bool OSExchangeDataProviderNonBacked::HasFile() const {
  return (formats_ & OSExchangeData::FILE_NAME) != 0;
}

bool OSExchangeDataProviderNonBacked::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return base::Contains(pickle_data_, format);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
void OSExchangeDataProviderNonBacked::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {
  NOTIMPLEMENTED();
}
#endif

void OSExchangeDataProviderNonBacked::SetHtml(const base::string16& html,
                                              const GURL& base_url) {
  formats_ |= OSExchangeData::HTML;
  html_ = html;
  base_url_ = base_url;
}

bool OSExchangeDataProviderNonBacked::GetHtml(base::string16* html,
                                              GURL* base_url) const {
  if ((formats_ & OSExchangeData::HTML) == 0)
    return false;
  *html = html_;
  *base_url = base_url_;
  return true;
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
  base::FilePath file_path;
  if (!GetFilename(&file_path))
    return false;

  GURL test_url = net::FilePathToFileURL(file_path);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

bool OSExchangeDataProviderNonBacked::GetPlainTextURL(GURL* url) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;

  GURL test_url(string_);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

void OSExchangeDataProviderNonBacked::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {
  source_ = std::move(data_source);
}

DataTransferEndpoint* OSExchangeDataProviderNonBacked::GetSource() const {
  return source_.get();
}

}  // namespace ui
