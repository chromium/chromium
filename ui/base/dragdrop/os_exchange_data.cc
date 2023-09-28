// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/os_exchange_data.h"

#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/dragdrop/os_exchange_data_provider_factory.h"
#include "url/origin.h"

namespace ui {

OSExchangeData::OSExchangeData()
    : provider_(OSExchangeDataProviderFactory::CreateProvider()) {
}

OSExchangeData::OSExchangeData(std::unique_ptr<OSExchangeDataProvider> provider)
    : provider_(std::move(provider)) {}

OSExchangeData::~OSExchangeData() {
}

void OSExchangeData::MarkRendererTaintedFromOrigin(const url::Origin& origin) {
  provider_->MarkRendererTaintedFromOrigin(origin);
}

bool OSExchangeData::IsRendererTainted() const {
  return provider_->IsRendererTainted();
}

absl::optional<url::Origin> OSExchangeData::GetRendererTaintedOrigin() const {
  return provider_->GetRendererTaintedOrigin();
}

void OSExchangeData::MarkAsFromPrivileged() {
  provider_->MarkAsFromPrivileged();
}

bool OSExchangeData::IsFromPrivileged() const {
  return provider_->IsFromPrivileged();
}

void OSExchangeData::SetString(const std::u16string& data) {
  provider_->SetString(data);
}

void OSExchangeData::SetURL(const GURL& url, const std::u16string& title) {
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

bool OSExchangeData::GetString(std::u16string* data) const {
  return provider_->GetString(data);
}

bool OSExchangeData::GetURLAndTitle(FilenameToURLPolicy policy,
                                    GURL* url,
                                    std::u16string* title) const {
  return provider_->GetURLAndTitle(policy, url, title);
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

bool OSExchangeData::HasFileContents() const {
  return provider_->HasFileContents();
}

bool OSExchangeData::HasCustomFormat(const ClipboardFormatType& format) const {
  return provider_->HasCustomFormat(format);
}

bool OSExchangeData::HasAnyFormat(
    int formats,
    const std::set<ClipboardFormatType>& format_types) const {
  if ((formats & STRING) != 0 && HasString())
    return true;
  if ((formats & URL) != 0 && HasURL(FilenameToURLPolicy::CONVERT_FILENAMES))
    return true;
  if ((formats & FILE_CONTENTS) != 0 && provider_->HasFileContents())
    return true;
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

void OSExchangeData::SetFileContents(const base::FilePath& filename,
                                     const std::string& file_contents) {
  provider_->SetFileContents(filename, file_contents);
}

bool OSExchangeData::GetFileContents(base::FilePath* filename,
                                     std::string* file_contents) const {
  return provider_->GetFileContents(filename, file_contents);
}

#if BUILDFLAG(IS_WIN)
bool OSExchangeData::HasVirtualFilenames() const {
  return provider_->HasVirtualFilenames();
}

bool OSExchangeData::GetVirtualFilenames(
    std::vector<FileInfo>* filenames) const {
  return provider_->GetVirtualFilenames(filenames);
}

void OSExchangeData::GetVirtualFilesAsTempFiles(
    base::OnceCallback<
        void(const std::vector<std::pair<base::FilePath, base::FilePath>>&)>
        callback) const {
  provider_->GetVirtualFilesAsTempFiles(std::move(callback));
}
#endif

#if defined(USE_AURA)
bool OSExchangeData::HasHtml() const {
  return provider_->HasHtml();
}

void OSExchangeData::SetHtml(const std::u16string& html, const GURL& base_url) {
  provider_->SetHtml(html, base_url);
}

bool OSExchangeData::GetHtml(std::u16string* html, GURL* base_url) const {
  return provider_->GetHtml(html, base_url);
}
#endif

void OSExchangeData::SetSource(
    std::unique_ptr<DataTransferEndpoint> data_source) {
  provider_->SetSource(std::move(data_source));
}

DataTransferEndpoint* OSExchangeData::GetSource() const {
  return provider_->GetSource();
}

}  // namespace ui
