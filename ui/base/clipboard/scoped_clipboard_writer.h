// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_
#define UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace base {
class Pickle;
}

namespace ui {

// |ScopedClipboardWriter|:
// - is a wrapper for |Clipboard|.
// - simplifies writing data to the system clipboard.
// - handles packing data into a Clipboard::ObjectMap.
//
// Upon deletion, the class atomically writes all data to the clipboard,
// avoiding any potential race condition with other processes that are also
// writing to the system clipboard.
class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ScopedClipboardWriter {
 public:
  // Create an instance that is a simple wrapper around the clipboard of the
  // given buffer with an optional parameter indicating the source of the data.
  // TODO(crbug.com/40704495): change its references to use
  // DataTransferEndpoint, if possible.
  explicit ScopedClipboardWriter(
      ClipboardBuffer buffer,
      std::unique_ptr<DataTransferEndpoint> src = nullptr);
  ScopedClipboardWriter(const ScopedClipboardWriter&) = delete;
  ScopedClipboardWriter& operator=(const ScopedClipboardWriter&) = delete;
  ~ScopedClipboardWriter();

  // Sets the clipboard's source metadata.
  void SetDataSource(std::unique_ptr<DataTransferEndpoint> data_src);

  // Set the clipboards source URL.
  // Typically used for attributing content originally copied from a web page.
  void SetDataSourceURL(const GURL& main_frame, const GURL& frame);

  // Converts |text| to UTF-8 and adds it to the clipboard.
  void WriteText(const std::u16string& text);

  // Adds HTML to the clipboard. The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links.
  // The `content_type` refers to the sanitization of the markup.
  void WriteHTML(const std::u16string& markup, const std::string& source_url);

  // Adds SVG to the clipboard.
  void WriteSvg(const std::u16string& text);

  // Adds RTF to the clipboard.
  void WriteRTF(const std::string& rtf_data);

  // Adds text/uri-list filenames to the clipboard.
  // Security Note: This function is expected to be called only by exo in
  // Chrome OS. It should not be called by renderers or any other untrusted
  // party since any paths written to the clipboard can be read by renderers.
  void WriteFilenames(const std::string& uri_list);

  // Adds a bookmark to the clipboard.
  void WriteBookmark(const std::u16string& bookmark_title,
                     const std::string& url);

  // Adds an html hyperlink (<a href>) to the clipboard. |anchor_text| and
  // |url| will be escaped as needed.
  void WriteHyperlink(const std::u16string& anchor_text,
                      const std::string& url);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();

  // Adds arbitrary pickled data to clipboard.
  void WritePickledData(const base::Pickle& pickle,
                        const ClipboardFormatType& format);

  // Data is written to the system clipboard in the same order as WriteData
  // calls are received.
  // This is only used to write custom format data.
  void WriteData(const std::u16string& format, mojo_base::BigBuffer data);

  void WriteImage(const SkBitmap& bitmap);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Used by clipboard unit tests to write an encoded clipboard source DTE.
  void WriteEncodedDataTransferEndpointForTesting(const std::string& json);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Mark the data to be written as confidential.
  void MarkAsConfidential();

  // Data is copied from an incognito window.
  void MarkAsOffTheRecord();

  // Removes all objects that would be written to the clipboard.
  void Reset();

 private:
  // We accumulate the data passed to the various targets in the |objects_|
  // vector, and pass it to Clipboard::WritePortableRepresentations() during
  // object destruction.
  Clipboard::ObjectMap objects_;

  std::vector<Clipboard::PlatformRepresentation> platform_representations_;
  // Keeps track of the unique custom formats registered in the clipboard.
  base::flat_map<std::string, std::string> registered_formats_;
  int counter_ = 0;

  // The type is set at construction, and can be changed before committing.
  const ClipboardBuffer buffer_;

  // Contains the `Clipboard::PrivacyTypes` based on whether the content was
  // marked as confidential or off the record. e.g. password is considered as
  // confidential that should be concealed.
  uint32_t privacy_types_ = 0;

  // The source of the data written in ScopedClipboardWriter, nullptr means it's
  // not set, or the source of the data can't be represented by
  // DataTransferEndpoint.
  std::unique_ptr<DataTransferEndpoint> data_src_;

  // The URL of the mainframe the contents are from.
  GURL main_frame_url_;

  // The URL of the frame the contents are from.
  GURL frame_url_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_
