// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/public/browser.h"

namespace weblayer {

Browser::PersistenceInfo::PersistenceInfo() = default;

Browser::PersistenceInfo::PersistenceInfo(const PersistenceInfo& other) =
    default;

Browser::PersistenceInfo::~PersistenceInfo() = default;

}  // namespace weblayer
