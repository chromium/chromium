// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data_provider_aura.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/file_info.h"

namespace ui {

OSExchangeDataProviderAura::OSExchangeDataProviderAura()
    : formats_(0) {
}

OSExchangeDataProviderAura::~OSExchangeDataProviderAura() = default;

std::unique_ptr<OSExchangeData::Provider>
OSExchangeDataProviderAura::Clone() const {
  OSExchangeDataProviderAura* ret = new OSExchangeDataProviderAura();
  ret->formats_ = formats_;
  ret->string_ = string_;
  ret->url_ = url_;
  ret->title_ = title_;
  ret->filenames_ = filenames_;
  ret->pickle_data_ = pickle_data_;
  // We skip copying the drag images.
  ret->html_ = html_;
  ret->base_url_ = base_url_;

  return base::WrapUnique<OSExchangeData::Provider>(ret);
}

void OSExchangeDataProviderAura::MarkOriginatedFromRenderer() {
  // TODO(dcheng): Currently unneeded because ChromeOS Aura correctly separates
  // URL and filename metadata, and does not implement the DownloadURL protocol.
}

bool OSExchangeDataProviderAura::DidOriginateFromRenderer() const {
  return false;
}

void OSExchangeDataProviderAura::SetString(const base::string16& data) {
  if (HasString())
    return;

  string_ = data;
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderAura::SetURL(const GURL& url,
                                        const base::string16& title) {
  url_ = url;
  title_ = title;
  formats_ |= OSExchangeData::URL;

  SetString(base::UTF8ToUTF16(url.spec()));
}

void OSExchangeDataProviderAura::SetFilename(const base::FilePath& path) {
  filenames_.clear();
  filenames_.push_back(FileInfo(path, base::FilePath()));
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderAura::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  filenames_ = filenames;
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderAura::SetPickledData(
    const ClipboardFormatType& format,
    const base::Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

bool OSExchangeDataProviderAura::GetString(base::string16* data) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;
  *data = string_;
  return true;
}

bool OSExchangeDataProviderAura::GetURLAndTitle(
    OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  if ((formats_ & OSExchangeData::URL) == 0) {
    title->clear();
    return GetPlainTextURL(url) ||
           (policy == OSExchangeData::CONVERT_FILENAMES && GetFileURL(url));
  }

  if (!url_.is_valid())
    return false;

  *url = url_;
  *title = title_;
  return true;
}

bool OSExchangeDataProviderAura::GetFilename(base::FilePath* path) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  DCHECK(!filenames_.empty());
  *path = filenames_[0].path;
  return true;
}

bool OSExchangeDataProviderAura::GetFilenames(
    std::vector<FileInfo>* filenames) const {
  if ((formats_ & OSExchangeData::FILE_NAME) == 0)
    return false;
  *filenames = filenames_;
  return true;
}

bool OSExchangeDataProviderAura::GetPickledData(
    const ClipboardFormatType& format,
    base::Pickle* data) const {
  PickleData::const_iterator i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderAura::HasString() const {
  return (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderAura::HasURL(
    OSExchangeData::FilenameToURLPolicy policy) const {
  if ((formats_ & OSExchangeData::URL) != 0) {
    return true;
  }
  // No URL, see if we have plain text that can be parsed as a URL.
  return GetPlainTextURL(NULL) ||
         (policy == OSExchangeData::CONVERT_FILENAMES && GetFileURL(nullptr));
}

bool OSExchangeDataProviderAura::HasFile() const {
  return (formats_ & OSExchangeData::FILE_NAME) != 0;
}

bool OSExchangeDataProviderAura::HasCustomFormat(
    const ClipboardFormatType& format) const {
  return pickle_data_.find(format) != pickle_data_.end();
}

void OSExchangeDataProviderAura::SetHtml(const base::string16& html,
                                         const GURL& base_url) {
  formats_ |= OSExchangeData::HTML;
  html_ = html;
  base_url_ = base_url;
}

bool OSExchangeDataProviderAura::GetHtml(base::string16* html,
                                         GURL* base_url) const {
  if ((formats_ & OSExchangeData::HTML) == 0)
    return false;
  *html = html_;
  *base_url = base_url_;
  return true;
}

bool OSExchangeDataProviderAura::HasHtml() const {
  return ((formats_ & OSExchangeData::HTML) != 0);
}

void OSExchangeDataProviderAura::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderAura::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderAura::GetDragImageOffset() const {
  return drag_image_offset_;
}

bool OSExchangeDataProviderAura::GetFileURL(GURL* url) const {
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

bool OSExchangeDataProviderAura::GetPlainTextURL(GURL* url) const {
  if ((formats_ & OSExchangeData::STRING) == 0)
    return false;

  GURL test_url(string_);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

}  // namespace ui
