// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_

#include <shlobj.h>
#include <stddef.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "ui/base/clipboard/file_info.h"

class GURL;

namespace ui {

// Contains helper functions for working with the clipboard and IDataObjects.
namespace clipboard_util {

/////////////////////////////////////////////////////////////////////////////
// These methods check to see if |data_object| has the requested type.
// Returns true if it does.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool HasUrl(IDataObject* data_object, bool convert_filenames);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD) bool HasFilenames(IDataObject* data_object);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool HasVirtualFilenames(IDataObject* data_object);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD) bool HasPlainText(IDataObject* data_object);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool HasFileContents(IDataObject* data_object);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD) bool HasHtml(IDataObject* data_object);

/////////////////////////////////////////////////////////////////////////////
// Helper methods to extract information from an IDataObject.  These methods
// return true if the requested data type is found in |data_object|.

// Only returns true if url->is_valid() is true.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetUrl(IDataObject* data_object,
            GURL* url,
            std::u16string* title,
            bool convert_filenames);
// Only returns true if |*filenames| is not empty.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetFilenames(IDataObject* data_object,
                  std::vector<std::wstring>* filenames);

// Creates a new STGMEDIUM object to hold files.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
STGMEDIUM CreateStorageForFileNames(const std::vector<FileInfo>& filenames);

// Fills a vector of display names of "virtual files" in the data store, but
// does not actually retrieve the file contents. Display names are assured to be
// unique. Method is called on drag enter of the Chromium drop target, when only
// the display names are needed. If there are no display names, returns nullopt.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
std::optional<std::vector<base::FilePath>> GetVirtualFilenames(
    IDataObject* data_object);

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
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void GetVirtualFilesAsTempFiles(
    IDataObject* data_object,
    base::OnceCallback<
        void(const std::vector<std::pair</*temp path*/ base::FilePath,
                                         /*display name*/ base::FilePath>>&)>
        callback);

COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetPlainText(IDataObject* data_object, std::u16string* plain_text);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetHtml(IDataObject* data_object,
             std::u16string* text_html,
             std::string* base_url);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetFileContents(IDataObject* data_object,
                     std::wstring* filename,
                     std::string* file_contents);
// This represents custom MIME types a web page might set to transport its
// own types of data for drag and drop. It is sandboxed in its own CLIPFORMAT
// to avoid polluting the ::RegisterClipboardFormat() namespace with random
// strings from web content.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
bool GetDataTransferCustomData(
    IDataObject* data_object,
    std::unordered_map<std::u16string, std::u16string>* custom_data);

// Helper method for converting between MS CF_HTML format and plain
// text/html.
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
std::string HtmlToCFHtml(std::string_view html, std::string_view base_url);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void CFHtmlToHtml(std::string_view cf_html,
                  std::string* html,
                  std::string* base_url);
COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
void CFHtmlExtractMetadata(std::string_view cf_html,
                           std::string* base_url,
                           size_t* html_start,
                           size_t* fragment_start,
                           size_t* fragment_end);

}  // namespace clipboard_util

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_WIN_H_
