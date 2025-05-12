// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/infolist_entry.h"

namespace ui {

InfolistEntry::InfolistEntry(const std::u16string& title,
                             const std::u16string& body)
    : title(title), body(body), highlighted(false) {}

}  // namespace ui
