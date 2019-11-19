// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "url/gurl.h"

namespace ui {

OSExchangeData::DownloadFileInfo::DownloadFileInfo(
    const base::FilePath& filename,
    std::unique_ptr<DownloadFileProvider> downloader)
    : filename(filename), downloader(std::move(downloader)) {}

OSExchangeData::DownloadFileInfo::~DownloadFileInfo() = default;

OSExchangeData::OSExchangeData()
    : provider_(OSExchangeDataProviderFactory::CreateProvider()) {
}

OSExchangeData::OSExchangeData(std::unique_ptr<Provider> provider)
    : provider_(std::move(provider)) {
}

OSExchangeData::~OSExchangeData() {
}

void OSExchangeData::MarkOriginatedFromRenderer() {
  provider_->MarkOriginatedFromRenderer();
}

bool OSExchangeData::DidOriginateFromRenderer() const {
  return provider_->DidOriginateFromRenderer();
}

void OSExchangeData::SetString(const base::string16& data) {
  provider_->SetString(data);
}

void OSExchangeData::SetURL(const GURL& url, const base::string16& title) {
  provider_->SetURL(url, title);
}

void OSExchangeData::SetFilename(const base::FilePath& path) {
  provider_->SetFilename(path);
}

void OSExchangeData::SetFilenames(
    const std::vector<FileInfo>& filenames) {
  provider_->SetFilenames(filenames);
}

void OSExchangeData::SetPickledData(const ClipboardFormatType& format,
                                    const base::Pickle& data) {
  provider_->SetPickledData(format, data);
}

bool OSExchangeData::GetString(base::string16* data) const {
  return provider_->GetString(data);
}

bool OSExchangeData::GetURLAndTitle(FilenameToURLPolicy policy,
                                    GURL* url,
                                    base::string16* title) const {
  return provider_->GetURLAndTitle(policy, url, title);
}

bool OSExchangeData::GetFilename(base::FilePath* path) const {
  return provider_->GetFilename(path);
}

bool OSExchangeData::GetFilenames(std::vector<FileInfo>* filenames) const {
  return provider_->GetFilenames(filenames);
}

bool OSExchangeData::GetPickledData(const ClipboardFormatType& format,
                                    base::Pickle* data) const {
  return provider_->GetPickledData(format, data);
}

bool OSExchangeData::HasString() const {
  return provider_->HasString();
}

bool OSExchangeData::HasURL(FilenameToURLPolicy policy) const {
  return provider_->HasURL(policy);
}

bool OSExchangeData::HasFile() const {
  return provider_->HasFile();
}

bool OSExchangeData::HasCustomFormat(const ClipboardFormatType& format) const {
  return provider_->HasCustomFormat(format);
}

bool OSExchangeData::HasAnyFormat(
    int formats,
    const std::set<ClipboardFormatType>& format_types) const {
  if ((formats & STRING) != 0 && HasString())
    return true;
  if ((formats & URL) != 0 && HasURL(CONVERT_FILENAMES))
    return true;
#if defined(OS_WIN)
  if ((formats & FILE_CONTENTS) != 0 && provider_->HasFileContents())
    return true;
#endif
#if defined(USE_AURA)
  if ((formats & HTML) != 0 && provider_->HasHtml())
    return true;
#endif
  if ((formats & FILE_NAME) != 0 && provider_->HasFile())
    return true;
  for (const auto& format : format_types) {
    if (HasCustomFormat(format))
      return true;
  }
  return false;
}

#if defined(OS_WIN)
void OSExchangeData::SetFileContents(const base::FilePath& filename,
                                     const std::string& file_contents) {
  provider_->SetFileContents(filename, file_contents);
}

bool OSExchangeData::GetFileContents(base::FilePath* filename,
                                     std::string* file_contents) const {
  return provider_->GetFileContents(filename, file_contents);
}

bool OSExchangeData::HasVirtualFilenames() const {
  return provider_->HasVirtualFilenames();
}

bool OSExchangeData::GetVirtualFilenames(
    std::vector<FileInfo>* filenames) const {
  return provider_->GetVirtualFilenames(filenames);
}

bool OSExchangeData::GetVirtualFilesAsTempFiles(
    base::OnceCallback<
        void(const std::vector<std::pair<base::FilePath, base::FilePath>>&)>
        callback) const {
  return provider_->GetVirtualFilesAsTempFiles(std::move(callback));
}

void OSExchangeData::SetDownloadFileInfo(DownloadFileInfo* download) {
  provider_->SetDownloadFileInfo(download);
}
#endif

#if defined(USE_AURA)
bool OSExchangeData::HasHtml() const {
  return provider_->HasHtml();
}

void OSExchangeData::SetHtml(const base::string16& html, const GURL& base_url) {
  provider_->SetHtml(html, base_url);
}

bool OSExchangeData::GetHtml(base::string16* html, GURL* base_url) const {
  return provider_->GetHtml(html, base_url);
}
#endif

}  // namespace ui
