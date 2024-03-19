// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_NON_BACKED_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_NON_BACKED_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "base/pickle.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace base {
class FilePath;
}  // namespace base

namespace ui {

class ClipboardFormatType;

// Simple OSExchangeDataProvider implementation for aura-based ports with no
// actual platform integration. So data managed by this class is exchangeable
// only among Chromium windows and is available only while it is alive.
class COMPONENT_EXPORT(UI_BASE) OSExchangeDataProviderNonBacked
    : public OSExchangeDataProvider {
 public:
  OSExchangeDataProviderNonBacked();
  OSExchangeDataProviderNonBacked(const OSExchangeDataProviderNonBacked&) =
      delete;
  OSExchangeDataProviderNonBacked& operator=(
      const OSExchangeDataProviderNonBacked&) = delete;
  ~OSExchangeDataProviderNonBacked() override;

  // Overridden from OSExchangeDataProvider:
  std::unique_ptr<OSExchangeDataProvider> Clone() const override;
  void MarkRendererTaintedFromOrigin(const url::Origin& origin) override;
  bool IsRendererTainted() const override;
  std::optional<url::Origin> GetRendererTaintedOrigin() const override;
  void MarkAsFromPrivileged() override;
  bool IsFromPrivileged() const override;
  void SetString(const std::u16string& data) override;
  void SetURL(const GURL& url, const std::u16string& title) override;
  void SetFilename(const base::FilePath& path) override;
  void SetFilenames(const std::vector<FileInfo>& filenames) override;
  void SetPickledData(const ClipboardFormatType& format,
                      const base::Pickle& data) override;
  std::optional<std::u16string> GetString() const override;
  std::optional<UrlInfo> GetURLAndTitle(
      FilenameToURLPolicy policy) const override;
  std::optional<std::vector<GURL>> GetURLs(
      FilenameToURLPolicy policy) const override;
  std::optional<std::vector<FileInfo>> GetFilenames() const override;
  std::optional<base::Pickle> GetPickledData(
      const ClipboardFormatType& format) const override;
  bool HasString() const override;
  bool HasURL(FilenameToURLPolicy policy) const override;
  bool HasFile() const override;
  bool HasCustomFormat(const ClipboardFormatType& format) const override;
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents) override;
  std::optional<FileContentsInfo> GetFileContents() const override;
  bool HasFileContents() const override;

  void SetHtml(const std::u16string& html, const GURL& base_url) override;
  std::optional<HtmlInfo> GetHtml() const override;
  bool HasHtml() const override;
  void SetDragImage(const gfx::ImageSkia& image,
                    const gfx::Vector2d& cursor_offset) override;
  gfx::ImageSkia GetDragImage() const override;
  gfx::Vector2d GetDragImageOffset() const override;

  void SetSource(std::unique_ptr<DataTransferEndpoint> data_source) override;
  DataTransferEndpoint* GetSource() const override;

 protected:
  // Copy internal data into |provider| object.
  void CopyData(OSExchangeDataProviderNonBacked* provider) const;

  const std::map<ClipboardFormatType, base::Pickle>& pickle_data() const {
    return pickle_data_;
  }

 private:
  // Returns true if |formats_| contains a file format and the file name can be
  // parsed as a URL.
  bool GetFileURL(GURL* url) const;

  // Returns true if |formats_| contains a string format and the string can be
  // parsed as a URL.
  bool GetPlainTextURL(GURL* url) const;

  // Actual formats that have been set.
  // for details.
  int formats_ = 0;

  // String contents.
  std::u16string string_;

  // URL contents.
  GURL url_;
  std::u16string title_;

  // File name.
  std::vector<FileInfo> filenames_;

  // PICKLED_DATA contents.
  std::map<ClipboardFormatType, base::Pickle> pickle_data_;

  // Drag image and offset data.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d drag_image_offset_;

  // For file contents.
  base::FilePath file_contents_filename_;
  std::string file_contents_;

  // For HTML format
  std::u16string html_;
  GURL base_url_;

  // For marking data originating from the renderer.
  std::optional<url::Origin> tainted_by_renderer_origin_;

  // For marking data originating by privileged WebContents.
  bool is_from_privileged_ = false;

  // Data source.
  std::unique_ptr<DataTransferEndpoint> source_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_PROVIDER_NON_BACKED_H_
