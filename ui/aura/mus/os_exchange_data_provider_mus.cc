// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/os_exchange_data_provider_mus.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/filename_util.h"
#include "ui/base/dragdrop/file_info.h"
#include "ui/base/mojo/clipboard.mojom.h"
#include "url/gurl.h"

namespace aura {

namespace {

std::vector<uint8_t> FromString(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

std::string ToString(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

base::string16 ToString16(const std::vector<uint8_t>& v) {
  DCHECK_EQ(0u, v.size() % 2);
  return base::string16(reinterpret_cast<const base::char16*>(v.data()),
                        v.size() / 2);
}

std::vector<base::StringPiece> ParseURIList(const std::vector<uint8_t>& data) {
  return base::SplitStringPiece(
      base::StringPiece(reinterpret_cast<const char*>(&data.front()),
                        data.size()),
      "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

void AddString16ToVector(const base::string16& str,
                         std::vector<uint8_t>* bytes) {
  const unsigned char* front = reinterpret_cast<const uint8_t*>(str.data());
  bytes->insert(bytes->end(), front, front + (str.size() * 2));
}

}  // namespace

OSExchangeDataProviderMus::OSExchangeDataProviderMus() {}

OSExchangeDataProviderMus::OSExchangeDataProviderMus(Data data)
    : mime_data_(std::move(data)) {}

OSExchangeDataProviderMus::~OSExchangeDataProviderMus() {}

OSExchangeDataProviderMus::Data OSExchangeDataProviderMus::GetData() const {
  return mime_data_;
}

std::unique_ptr<ui::OSExchangeData::Provider> OSExchangeDataProviderMus::Clone()
    const {
  std::unique_ptr<OSExchangeDataProviderMus> r =
      std::make_unique<OSExchangeDataProviderMus>();
  r->drag_image_ = drag_image_;
  r->drag_image_offset_ = drag_image_offset_;
  r->mime_data_ = mime_data_;
  return base::WrapUnique<ui::OSExchangeData::Provider>(r.release());
}

void OSExchangeDataProviderMus::MarkOriginatedFromRenderer() {
  // Currently unimplemented because ChromeOS doesn't need this.
  //
  // TODO(erg): Implement this when we start porting mus to other platforms.
}

bool OSExchangeDataProviderMus::DidOriginateFromRenderer() const {
  return false;
}

void OSExchangeDataProviderMus::SetString(const base::string16& data) {
  if (HasString())
    return;

  mime_data_[ui::Clipboard::kMimeTypeText] =
      FromString(base::UTF16ToUTF8(data));
}

void OSExchangeDataProviderMus::SetURL(const GURL& url,
                                       const base::string16& title) {
  base::string16 spec = base::UTF8ToUTF16(url.spec());
  std::vector<unsigned char> data;
  AddString16ToVector(spec, &data);
  AddString16ToVector(base::ASCIIToUTF16("\n"), &data);
  AddString16ToVector(title, &data);
  mime_data_[ui::Clipboard::kMimeTypeMozillaURL] = std::move(data);

  if (!base::ContainsKey(mime_data_, ui::Clipboard::kMimeTypeText))
    mime_data_[ui::Clipboard::kMimeTypeText] = FromString(url.spec());
}

void OSExchangeDataProviderMus::SetFilename(const base::FilePath& path) {
  std::vector<ui::FileInfo> data;
  data.push_back(ui::FileInfo(path, base::FilePath()));
  SetFilenames(data);
}

void OSExchangeDataProviderMus::SetFilenames(
    const std::vector<ui::FileInfo>& file_names) {
  std::vector<std::string> paths;
  for (auto it = file_names.begin(); it != file_names.end(); ++it) {
    std::string url_spec = net::FilePathToFileURL(it->path).spec();
    if (!url_spec.empty())
      paths.push_back(url_spec);
  }

  std::string joined_data = base::JoinString(paths, "\n");
  mime_data_[ui::Clipboard::kMimeTypeURIList] = FromString(joined_data);
}

void OSExchangeDataProviderMus::SetPickledData(
    const ui::Clipboard::FormatType& format,
    const base::Pickle& pickle) {
  const unsigned char* bytes =
      reinterpret_cast<const unsigned char*>(pickle.data());

  mime_data_[format.Serialize()] =
      std::vector<uint8_t>(bytes, bytes + pickle.size());
}

bool OSExchangeDataProviderMus::GetString(base::string16* data) const {
  auto it = mime_data_.find(ui::Clipboard::kMimeTypeText);
  if (it != mime_data_.end())
    *data = base::UTF8ToUTF16(ToString(it->second));
  return it != mime_data_.end();
}

bool OSExchangeDataProviderMus::GetURLAndTitle(
    ui::OSExchangeData::FilenameToURLPolicy policy,
    GURL* url,
    base::string16* title) const {
  auto it = mime_data_.find(ui::Clipboard::kMimeTypeMozillaURL);
  if (it == mime_data_.end()) {
    title->clear();
    return GetPlainTextURL(url) ||
           (policy == ui::OSExchangeData::CONVERT_FILENAMES && GetFileURL(url));
  }

  base::string16 data = ToString16(it->second);
  base::string16::size_type newline = data.find('\n');
  if (newline == std::string::npos)
    return false;

  GURL unparsed_url(data.substr(0, newline));
  if (!unparsed_url.is_valid())
    return false;

  *url = unparsed_url;
  *title = data.substr(newline + 1);
  return true;
}

bool OSExchangeDataProviderMus::GetFilename(base::FilePath* path) const {
  std::vector<ui::FileInfo> filenames;
  if (GetFilenames(&filenames)) {
    *path = filenames.front().path;
    return true;
  }

  return false;
}

bool OSExchangeDataProviderMus::GetFilenames(
    std::vector<ui::FileInfo>* file_names) const {
  auto it = mime_data_.find(ui::Clipboard::kMimeTypeURIList);
  if (it == mime_data_.end())
    return false;

  file_names->clear();
  for (const base::StringPiece& piece : ParseURIList(it->second)) {
    GURL url(piece);
    base::FilePath file_path;
    if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path))
      file_names->push_back(ui::FileInfo(file_path, base::FilePath()));
  }

  return true;
}

bool OSExchangeDataProviderMus::GetPickledData(
    const ui::Clipboard::FormatType& format,
    base::Pickle* data) const {
  auto it = mime_data_.find(format.Serialize());
  if (it == mime_data_.end())
    return false;

  // Note that the pickle object on the right hand side of the assignment
  // only refers to the bytes in |data|. The assignment copies the data.
  *data = base::Pickle(reinterpret_cast<const char*>(it->second.data()),
                       static_cast<int>(it->second.size()));
  return true;
}

bool OSExchangeDataProviderMus::HasString() const {
  return base::ContainsKey(mime_data_, ui::Clipboard::kMimeTypeText);
}

bool OSExchangeDataProviderMus::HasURL(
    ui::OSExchangeData::FilenameToURLPolicy policy) const {
  if (base::ContainsKey(mime_data_, ui::Clipboard::kMimeTypeMozillaURL))
    return true;

  auto it = mime_data_.find(ui::Clipboard::kMimeTypeURIList);
  if (it == mime_data_.end())
    return false;

  for (const base::StringPiece& piece : ParseURIList(it->second)) {
    if (!GURL(piece).SchemeIsFile() ||
        policy == ui::OSExchangeData::CONVERT_FILENAMES) {
      return true;
    }
  }

  return false;
}

bool OSExchangeDataProviderMus::HasFile() const {
  auto it = mime_data_.find(ui::Clipboard::kMimeTypeURIList);
  if (it == mime_data_.end())
    return false;

  for (const base::StringPiece& piece : ParseURIList(it->second)) {
    GURL url(piece);
    base::FilePath file_path;
    if (url.SchemeIsFile() && net::FileURLToFilePath(url, &file_path))
      return true;
  }

  return false;
}

bool OSExchangeDataProviderMus::HasCustomFormat(
    const ui::Clipboard::FormatType& format) const {
  return base::ContainsKey(mime_data_, format.Serialize());
}

// These methods were added in an ad-hoc way to different operating
// systems. We need to support them until they get cleaned up.
#if defined(USE_X11) || defined(OS_WIN)
void OSExchangeDataProviderMus::SetFileContents(
    const base::FilePath& filename,
    const std::string& file_contents) {}
#endif

#if defined(OS_WIN)
bool OSExchangeDataProviderMus::GetFileContents(
    base::FilePath* filename,
    std::string* file_contents) const {
  return false;
}

bool OSExchangeDataProviderMus::HasFileContents() const {
  return false;
}

void OSExchangeDataProviderMus::SetDownloadFileInfo(
    const ui::OSExchangeData::DownloadFileInfo& download) {}
#endif

#if defined(USE_AURA)
void OSExchangeDataProviderMus::SetHtml(const base::string16& html,
                                        const GURL& base_url) {
  std::vector<unsigned char> bytes;
  // Manually jam a UTF16 BOM into bytes because otherwise, other programs will
  // assume UTF-8.
  bytes.push_back(0xFF);
  bytes.push_back(0xFE);
  AddString16ToVector(html, &bytes);
  mime_data_[ui::Clipboard::kMimeTypeHTML] = bytes;
}

bool OSExchangeDataProviderMus::GetHtml(base::string16* html,
                                        GURL* base_url) const {
  auto it = mime_data_.find(ui::Clipboard::kMimeTypeHTML);
  if (it == mime_data_.end())
    return false;

  const unsigned char* data = it->second.data();
  size_t size = it->second.size();
  base::string16 markup;

  // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
  // UTF-16, otherwise assume UTF-8.
  if (size >= 2 && reinterpret_cast<const uint16_t*>(data)[0] == 0xFEFF) {
    markup.assign(reinterpret_cast<const base::char16*>(data) + 1,
                  (size / 2) - 1);
  } else {
    base::UTF8ToUTF16(reinterpret_cast<const char*>(data), size, &markup);
  }

  // If there is a terminating NULL, drop it.
  if (!markup.empty() && markup.at(markup.length() - 1) == '\0')
    markup.resize(markup.length() - 1);

  *html = markup;
  *base_url = GURL();
  return true;
}

bool OSExchangeDataProviderMus::HasHtml() const {
  return base::ContainsKey(mime_data_, ui::Clipboard::kMimeTypeHTML);
}
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
void OSExchangeDataProviderMus::SetDragImage(
    const gfx::ImageSkia& image,
    const gfx::Vector2d& cursor_offset) {
  drag_image_ = image;
  drag_image_offset_ = cursor_offset;
}

gfx::ImageSkia OSExchangeDataProviderMus::GetDragImage() const {
  return drag_image_;
}

gfx::Vector2d OSExchangeDataProviderMus::GetDragImageOffset() const {
  return drag_image_offset_;
}
#endif

bool OSExchangeDataProviderMus::GetFileURL(GURL* url) const {
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

bool OSExchangeDataProviderMus::GetPlainTextURL(GURL* url) const {
  base::string16 str;
  if (!GetString(&str))
    return false;

  GURL test_url(str);
  if (!test_url.is_valid())
    return false;

  if (url)
    *url = test_url;
  return true;
}

}  // namespace aura
