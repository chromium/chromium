// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"

namespace ui {

namespace {

// TODO(crbug.com/1236708): This duplicates logic in tab_strip_ui::IsDraggedTab.
// Check if it is really needed to extract app-specific types from pickled data
// and, if yes, factor it out to a common place and reuse it here instead.
void AddTabDragMimeTypes(const base::Pickle& pickle,
                         std::vector<std::string>* mime_types) {
  DCHECK(mime_types);
  base::PickleIterator iter(pickle);
  uint32_t entry_count = 0;
  if (iter.ReadUInt32(&entry_count)) {
    for (uint32_t i = 0; i < entry_count; ++i) {
      base::StringPiece16 type;
      base::StringPiece16 data;
      if (!iter.ReadStringPiece16(&type) || !iter.ReadStringPiece16(&data))
        break;
      const std::u16string kWebUITabIdDataType =
          u"application/vnd.chromium.tab";
      const std::u16string kWebUITabGroupIdDataType =
          u"application/vnd.chromium.tabgroup";
      if (type == kWebUITabIdDataType) {
        mime_types->push_back(base::UTF16ToASCII(kWebUITabIdDataType));
      } else if (type == kWebUITabGroupIdDataType) {
        mime_types->push_back(base::UTF16ToASCII(kWebUITabIdDataType));
      }
    }
  }
}

}  // namespace

WaylandExchangeDataProvider::WaylandExchangeDataProvider() = default;

WaylandExchangeDataProvider::~WaylandExchangeDataProvider() = default;

std::vector<std::string> WaylandExchangeDataProvider::BuildMimeTypesList()
    const {
  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland
  // specs don't say anything like that, but here we follow that common
  // practice: begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (HasFile())
    mime_types.push_back(ui::kMimeTypeURIList);

  if (HasURL(FilenameToURLPolicy::CONVERT_FILENAMES))
    mime_types.push_back(ui::kMimeTypeMozillaURL);

  if (HasHtml())
    mime_types.push_back(ui::kMimeTypeHTML);

  if (HasString()) {
    mime_types.push_back(ui::kMimeTypeTextUtf8);
    mime_types.push_back(ui::kMimeTypeText);
  }
  if (HasFileContents()) {
    base::FilePath file_contents_filename;
    std::string file_contents;
    GetFileContents(&file_contents_filename, &file_contents);

    std::string filename = file_contents_filename.value();
    base::ReplaceChars(filename, "\\", "\\\\", &filename);
    base::ReplaceChars(filename, "\"", "\\\"", &filename);
    const std::string mime_type =
        base::StrCat({ui::kMimeTypeOctetStream, ";name=\"", filename, "\""});
    mime_types.push_back(mime_type);
  }

  for (auto item : pickle_data()) {
    if (item.first == ClipboardFormatType::WebCustomDataType()) {
      AddTabDragMimeTypes(item.second, &mime_types);
      continue;
    }
    mime_types.push_back(item.first.GetName());
  }

  return mime_types;
}

}  // namespace ui
