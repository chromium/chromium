// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/download_file_info.h"
#include "ui/base/dragdrop/download_file_interface.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/callback_forward.h"
#endif

#if defined(USE_AURA) || BUILDFLAG(IS_APPLE)
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#endif

namespace ui {

class DataTransferEndpoint;

// Controls whether or not filenames should be converted to file: URLs when
// getting a URL.
enum class FilenameToURLPolicy {
  CONVERT_FILENAMES,
  DO_NOT_CONVERT_FILENAMES,
};

// Provider defines the platform specific part of OSExchangeData that
// interacts with the native system.
class COMPONENT_EXPORT(UI_BASE_DATA_EXCHANGE) OSExchangeDataProvider {
 public:
  OSExchangeDataProvider() = default;
  virtual ~OSExchangeDataProvider() = default;

  virtual std::unique_ptr<OSExchangeDataProvider> Clone() const = 0;

  virtual void MarkOriginatedFromRenderer() = 0;
  virtual bool DidOriginateFromRenderer() const = 0;

  virtual void MarkAsFromPrivileged() = 0;
  virtual bool IsFromPrivileged() const = 0;

  virtual void SetString(const std::u16string& data) = 0;
  virtual void SetURL(const GURL& url, const std::u16string& title) = 0;
  virtual void SetFilename(const base::FilePath& path) = 0;
  virtual void SetFilenames(const std::vector<FileInfo>& file_names) = 0;
  virtual void SetPickledData(const ClipboardFormatType& format,
                              const base::Pickle& data) = 0;

  virtual bool GetString(std::u16string* data) const = 0;
  virtual bool GetURLAndTitle(FilenameToURLPolicy policy,
                              GURL* url,
                              std::u16string* title) const = 0;
  virtual bool GetFilename(base::FilePath* path) const = 0;
  virtual bool GetFilenames(std::vector<FileInfo>* file_names) const = 0;
  virtual bool GetPickledData(const ClipboardFormatType& format,
                              base::Pickle* data) const = 0;

  virtual bool HasString() const = 0;
  virtual bool HasURL(FilenameToURLPolicy policy) const = 0;
  virtual bool HasFile() const = 0;
  virtual bool HasCustomFormat(const ClipboardFormatType& format) const = 0;

  virtual void SetFileContents(const base::FilePath& filename,
                               const std::string& file_contents) = 0;
  virtual bool GetFileContents(base::FilePath* filename,
                               std::string* file_contents) const = 0;
  virtual bool HasFileContents() const = 0;
#if BUILDFLAG(IS_WIN)
  virtual bool HasVirtualFilenames() const = 0;
  virtual bool GetVirtualFilenames(std::vector<FileInfo>* file_names) const = 0;
  virtual bool GetVirtualFilesAsTempFiles(
      base::OnceCallback<
          void(const std::vector<std::pair</*temp path*/ base::FilePath,
                                           /*display name*/ base::FilePath>>&)>
          callback) const = 0;
  virtual void SetVirtualFileContentsForTesting(
      const std::vector<std::pair<base::FilePath, std::string>>&
          filenames_and_contents,
      DWORD tymed) = 0;
  virtual void SetDownloadFileInfo(DownloadFileInfo* download) = 0;
#endif

#if defined(USE_AURA)
  virtual void SetHtml(const std::u16string& html, const GURL& base_url) = 0;
  virtual bool GetHtml(std::u16string* html, GURL* base_url) const = 0;
  virtual bool HasHtml() const = 0;
#endif

#if defined(USE_AURA) || BUILDFLAG(IS_APPLE)
  virtual void SetDragImage(const gfx::ImageSkia& image,
                            const gfx::Vector2d& cursor_offset) = 0;
  virtual gfx::ImageSkia GetDragImage() const = 0;
  virtual gfx::Vector2d GetDragImageOffset() const = 0;
#endif

  // These functions are only implemented on Chrome OS currently.
  virtual void SetSource(std::unique_ptr<DataTransferEndpoint> data_source) = 0;
  virtual DataTransferEndpoint* GetSource() const = 0;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_H_
