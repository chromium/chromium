// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_
#define UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <objidl.h>
#endif

#include "base/files/file_path.h"
#include "base/macros.h"
#include "ui/base/dragdrop/download_file_interface.h"
#include "ui/base/ui_base_export.h"

class GURL;

namespace base {
class Pickle;
}

namespace gfx {
class ImageSkia;
class Vector2d;
}

namespace ui {

struct ClipboardFormatType;
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
class UI_BASE_EXPORT OSExchangeData {
 public:
  // Enumeration of the known formats.
  enum Format {
    STRING         = 1 << 0,
    URL            = 1 << 1,
    FILE_NAME      = 1 << 2,
    PICKLED_DATA   = 1 << 3,
#if defined(OS_WIN)
    FILE_CONTENTS  = 1 << 4,
#endif
#if defined(USE_AURA)
    HTML           = 1 << 5,
#endif
  };

  // Controls whether or not filenames should be converted to file: URLs when
  // getting a URL.
  enum FilenameToURLPolicy { CONVERT_FILENAMES, DO_NOT_CONVERT_FILENAMES, };

  // Encapsulates the info about a file to be downloaded.
  struct UI_BASE_EXPORT DownloadFileInfo {
    DownloadFileInfo(const base::FilePath& filename,
                     std::unique_ptr<DownloadFileProvider> downloader);
    ~DownloadFileInfo();

    base::FilePath filename;
    std::unique_ptr<DownloadFileProvider> downloader;
  };

  // Provider defines the platform specific part of OSExchangeData that
  // interacts with the native system.
  class UI_BASE_EXPORT Provider {
   public:
    Provider() {}
    virtual ~Provider() {}

    virtual std::unique_ptr<Provider> Clone() const = 0;

    virtual void MarkOriginatedFromRenderer() = 0;
    virtual bool DidOriginateFromRenderer() const = 0;

    virtual void SetString(const base::string16& data) = 0;
    virtual void SetURL(const GURL& url, const base::string16& title) = 0;
    virtual void SetFilename(const base::FilePath& path) = 0;
    virtual void SetFilenames(const std::vector<FileInfo>& file_names) = 0;
    virtual void SetPickledData(const ClipboardFormatType& format,
                                const base::Pickle& data) = 0;

    virtual bool GetString(base::string16* data) const = 0;
    virtual bool GetURLAndTitle(FilenameToURLPolicy policy,
                                GURL* url,
                                base::string16* title) const = 0;
    virtual bool GetFilename(base::FilePath* path) const = 0;
    virtual bool GetFilenames(std::vector<FileInfo>* file_names) const = 0;
    virtual bool GetPickledData(const ClipboardFormatType& format,
                                base::Pickle* data) const = 0;

    virtual bool HasString() const = 0;
    virtual bool HasURL(FilenameToURLPolicy policy) const = 0;
    virtual bool HasFile() const = 0;
    virtual bool HasCustomFormat(const ClipboardFormatType& format) const = 0;

#if defined(USE_X11) || defined(OS_WIN)
    virtual void SetFileContents(const base::FilePath& filename,
                                 const std::string& file_contents) = 0;
#endif
#if defined(OS_WIN)
    virtual bool GetFileContents(base::FilePath* filename,
                                 std::string* file_contents) const = 0;
    virtual bool HasFileContents() const = 0;
    virtual bool HasVirtualFilenames() const = 0;
    virtual bool GetVirtualFilenames(
        std::vector<FileInfo>* file_names) const = 0;
    virtual bool GetVirtualFilesAsTempFiles(
        base::OnceCallback<void(
            const std::vector<std::pair</*temp path*/ base::FilePath,
                                        /*display name*/ base::FilePath>>&)>
            callback) const = 0;
    virtual void SetVirtualFileContentsForTesting(
        const std::vector<std::pair<base::FilePath, std::string>>&
            filenames_and_contents,
        DWORD tymed) = 0;
    virtual void SetDownloadFileInfo(DownloadFileInfo* download) = 0;
#endif

#if defined(USE_AURA)
    virtual void SetHtml(const base::string16& html, const GURL& base_url) = 0;
    virtual bool GetHtml(base::string16* html, GURL* base_url) const = 0;
    virtual bool HasHtml() const = 0;
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
    virtual void SetDragImage(const gfx::ImageSkia& image,
                              const gfx::Vector2d& cursor_offset) = 0;
    virtual gfx::ImageSkia GetDragImage() const = 0;
    virtual gfx::Vector2d GetDragImageOffset() const = 0;
#endif
  };

