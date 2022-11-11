// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URL_URL_IDNA_ICU_H_
#define URL_URL_IDNA_ICU_H_

#include "base/component_export.h"

namespace url {

// Closes the currently used global ICU IDNA instance and resets its pointer.
COMPONENT_EXPORT(URL) void ResetUIDNAForTesting();

}  // namespace url

#endif
