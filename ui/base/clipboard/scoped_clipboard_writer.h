// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_
#define UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

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
class COMPONENT_EXPORT(BASE_CLIPBOARD) ScopedClipboardWriter {
 public:
  // Create an instance that is a simple wrapper around the clipboard of the
  // given buffer.
  explicit ScopedClipboardWriter(ClipboardBuffer buffer);

  ~ScopedClipboardWriter();

  // Converts |text| to UTF-8 and adds it to the clipboard.
  void WriteText(const base::string16& text);

  // Adds HTML to the clipboard.  The url parameter is optional, but especially
  // useful if the HTML fragment contains relative links.
  void WriteHTML(const base::string16& markup, const std::string& source_url);

  // Adds RTF to the clipboard.
  void WriteRTF(const std::string& rtf_data);

  // Adds a bookmark to the clipboard.
  void WriteBookmark(const base::string16& bookmark_title,
                     const std::string& url);

  // Adds an html hyperlink (<a href>) to the clipboard. |anchor_text| and
  // |url| will be escaped as needed.
  void WriteHyperlink(const base::string16& anchor_text,
                      const std::string& url);

  // Used by WebKit to determine whether WebKit wrote the clipboard last
  void WriteWebSmartPaste();

  // Adds arbitrary pickled data to clipboard.
  void WritePickledData(const base::Pickle& pickle,
                        const ClipboardFormatType& format);

  // Data is written to the system clipboard in the same order as WriteData
  // calls are received.
  void WriteData(const base::string16& format, mojo_base::BigBuffer data);

  void WriteImage(const SkBitmap& bitmap);

  // Removes all objects that would be written to the clipboard.
  void Reset();

 private:
  // We accumulate the data passed to the various targets in the |objects_|
  // vector, and pass it to Clipboard::WritePortableRepresentations() during
  // object destruction.
  Clipboard::ObjectMap objects_;

  std::vector<Clipboard::PlatformRepresentation> platform_representations_;

  // The type is set at construction, and can be changed before committing.
  const ClipboardBuffer buffer_;

  SkBitmap bitmap_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriter);
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_SCOPED_CLIPBOARD_WRITER_H_
