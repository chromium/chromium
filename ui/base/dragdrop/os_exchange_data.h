// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"

class GURL;

namespace base {
class Pickle;
}

namespace url {
class Origin;
}

namespace ui {

class ClipboardFormatType;
class DataTransferEndpoint;
struct FileInfo;

///////////////////////////////////////////////////////////////////////////////
//
// OSExchangeData
//  An object that holds interchange data to be sent out to OS services like
//  clipboard, drag and drop, etc. This object exposes an API that clients can
//  use to specify raw data and its high level type. This object takes care of
//  translating that into something the OS can understand.
//
///////////////////////////////////////////////////////////////////////////////

// NOTE: Support for html and file contents is required by TabContentViewWin.
// TabContentsViewGtk uses a different class to handle drag support that does
// not use OSExchangeData. As such, file contents and html support is only
// compiled on windows.
class COMPONENT_EXPORT(UI_BASE) OSExchangeData {
 public:
  // Enumeration of the known formats.
  enum Format {
    STRING = 1 << 0,
    URL = 1 << 1,
    FILE_NAME = 1 << 2,
    PICKLED_DATA = 1 << 3,
    FILE_CONTENTS = 1 << 4,
#if defined(USE_AURA)
    HTML = 1 << 5,
#endif
#if BUILDFLAG(IS_CHROMEOS)
    DATA_TRANSFER_ENDPOINT = 1 << 6,
#endif
  };

  OSExchangeData();
  // Creates an OSExchangeData with the specified provider. OSExchangeData
  // takes ownership of the supplied provider.
  explicit OSExchangeData(std::unique_ptr<OSExchangeDataProvider> provider);

  OSExchangeData(const OSExchangeData&) = delete;
  OSExchangeData& operator=(const OSExchangeData&) = delete;

  ~OSExchangeData();

  // Returns the Provider, which actually stores and manages the data.
  const OSExchangeDataProvider& provider() const { return *provider_; }
  OSExchangeDataProvider& provider() { return *provider_; }

  // Marks drag data as tainted by the renderer, with `origin` as the source of
  // the data. This is used to:
  // - avoid granting privileges to a renderer when dragging in tainted data,
  //   since it could allow potential escalation of privileges.
  // - track the origin where the drag data came from.
  void MarkRendererTaintedFromOrigin(const url::Origin& origin);
  bool IsRendererTainted() const;
  std::optional<url::Origin> GetRendererTaintedOrigin() const;

  // Marks drag data as from privileged WebContents. This is used to
  // make sure non-privileged WebContents will not accept drop data from
  // privileged WebContents or vise versa.
  void MarkAsFromPrivileged();
  bool IsFromPrivileged() const;

  // These functions add data to the OSExchangeData object of various Chrome
  // types. The OSExchangeData object takes care of translating the data into
  // a format suitable for exchange with the OS.
  // NOTE WELL: Typically, a data object like this will contain only one of the
  //            following types of data. In cases where more data is held, the
  //            order in which these functions are called is _important_!
  //       ---> The order types are added to an OSExchangeData object controls
  //            the order of enumeration in our IEnumFORMATETC implementation!
  //            This comes into play when selecting the best (most preferable)
  //            data type for insertion into a DropTarget.
  void SetString(const std::u16string& data);
  // A URL can have an optional title in some exchange formats.
  void SetURL(const GURL& url, const std::u16string& title);
  // A full path to a file.
  void SetFilename(const base::FilePath& path);
  // Full path to one or more files. See also SetFilenames() in Provider.
  void SetFilenames(
      const std::vector<FileInfo>& file_names);
  // Adds pickled data of the specified format.
  void SetPickledData(const ClipboardFormatType& format,
                      const base::Pickle& data);

  // These functions retrieve data of the specified type. If the data is
  // present, it is returned, and if not, nullopt is returned.

