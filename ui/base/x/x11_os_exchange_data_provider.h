// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_OS_EXCHANGE_DATA_PROVIDER_H_
#define UI_BASE_X_X11_OS_EXCHANGE_DATA_PROVIDER_H_

#include <stdint.h>
#include <map>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/x/selection_owner.h"
#include "ui/base/x/selection_requestor.h"
#include "ui/base/x/selection_utils.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace x11 {
class Connection;
}

namespace ui {

class OSExchangeDataProviderX11Test;

// Generic OSExchangeDataProvider implementation for X11.  Lacks the event
// handling; the subclass should listen for SelectionRequest X events and
// route them to the |selection_owner_|.
class COMPONENT_EXPORT(UI_BASE_X) XOSExchangeDataProvider
    : public OSExchangeDataProvider {
 public:
  // |x_window| is the window the cursor is over, |source_window| is the window
  // where the drag started, and |selection| is the set of data being offered.
  XOSExchangeDataProvider(x11::Window x_window,
                          x11::Window source_window,
                          const SelectionFormatMap& selection);

  // Creates a Provider for sending drag information. This creates its own,
  // hidden X11 window to own send data.
  XOSExchangeDataProvider();

  ~XOSExchangeDataProvider() override;

  XOSExchangeDataProvider(const XOSExchangeDataProvider&) = delete;
  XOSExchangeDataProvider& operator=(const XOSExchangeDataProvider&) = delete;

  // After all the Set* methods have built up the data we're offering, call
  // this to take ownership of the XdndSelection clipboard.
  void TakeOwnershipOfSelection() const;

  // Retrieves a list of types we're offering. Noop if we haven't taken the
  // selection.
  void RetrieveTargets(std::vector<x11::Atom>* targets) const;

  // Makes a copy of the format map currently being offered.
  SelectionFormatMap GetFormatMap() const;

  const base::FilePath& file_contents_name() const {
    return file_contents_name_;
  }

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
                      const base::Pickle& pickle) override;
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
  friend class OSExchangeDataProviderX11Test;
  using PickleData = std::map<ClipboardFormatType, base::Pickle>;

  bool own_window() const { return own_window_; }
  x11::Window x_window() const { return x_window_; }
  const SelectionFormatMap& format_map() const { return format_map_; }
  void set_format_map(const SelectionFormatMap& format_map) {
    format_map_ = format_map;
  }
  void set_file_contents_name(const base::FilePath& path) {
    file_contents_name_ = path;
  }
  SelectionOwner& selection_owner() const { return selection_owner_; }

  // Returns the targets in |format_map_|.
  std::vector<x11::Atom> GetTargets() const;

  // Inserts data into the format map.
  void InsertData(x11::Atom format,
                  const scoped_refptr<base::RefCountedMemory>& data);

 private:
  // Drag image and offset data.
  gfx::ImageSkia drag_image_;
  gfx::Vector2d drag_image_offset_;

  // Our X11 state.
  raw_ref<x11::Connection> connection_;
  x11::Window x_root_window_;

  // In X11, because the IPC parts of drag operations are implemented by
  // XSelection, we require an x11 window to receive drag messages on. The
  // OSExchangeDataProvider system is modeled on the Windows implementation,
  // which does not require a window. We only sometimes have a valid window
  // available (in the case of drag receiving). Other times, we need to create
  // our own xwindow just to receive events on it.
  const bool own_window_;

  x11::Window x_window_;
  x11::Window source_window_;

  // A representation of data. This is either passed to us from the other
  // process, or built up through a sequence of Set*() calls. It can be passed
  // to |selection_owner_| when we take the selection.
  SelectionFormatMap format_map_;

  // Auxiliary data for the X Direct Save protocol.
  base::FilePath file_contents_name_;

  // Takes a snapshot of |format_map_| and offers it to other windows.
  mutable SelectionOwner selection_owner_;

  bool is_from_privileged_ = false;
};

}  // namespace ui

#endif  // UI_BASE_X_X11_OS_EXCHANGE_DATA_PROVIDER_H_