  OSExchangeData();
  // Creates an OSExchangeData with the specified provider. OSExchangeData
  // takes ownership of the supplied provider.
  explicit OSExchangeData(std::unique_ptr<Provider> provider);

  ~OSExchangeData();

  // Returns the Provider, which actually stores and manages the data.
  const Provider& provider() const { return *provider_; }
  Provider& provider() { return *provider_; }

  // Marks drag data as tainted if it originates from the renderer. This is used
  // to avoid granting privileges to a renderer when dragging in tainted data,
  // since it could allow potential escalation of privileges.
  void MarkOriginatedFromRenderer();
  bool DidOriginateFromRenderer() const;

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
  void SetString(const base::string16& data);
  // A URL can have an optional title in some exchange formats.
  void SetURL(const GURL& url, const base::string16& title);
  // A full path to a file.
  void SetFilename(const base::FilePath& path);
  // Full path to one or more files. See also SetFilenames() in Provider.
  void SetFilenames(
      const std::vector<FileInfo>& file_names);
  // Adds pickled data of the specified format.
  void SetPickledData(const ClipboardFormatType& format,
                      const base::Pickle& data);

  // These functions retrieve data of the specified type. If data exists, the
  // functions return and the result is in the out parameter. If the data does
  // not exist, the out parameter is not touched. The out parameter cannot be
  // NULL.
  // GetString() returns the plain text representation of the pasteboard
  // contents.
  bool GetString(base::string16* data) const;
  bool GetURLAndTitle(FilenameToURLPolicy policy,
                      GURL* url,
                      base::string16* title) const;
  // Return the path of a file, if available.
  bool GetFilename(base::FilePath* path) const;
  bool GetFilenames(std::vector<FileInfo>* file_names) const;
  bool GetPickledData(const ClipboardFormatType& format,
                      base::Pickle* data) const;

  // Test whether or not data of certain types is present, without actually
  // returning anything.
  bool HasString() const;
  bool HasURL(FilenameToURLPolicy policy) const;
  bool HasFile() const;
  bool HasCustomFormat(const ClipboardFormatType& format) const;

  // Returns true if this OSExchangeData has data in any of the formats in
  // |formats| or any custom format in |custom_formats|.
  bool HasAnyFormat(int formats,
                    const std::set<ClipboardFormatType>& types) const;

#if defined(OS_WIN)
  // Adds the bytes of a file (CFSTR_FILECONTENTS and CFSTR_FILEDESCRIPTOR on
  // Windows).
  void SetFileContents(const base::FilePath& filename,
                       const std::string& file_contents);
  bool GetFileContents(base::FilePath* filename,
                       std::string* file_contents) const;

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
  bool GetVirtualFilenames(std::vector<FileInfo>* file_names) const;

  // Retrieves "virtual file" contents via creation of intermediary temp files.
  // Method is called on dropping on the Chromium drop target. Since creating
  // the temp files involves file I/O, the method is asynchronous and the caller
  // must provide a callback function that receives a vector of pairs of temp
  // file paths and display names. Method immediately returns false if there are
  // no virtual files in the data object, in which case the callback will never
  // be invoked.
  // TODO(https://crbug.com/951574): Implement virtual file extraction to
  // dynamically stream data to the renderer when File's bytes are actually
  // requested
  bool GetVirtualFilesAsTempFiles(
      base::OnceCallback<void(const std::vector</*temp path*/ std::pair<
                                  base::FilePath,
                                  /*display name*/ base::FilePath>>&)> callback)
      const;

  // Adds a download file with full path (CF_HDROP).
  void SetDownloadFileInfo(DownloadFileInfo* download);
#endif

#if defined(USE_AURA)
  // Adds a snippet of HTML.  |html| is just raw html but this sets both
  // text/html and CF_HTML.
  void SetHtml(const base::string16& html, const GURL& base_url);
  bool GetHtml(base::string16* html, GURL* base_url) const;
  bool HasHtml() const;
#endif

 private:
  // Provides the actual data.
  std::unique_ptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(OSExchangeData);
};

}  // namespace ui

#endif  // UI_BASE_DRAGDROP_OS_EXCHANGE_DATA_H_
