// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INFOLIST_ENTRY_H_
#define UI_BASE_IME_INFOLIST_ENTRY_H_

#include <string>

#include "base/component_export.h"

namespace ui {

// The data model of infolist window.
struct COMPONENT_EXPORT(UI_BASE_IME_TYPES) InfolistEntry {
  std::u16string title;
  std::u16string body;
  bool highlighted;

  InfolistEntry(const std::u16string& title, const std::u16string& body);
  bool operator==(const InfolistEntry& entry) const;
  bool operator!=(const InfolistEntry& entry) const;
};

}  // namespace ui

#endif  // UI_BASE_IME_INFOLIST_ENTRY_H_
