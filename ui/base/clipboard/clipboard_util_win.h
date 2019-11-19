// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_

#include <shlobj.h>
#include <stddef.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

class GURL;

namespace ui {

// Contains helper functions for working with the clipboard and IDataObjects.
class COMPONENT_EXPORT(BASE_CLIPBOARD) ClipboardUtil {
 public:
  /////////////////////////////////////////////////////////////////////////////
  // These methods check to see if |data_object| has the requested type.
  // Returns true if it does.
  static bool HasUrl(IDataObject* data_object, bool convert_filenames);
  static bool HasFilenames(IDataObject* data_object);
  static bool HasVirtualFilenames(IDataObject* data_object);
  static bool HasPlainText(IDataObject* data_object);
  static bool HasFileContents(IDataObject* data_object);
  static bool HasHtml(IDataObject* data_object);

  /////////////////////////////////////////////////////////////////////////////
  // Helper methods to extract information from an IDataObject.  These methods
  // return true if the requested data type is found in |data_object|.

  // Only returns true if url->is_valid() is true.
  static bool GetUrl(IDataObject* data_object,
                     GURL* url,
                     base::string16* title,
                     bool convert_filenames);
  // Only returns true if |*filenames| is not empty.
  static bool GetFilenames(IDataObject* data_object,
                           std::vector<base::string16>* filenames);

  // Fills a vector of display names of "virtual files" in the data store, but
  // does not actually retrieve the file contents. Display names are assured to
  // be unique. Method is called on drag enter of the Chromium drop target, when
  // only the display names are needed. Method only returns true if |filenames|
  // is not empty.
  static bool GetVirtualFilenames(IDataObject* data_object,
                                  std::vector<base::FilePath>* filenames);

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
  static bool GetVirtualFilesAsTempFiles(
      IDataObject* data_object,
      base::OnceCallback<
          void(const std::vector<std::pair</*temp path*/ base::FilePath,
                                           /*display name*/ base::FilePath>>&)>
          callback);

  static bool GetPlainText(IDataObject* data_object,
                           base::string16* plain_text);
  static bool GetHtml(IDataObject* data_object,
                      base::string16* text_html,
                      std::string* base_url);
  static bool GetFileContents(IDataObject* data_object,
                              base::string16* filename,
                              std::string* file_contents);
  // This represents custom MIME types a web page might set to transport its
  // own types of data for drag and drop. It is sandboxed in its own CLIPFORMAT
  // to avoid polluting the ::RegisterClipboardFormat() namespace with random
  // strings from web content.
  static bool GetWebCustomData(
      IDataObject* data_object,
      std::unordered_map<base::string16, base::string16>* custom_data);

  // Helper method for converting between MS CF_HTML format and plain
  // text/html.
  static std::string HtmlToCFHtml(const std::string& html,
                                  const std::string& base_url);
  static void CFHtmlToHtml(const std::string& cf_html,
                           std::string* html,
                           std::string* base_url);
  static void CFHtmlExtractMetadata(const std::string& cf_html,
                                    std::string* base_url,
                                    size_t* html_start,
                                    size_t* fragment_start,
                                    size_t* fragment_end);
};
}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_