  // GetString() returns the plain text representation of the pasteboard
  // contents.
  std::optional<std::u16string> GetString() const;
  using UrlInfo = OSExchangeDataProvider::UrlInfo;
  std::optional<UrlInfo> GetURLAndTitle(FilenameToURLPolicy policy) const;
  std::optional<std::vector<GURL>> GetURLs(FilenameToURLPolicy policy) const;
  // Return information about the contained files, if any.
  std::optional<std::vector<FileInfo>> GetFilenames() const;
  std::optional<base::Pickle> GetPickledData(
      const ClipboardFormatType& format) const;

  // Test whether or not data of certain types is present, without actually
  // returning anything.
  bool HasString() const;
  bool HasURL(FilenameToURLPolicy policy) const;
  bool HasFile() const;
  bool HasFileContents() const;
  bool HasCustomFormat(const ClipboardFormatType& format) const;

  // Returns true if this OSExchangeData has data in any of the formats in
  // |formats| or any custom format in |custom_formats|.
  bool HasAnyFormat(int formats,
                    const std::set<ClipboardFormatType>& types) const;

  // Adds the bytes of a file (CFSTR_FILECONTENTS and CFSTR_FILEDESCRIPTOR on
  // Windows).
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents);
  using FileContentsInfo = OSExchangeDataProvider::FileContentsInfo;
  std::optional<FileContentsInfo> GetFileContents() const;

#if BUILDFLAG(IS_WIN)
  // Methods used to query and retrieve file data from a drag source
  // IDataObject implementation packaging the data with the
  // CFSTR_FILEDESCRIPTOR/CFSTR_FILECONTENTS clipboard formats instead of the
  // more common CF_HDROP. These formats are intended to represent "virtual
  // files," not files that live on the platform file system. For a drop target
  // to read the file contents, it must be streamed from the drag source
  // application.

  // Method that returns true if there are virtual files packaged in the data
  // store.
  bool HasVirtualFilenames() const;

  // Retrieves names of any "virtual files" in the data store packaged using the
  // CFSTR_FILEDESCRIPTOR/CFSTR_FILECONTENTS clipboard formats instead of the
  // more common CF_HDROP used for "real files." Real files are preferred over
  // virtual files here to avoid duplication, as the data store may package
  // the same file lists using different formats. GetVirtualFilenames just
  // retrieves the display names but not the temp file paths. The temp files
  // are only created upon drop via a call to the async method
  // GetVirtualFilesAsTempFiles.
  std::optional<std::vector<FileInfo>> GetVirtualFilenames() const;

  // Retrieves "virtual file" contents via creation of intermediary temp files.
  // Method is called on dropping on the Chromium drop target. Since creating
  // the temp files involves file I/O, the method is asynchronous and the caller
  // must provide a callback function that receives a vector of pairs of temp
  // file paths and display names. The method will invoke the callback with an
  // empty vector if there are no virtual files in the data object.
  //
  // TODO(crbug.com/41452260): Implement virtual file extraction to
  // dynamically stream data to the renderer when File's bytes are actually
  // requested
  void GetVirtualFilesAsTempFiles(
      base::OnceCallback<void(const std::vector</*temp path*/ std::pair<
                                  base::FilePath,
                                  /*display name*/ base::FilePath>>&)> callback)
      const;
#endif

#if defined(USE_AURA)
  // Adds a snippet of HTML.  |html| is just raw html but this sets both
  // text/html and CF_HTML.
  void SetHtml(const std::u16string& html, const GURL& base_url);
  using HtmlInfo = OSExchangeDataProvider::HtmlInfo;
  std::optional<HtmlInfo> GetHtml() const;
  bool HasHtml() const;
#endif

  // Adds a DataTransferEndpoint to represent the source of the data.
  // TODO(crbug.com/40727723): Update all drag-and-drop references to set the
  // source of the data.
  void SetSource(std::unique_ptr<DataTransferEndpoint> data_source);
  DataTransferEndpoint* GetSource() const;

 private:
  // Provides the actual data.
  std::unique_ptr<OSExchangeDataProvider> provider_;
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_
